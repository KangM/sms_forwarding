#pragma once

// 掉线检测功能：按固定间隔通过WiFi向指定URL发起HTTP请求。

// 上次发起持续请求的时间戳（millis）
unsigned long lastKeepAliveMillis = 0;
// 是否还未触发过首次请求（启动后先触发一次，不等间隔）
bool keepAliveFirstRun = true;

// 执行一次持续请求
void doKeepAliveRequest() {
  if (config.keepAliveUrl.length() == 0) {
    appendPushDebugLog("持续请求跳过：URL为空");
    return;
  }

  HTTPClient http;
  http.begin(config.keepAliveUrl);

  int httpCode = 0;
  if (config.keepAliveMethod == "POST") {
    http.addHeader("Content-Type", "application/json");
    appendPushDebugLog("持续请求 POST " + config.keepAliveUrl);
    if (config.keepAliveBody.length() > 0) {
      appendPushDebugLog("持续请求 BODY: " + config.keepAliveBody);
    }
    httpCode = http.POST(config.keepAliveBody);
  } else {
    appendPushDebugLog("持续请求 GET " + config.keepAliveUrl);
    httpCode = http.GET();
  }

  if (httpCode > 0) {
    appendPushDebugLog("持续请求响应码: " + String(httpCode));
  } else {
    appendPushDebugLog("持续请求失败: " + http.errorToString(httpCode));
  }
  http.end();
}

// 在loop中调用：非阻塞地按间隔触发持续请求
void handleKeepAlive() {
  if (!config.keepAliveEnabled) return;
  if (WiFi.status() != WL_CONNECTED) return;

  int intervalSec = config.keepAliveInterval;
  if (intervalSec < KEEP_ALIVE_MIN_INTERVAL) intervalSec = KEEP_ALIVE_DEFAULT_INTERVAL;
  unsigned long intervalMs = (unsigned long)intervalSec * 1000UL;

  unsigned long now = millis();
  if (!keepAliveFirstRun && now - lastKeepAliveMillis < intervalMs) return;

  keepAliveFirstRun = false;
  lastKeepAliveMillis = now;
  doKeepAliveRequest();
}
