#pragma once

// 掉线检测功能：按固定间隔通过WiFi向指定URL发起HTTP请求。
// 该函数由网络后台任务(netTask)调用，可安全阻塞，不影响主循环。

// 上次发起持续请求的时间戳（millis）
unsigned long lastKeepAliveMillis = 0;
// 是否还未触发过首次请求（启动后先触发一次，不等间隔）
bool keepAliveFirstRun = true;
// KeepAlive 自动深度诊断：默认开启，连续失败期间只打印一次，恢复成功后重置。
bool keepAliveAutoDiagEnabled = true;
bool keepAliveFailureDiagArmed = true;
// 故障期加速：失败后临时加快复查，恢复成功后回到用户配置周期。
bool keepAliveFastRetryActive = false;
const unsigned long KEEP_ALIVE_FAILURE_RETRY_MS = 60000UL;

// 执行一次持续请求，返回是否成功（HTTP响应码 > 0 视为网络可达）
bool doKeepAliveRequest() {
  if (config.keepAliveUrl.length() == 0) {
    appendPushDebugLog("持续请求跳过：URL为空");
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_KEEPALIVE, "skip request: url empty");
    return false;
  }

  unsigned long t0 = millis();
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  http.begin(config.keepAliveUrl);

  int httpCode = 0;
  if (config.keepAliveMethod == "POST") {
    http.addHeader("Content-Type", "application/json");
    systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_KEEPALIVE, "request start POST " + config.keepAliveUrl);
    appendPushDebugLog("持续请求 POST " + config.keepAliveUrl);
    if (config.keepAliveBody.length() > 0) {
      appendPushDebugLog("持续请求 BODY: " + config.keepAliveBody);
    }
    httpCode = http.POST(config.keepAliveBody);
  } else {
    systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_KEEPALIVE, "request start GET " + config.keepAliveUrl);
    appendPushDebugLog("持续请求 GET " + config.keepAliveUrl);
    httpCode = http.GET();
  }

  unsigned long cost = millis() - t0;
  bool ok = httpCode > 0;
  if (ok) {
    appendPushDebugLog("持续请求响应码: " + String(httpCode));
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_KEEPALIVE,
                     "request ok code=" + String(httpCode) + " cost=" + String(cost) + "ms");
    if (keepAliveFastRetryActive) {
      appendPushDebugLog("持续请求恢复成功，退出故障期加速复查");
      systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_KEEPALIVE, "exit fast retry after recovery");
    }
    keepAliveFastRetryActive = false;
    keepAliveFailureDiagArmed = true;
  } else {
    appendPushDebugLog("持续请求失败: " + http.errorToString(httpCode));
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_KEEPALIVE,
                     "request failed error=" + http.errorToString(httpCode) + " cost=" + String(cost) + "ms");
    printWiFiDiagnostics("KeepAlive失败: " + http.errorToString(httpCode));
    if (keepAliveAutoDiagEnabled && keepAliveFailureDiagArmed) {
      systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_KEEPALIVE,
                       "trigger auto deep diagnostics on first consecutive failure");
      runKeepAliveDeepDiagnostics("Auto");
      keepAliveFailureDiagArmed = false;
    }
    if (!keepAliveFastRetryActive) {
      appendPushDebugLog("持续请求进入故障期加速复查");
      systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_KEEPALIVE, "enter fast retry mode");
    }
    keepAliveFastRetryActive = true;
    handleWiFiNetworkFailure("KeepAlive失败: " + http.errorToString(httpCode));
  }
  http.end();
  return ok;
}

// 由网络任务周期调用：按间隔触发持续请求。
void handleKeepAlive() {
  if (!config.keepAliveEnabled) return;

  int intervalSec = config.keepAliveInterval;
  if (intervalSec < KEEP_ALIVE_MIN_INTERVAL) intervalSec = KEEP_ALIVE_DEFAULT_INTERVAL;
  unsigned long normalIntervalMs = (unsigned long)intervalSec * 1000UL;
  unsigned long intervalMs = normalIntervalMs;
  if (keepAliveFastRetryActive && KEEP_ALIVE_FAILURE_RETRY_MS < intervalMs) {
    intervalMs = KEEP_ALIVE_FAILURE_RETRY_MS;
  }

  unsigned long now = millis();
  if (!keepAliveFirstRun && now - lastKeepAliveMillis < intervalMs) return;

  keepAliveFirstRun = false;
  lastKeepAliveMillis = now;

  // 仅当 WiFi 真正未连接时才交给看门狗重连；
  // 若已连接则发起请求，请求慢/失败可能是弱信号或上游问题，不轻易断开 WiFi。
  wl_status_t st = WiFi.status();
  if (st != WL_CONNECTED) {
    printWiFiDiagnostics("KeepAlive跳过: WiFi未连接");
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_KEEPALIVE,
                     "skip request: wifi not connected status=" + String((int)st));
    return;
  }

  if (!ensureWiFiGatewayReachable("KeepAlive前检查", true)) {
    appendPushDebugLog("持续请求跳过：WiFi网关不可达，已触发恢复");
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_KEEPALIVE,
                     "skip request: gateway unreachable, recovery triggered");
    return;
  }

  doKeepAliveRequest();
}
