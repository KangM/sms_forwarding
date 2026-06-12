#pragma once

// 掉线检测功能：按固定间隔通过WiFi向指定URL发起HTTP请求。
// 该函数由网络后台任务(netTask)调用，可安全阻塞，不影响主循环。

// 上次发起持续请求的时间戳（millis）
unsigned long lastKeepAliveMillis = 0;
// 是否还未触发过首次请求（启动后先触发一次，不等间隔）
bool keepAliveFirstRun = true;

// 执行一次持续请求，返回是否成功（HTTP响应码 > 0 视为网络可达）
bool doKeepAliveRequest() {
  if (config.keepAliveUrl.length() == 0) {
    Serial.println("[KeepAlive] 跳过：URL为空");
    appendPushDebugLog("持续请求跳过：URL为空");
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
    Serial.println("[KeepAlive] POST " + config.keepAliveUrl);
    appendPushDebugLog("持续请求 POST " + config.keepAliveUrl);
    if (config.keepAliveBody.length() > 0) {
      appendPushDebugLog("持续请求 BODY: " + config.keepAliveBody);
    }
    httpCode = http.POST(config.keepAliveBody);
  } else {
    Serial.println("[KeepAlive] GET " + config.keepAliveUrl);
    appendPushDebugLog("持续请求 GET " + config.keepAliveUrl);
    httpCode = http.GET();
  }

  unsigned long cost = millis() - t0;
  bool ok = httpCode > 0;
  if (ok) {
    Serial.println("[KeepAlive] 成功 响应码=" + String(httpCode) + " 耗时=" + String(cost) + "ms");
    appendPushDebugLog("持续请求响应码: " + String(httpCode));
  } else {
    Serial.println("[KeepAlive] 失败 错误=" + http.errorToString(httpCode) + " 耗时=" + String(cost) + "ms");
    appendPushDebugLog("持续请求失败: " + http.errorToString(httpCode));
    printWiFiDiagnostics("KeepAlive失败: " + http.errorToString(httpCode));
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
  unsigned long intervalMs = (unsigned long)intervalSec * 1000UL;

  unsigned long now = millis();
  if (!keepAliveFirstRun && now - lastKeepAliveMillis < intervalMs) return;

  keepAliveFirstRun = false;
  lastKeepAliveMillis = now;

  // 仅当 WiFi 真正未连接时才交给看门狗重连；
  // 若已连接则发起请求，请求慢/失败可能是弱信号或上游问题，不轻易断开 WiFi。
  wl_status_t st = WiFi.status();
  if (st != WL_CONNECTED) {
    Serial.println("[KeepAlive] 跳过请求：WiFi未连接(status=" + String((int)st) + ")");
    printWiFiDiagnostics("KeepAlive跳过: WiFi未连接");
    return;
  }

  if (!ensureWiFiGatewayReachable("KeepAlive前检查", true)) {
    Serial.println("[KeepAlive] 跳过请求：WiFi网关不可达，已触发恢复");
    appendPushDebugLog("持续请求跳过：WiFi网关不可达，已触发恢复");
    return;
  }

  doKeepAliveRequest();
}
