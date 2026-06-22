#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <pdulib.h>
#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>
#include <HTTPClient.h>
#include <mbedtls/md.h>  // 用于钉钉签名的HMAC-SHA256
#include <base64.h>      // Base64编码
#include "ping/ping_sock.h"

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
DNSServer dnsServer;

bool configValid = false;  // 配置是否有效
bool timeSynced = false;   // NTP时间是否已同步
String configInvalidReason = "";
bool pushDebugEnabled = false;
String pushDebugLog = "";
bool wifiConfigPortalActive = false;  // 是否处于配网门户模式

#define PUSH_DEBUG_LOG_MAX 6000
bool showWiFiDiagnostics = true;
bool pingGatewayInWiFiDiagnostics = true;
#define SERIAL_BUFFER_SIZE 500
#define MAX_PDU_LENGTH 300
char serialBuf[SERIAL_BUFFER_SIZE];
int serialBufLen = 0;

ConcatSms concatBuffer[MAX_CONCAT_MESSAGES];  // 长短信缓存
TaskHandle_t appLoopTaskHandle = nullptr;
unsigned long loopPerfWindowStartMs = 0;
unsigned long loopPerfLastTickMs = 0;
unsigned long loopPerfIterations = 0;
unsigned long loopPerfMaxGapMs = 0;
unsigned long perfAutoReportAtMs = 0;

#include "logger.h"
#include "config_core.h"
#include "diagnostics_utils.h"
#include "led_indicator.h"
#include "web_pages.h"

#include "web_config_handlers.h"

bool ensureWiFiGatewayReachable(const String& source, bool reconnectOnFailure);
void handleWiFiNetworkFailure(const String& source);
void wifiReachabilityWatchdog();
void forceWiFiFullReset(const char* reason);
void setupWiFiEventLogging();
void printSerialConsolePerf();

#include "modem_at.h"

#include "at_command.h"

#include "web_tool_handlers.h"

#include "modem_sms_control.h"

#include "sms_concat.h"

#include "push_channels.h"

#include "keep_alive.h"

#include "net_task.h"

#include "sms_receive.h"

#include "setup_helpers.h"

#include "wifi_provision.h"

#include "serial_console.h"

void setup() {
  //  指示灯
  ledInit();
  ledSetState(LED_BOOTING);

  // USB 串口日志
  Serial.begin(115200);
  delay(1500);  // 等 USB CDC 稳定
  initSystemLog();
  systemLog(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "boot start");
  appLoopTaskHandle = xTaskGetCurrentTaskHandle();
  loopPerfWindowStartMs = millis();
  loopPerfLastTickMs = loopPerfWindowStartMs;
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM,
                      "USB serial console ready; input HELP for commands");
  setupWiFiEventLogging();

  // 模组串口（UART）由 modem_at 单一管理器独占。
  modemAtBegin();

  // 模组从“干净状态”启动（EN 断电重启 + 清串口噪声）
  modemPowerCycle();

  // 初始化长短信缓存
  initConcatBuffer();
  modemAtSetUrcCallback(handleModemUrc, nullptr);

  // 加载配置
  loadConfig();

  // ========== 先初始化模组 ==========
  ledSetState(LED_MODEM_INIT);
  while (!sendATandWaitOK("AT", 1000)) {
    systemLogSerialOnly(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "AT not responding during boot init, retrying");
    blink_short();
  }
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "modem AT ready");

  //先设置CGACT，禁用数据连接
  while (!sendATandWaitOK("AT+CGACT=0,1", 5000)) {
    systemLogSerialOnly(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "AT+CGACT=0,1 failed during boot init, retrying");
    blink_short();
  }
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "CGACT disabled to avoid data usage");

  // 选择模块内部短信存储，避免占用SIM卡短信容量
  while (!sendATandWaitOK("AT+CPMS=\"" SMS_STORAGE "\",\"" SMS_STORAGE "\",\"" SMS_STORAGE "\"", 3000)) {
    systemLogSerialOnly(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "AT+CPMS failed during boot init, retrying");
    blink_short();
  }
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "CPMS storage set to " SMS_STORAGE);

  //设置短信自动上报
  while (!sendATandWaitOK("AT+CNMI=2,2,0,0,0", 1000)) {
    systemLogSerialOnly(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "AT+CNMI failed during boot init, retrying");
    blink_short();
  }
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "CNMI configured");

  //配置PDU模式
  while (!sendATandWaitOK("AT+CMGF=0", 1000)) {
    systemLogSerialOnly(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "AT+CMGF=0 failed during boot init, retrying");
    blink_short();
  }
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "PDU mode configured");

  //等待网络注册（LTE/4G）
  ledSetState(LED_WAIT_CELLULAR);
  while (!waitCEREG()) {
    systemLogSerialOnly(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "waiting for cellular registration");
    blink_short();
  }
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "cellular network registered");
  // ========== 模组初始化完成 ==========

  ledSetState(LED_WIFI_CONNECTING);
  bool wifiConnected = connectWiFiOrStartConfigPortal();
  if (!wifiConfigPortalActive) {
    printConfigValidationResult("上电加载");
  }

  // NTP时间同步（获取UTC时间）
  if (wifiConnected) {
    systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "syncing NTP time");
    configTime(0, 0, "ntp.ntsc.ac.cn", "ntp.aliyun.com", "pool.ntp.org");
    int ntpRetry = 0;
    while (time(nullptr) < 100000 && ntpRetry < 100) {
      delay(100);
      ntpRetry++;
    }
    if (time(nullptr) >= 100000) {
      timeSynced = true;
      time_t now = time(nullptr);
      systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "NTP sync ok utc=" + String((unsigned long)now));
    } else {
      systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SYSTEM, "NTP sync failed, using device time");
    }
  } else if (wifiConfigPortalActive) {
    // 配网模式下不做对时。
  } else {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SYSTEM, "skip NTP sync: WiFi not connected");
  }

  // 启动HTTP服务器
  server.on("/", handleRootOrWiFiConfig);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/tools", handleToolsPage);
  server.on("/wifi", handleWiFiConfigPage);
  server.on("/wifiscan", handleWiFiScan);
  server.on("/wifisave", HTTP_POST, handleWiFiConfigSave);
  server.on("/wificlear", HTTP_POST, handleWiFiConfigClear);
  server.on("/generate_204", handleCaptivePortalRedirect);
  server.on("/gen_204", handleCaptivePortalRedirect);
  server.on("/hotspot-detect.html", handleCaptivePortalRedirect);
  server.on("/connecttest.txt", handleCaptivePortalRedirect);
  server.on("/ncsi.txt", handleCaptivePortalRedirect);
  server.on("/fwlink", handleCaptivePortalRedirect);
  server.on("/success.txt", handleCaptivePortalRedirect);
  server.on("/canonical.html", handleCaptivePortalRedirect);
  server.on("/sms", handleToolsPage);  // 兼容旧链接
  server.on("/sendsms", HTTP_POST, handleSendSms);
  server.on("/smslist", handleSmsList);
  server.on("/smspdu", handleSmsPdu);
  server.on("/pushdebug", handlePushDebug);
  server.on("/testpush", HTTP_POST, handleTestPush);
  server.on("/pushfiltertest", HTTP_POST, handlePushFilterTest);
  server.on("/ping", HTTP_POST, handlePing);
  server.on("/query", handleQuery);
  server.on("/flight", handleFlightMode);
  server.on("/at", handleATCommand);
  server.onNotFound(handleCaptivePortalNotFound);
  server.begin();
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "http server started");

  ssl_client.setInsecure();

  // 根据最终状态确定正常运行常态：配网中 > 配置无效 > 正常待机
  if (wifiConfigPortalActive) {
    ledSetNormalState(LED_WIFI_PORTAL);
  } else if (!configValid) {
    ledSetNormalState(LED_ERROR);
  } else {
    ledSetNormalState(LED_RUNNING_IDLE);
  }

  // 如果配置有效，发送启动邮件通知
  if (wifiConnected && configValid && config.startupMailEnabled) {
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "config valid, sending startup email");
    String subject = "短信转发器已启动";
    String body = "设备已启动\n设备地址: " + getDeviceUrl();
    sendEmailNotification(subject.c_str(), body.c_str());
  } else if (wifiConnected && configValid && !config.startupMailEnabled) {
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "startup notification disabled, skip startup email");
  } else if (wifiConfigPortalActive) {
    // 配网模式下不发启动邮件。
  } else if (!wifiConnected) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SYSTEM, "skip startup email: WiFi not connected");
  }

  // 启动网络后台任务：异步处理推送、邮件和掉线检测，避免阻塞主循环
  if (wifiConfigPortalActive) {
    // 配网模式下不启动网络后台任务。
  } else {
    startNetTask();
  }
}

void loop() {
  modemAtPoll();

  unsigned long loopNow = millis();
  unsigned long loopGap = loopNow - loopPerfLastTickMs;
  loopPerfLastTickMs = loopNow;
  loopPerfIterations++;
  if (loopGap > loopPerfMaxGapMs) {
    loopPerfMaxGapMs = loopGap;
  }

  // 处理配网门户DNS劫持
  processWiFiConfigPortalDns();

  // WiFi 重连看门狗，掉线后主动恢复
  wifiReconnectWatchdog();

  // WiFi 可达性看门狗：处理 WL_CONNECTED 但网关不通的“假在线”
  wifiReachabilityWatchdog();

  // WiFi 漫游看门狗：弱信号时切换到同SSID更强的AP
  wifiRoamWatchdog();

  // 处理HTTP请求
  server.handleClient();

  // 检查长短信超时
  checkConcatTimeout();
  pollStoredSmsQueue();
  if (!wifiConfigPortalActive) {
    pollSmsReceiveWatchdog();
  }

  if (perfAutoReportAtMs != 0 && (long)(millis() - perfAutoReportAtMs) >= 0) {
    systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_SERIAL, "performance auto report");
    printSerialConsolePerf();
    perfAutoReportAtMs = 0;
  }

  // USB 串口命令台：本机诊断命令由 ESP32 处理，AT... 按整行转发给模组
  handleUsbSerialConsole();

  // 检查URC和解析由 modem_at 的 URC 回调处理。
  modemAtPoll();

  // 刷新 LED 状态节奏
  ledTick();
}
