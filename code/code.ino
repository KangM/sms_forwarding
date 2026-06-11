#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Preferences.h>
#include <pdulib.h>
#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>
#include <HTTPClient.h>
#include <mbedtls/md.h>  // 用于钉钉签名的HMAC-SHA256
#include <base64.h>      // Base64编码
#include "ping/ping_sock.h"

//wifi信息，需要你打开这个去改
#include "wifi_config.h"
#include "pins.h"
#include "config_types.h"
#include "sms_types.h"

Config config;
Preferences preferences;
WiFiMulti WiFiMulti;
PDU pdu = PDU(4096);
WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);
WebServer server(80);

bool configValid = false;  // 配置是否有效
bool timeSynced = false;   // NTP时间是否已同步
String configInvalidReason = "";
bool pushDebugEnabled = false;
String pushDebugLog = "";

#define PUSH_DEBUG_LOG_MAX 6000
bool showWiFiDiagnostics = true;
bool pingGatewayInWiFiDiagnostics = true;
#define SERIAL_BUFFER_SIZE 500
#define MAX_PDU_LENGTH 300
char serialBuf[SERIAL_BUFFER_SIZE];
int serialBufLen = 0;

ConcatSms concatBuffer[MAX_CONCAT_MESSAGES];  // 长短信缓存

#include "config_core.h"
#include "diagnostics_utils.h"
#include "web_pages.h"

#include "web_config_handlers.h"

#include "at_command.h"

#include "web_tool_handlers.h"

#include "modem_sms_control.h"

#include "sms_concat.h"

#include "push_channels.h"

#include "sms_receive.h"

#include "setup_helpers.h"

void setup() {
  //  指示灯
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // USB 串口日志
  Serial.begin(115200);
  delay(1500);  // 等 USB CDC 稳定

  // 模组串口（UART）
  Serial1.begin(115200, SERIAL_8N1, RXD, TXD);
  Serial1.setRxBufferSize(SERIAL_BUFFER_SIZE);

  // 模组从“干净状态”启动（EN 断电重启 + 清串口噪声）
  while (Serial1.available()) Serial1.read();
  modemPowerCycle();
  while (Serial1.available()) Serial1.read();

  // 初始化长短信缓存
  initConcatBuffer();

  // 加载配置
  loadConfig();
  printConfigValidationResult("上电加载");

  // ========== 先初始化模组 ==========
  while (!sendATandWaitOK("AT", 1000)) {
    Serial.println("AT未响应，重试...");
    blink_short();
  }
  Serial.println("模组AT响应正常");

  //先设置CGACT，禁用数据连接
  while (!sendATandWaitOK("AT+CGACT=0,1", 5000)) {
    Serial.println("设置CGACT失败，重试...");
    blink_short();
  }
  Serial.println("已禁用数据连接(AT+CGACT=0,1)，防止流量消耗");

  // 选择模块内部短信存储，避免占用SIM卡短信容量
  while (!sendATandWaitOK("AT+CPMS=\"" SMS_STORAGE "\",\"" SMS_STORAGE "\",\"" SMS_STORAGE "\"", 3000)) {
    Serial.println("设置CPMS失败，重试...");
    blink_short();
  }
  Serial.println("CPMS短信存储设置完成，当前存储: " SMS_STORAGE);

  //设置短信自动上报
  while (!sendATandWaitOK("AT+CNMI=2,1,0,0,0", 1000)) {
    Serial.println("设置CNMI失败，重试...");
    blink_short();
  }
  Serial.println("CNMI参数设置完成");

  //配置PDU模式
  while (!sendATandWaitOK("AT+CMGF=0", 1000)) {
    Serial.println("设置PDU模式失败，重试...");
    blink_short();
  }
  Serial.println("PDU模式设置完成");

  //等待网络注册（LTE/4G）
  while (!waitCEREG()) {
    Serial.println("等待网络注册...");
    blink_short();
  }
  Serial.println("网络已注册");
  // ========== 模组初始化完成 ==========

  // 扫描所有信道以连接信号最强的 AP，防止在 mesh 组网这类场景中连接到弱 AP
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  // 连接WiFi（支持隐藏SSID）
  // 参数: ssid, password, channel(0=自动), bssid(nullptr=自动), connect(true=连接隐藏网络)
  WiFi.begin(WIFI_SSID, WIFI_PASS, 0, nullptr, true);
  Serial.println("连接wifi");
  Serial.println(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) blink_short();
  Serial.println("wifi已连接");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());

  // NTP时间同步（获取UTC时间）
  Serial.println("正在同步NTP时间...");
  configTime(0, 0, "ntp.ntsc.ac.cn", "ntp.aliyun.com", "pool.ntp.org");
  int ntpRetry = 0;
  while (time(nullptr) < 100000 && ntpRetry < 100) {
    delay(100);
    ntpRetry++;
  }
  if (time(nullptr) >= 100000) {
    timeSynced = true;
    Serial.println("NTP时间同步成功");
    time_t now = time(nullptr);
    Serial.print("当前UTC时间戳: ");
    Serial.println(now);
  } else {
    Serial.println("NTP时间同步失败，将使用设备时间");
  }

  // 启动HTTP服务器
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/tools", handleToolsPage);
  server.on("/sms", handleToolsPage);  // 兼容旧链接
  server.on("/sendsms", HTTP_POST, handleSendSms);
  server.on("/smslist", handleSmsList);
  server.on("/pushdebug", handlePushDebug);
  server.on("/testpush", HTTP_POST, handleTestPush);
  server.on("/ping", HTTP_POST, handlePing);
  server.on("/query", handleQuery);
  server.on("/flight", handleFlightMode);
  server.on("/at", handleATCommand);
  server.begin();
  Serial.println("HTTP服务器已启动");

  ssl_client.setInsecure();
  digitalWrite(LED_BUILTIN, LOW);

  // 如果配置有效，发送启动邮件通知
  if (configValid) {
    Serial.println("配置有效，发送启动邮件通知...");
    String subject = "短信转发器已启动";
    String body = "设备已启动\n设备地址: " + getDeviceUrl();
    sendEmailNotification(subject.c_str(), body.c_str());
  }
}

void loop() {
  // 处理HTTP请求
  server.handleClient();

  // 检查长短信超时
  checkConcatTimeout();

  // 本地透传
  if (Serial.available()) Serial1.write(Serial.read());
  // 检查URC和解析
  checkSerial1URC();
}
