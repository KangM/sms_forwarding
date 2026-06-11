#pragma once

// 时间格式化、WiFi状态文本和网络诊断辅助函数。
String formatSmsTimestamp(const String& rawTimestamp) {
  String ts = rawTimestamp;
  ts.trim();

  if (ts.length() >= 12) {
    bool hasDigitsPrefix = true;
    for (int i = 0; i < 12; i++) {
      if (!isDigit(ts.charAt(i))) {
        hasDigitsPrefix = false;
        break;
      }
    }

    if (hasDigitsPrefix) {
      return ts.substring(0, 2) + "/" +
             ts.substring(2, 4) + "/" +
             ts.substring(4, 6) + " " +
             ts.substring(6, 8) + ":" +
             ts.substring(8, 10) + ":" +
             ts.substring(10, 12);
    }
  }

  ts.replace(",", " ");
  return ts;
}

String wifiStatusText(wl_status_t status) {
  switch (status) {
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_NO_SHIELD: return "WL_NO_SHIELD";
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
    default: return "UNKNOWN(" + String((int)status) + ")";
  }
}

struct WiFiGatewayPingResult {
  bool success;
  bool finished;
  uint32_t elapsedMs;
  uint32_t transmitted;
  uint32_t received;
  uint32_t totalMs;
  uint8_t ttl;
  SemaphoreHandle_t done;
};

static void wifiGatewayPingOnSuccess(esp_ping_handle_t hdl, void *args) {
  WiFiGatewayPingResult* result = (WiFiGatewayPingResult*)args;
  result->success = true;
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &result->elapsedMs, sizeof(result->elapsedMs));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &result->ttl, sizeof(result->ttl));
}

static void wifiGatewayPingOnEnd(esp_ping_handle_t hdl, void *args) {
  WiFiGatewayPingResult* result = (WiFiGatewayPingResult*)args;
  result->finished = true;
  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &result->transmitted, sizeof(result->transmitted));
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &result->received, sizeof(result->received));
  esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &result->totalMs, sizeof(result->totalMs));
  if (result->done) xSemaphoreGive(result->done);
}

String pingGatewayForWiFiDiagnostics(IPAddress gateway) {
  if (WiFi.status() != WL_CONNECTED) return "跳过（WiFi未连接）";
  if (gateway == IPAddress(0, 0, 0, 0)) return "跳过（网关地址无效）";

  WiFiGatewayPingResult result = {};
  result.done = xSemaphoreCreateBinary();
  if (!result.done) return "失败（无法创建信号量）";

  ip_addr_t targetAddr;
  targetAddr.type = IPADDR_TYPE_V4;
  targetAddr.u_addr.ip4.addr = (uint32_t)gateway;

  esp_ping_config_t pingConfig = ESP_PING_DEFAULT_CONFIG();
  pingConfig.target_addr = targetAddr;
  pingConfig.count = 1;
  pingConfig.timeout_ms = 1000;
  pingConfig.interval_ms = 100;

  esp_ping_callbacks_t callbacks = {};
  callbacks.cb_args = &result;
  callbacks.on_ping_success = wifiGatewayPingOnSuccess;
  callbacks.on_ping_end = wifiGatewayPingOnEnd;

  esp_ping_handle_t pingHandle = NULL;
  if (esp_ping_new_session(&pingConfig, &callbacks, &pingHandle) != ESP_OK) {
    vSemaphoreDelete(result.done);
    return "失败（无法创建ping会话）";
  }

  esp_ping_start(pingHandle);
  bool completed = xSemaphoreTake(result.done, pdMS_TO_TICKS(1500)) == pdTRUE;
  esp_ping_delete_session(pingHandle);
  vSemaphoreDelete(result.done);

  if (!completed || !result.finished) return "失败（ping超时）";
  if (!result.success || result.received == 0) {
    return "失败（" + String(result.transmitted) + "发/" + String(result.received) + "收，耗时" + String(result.totalMs) + "ms）";
  }
  return "成功（" + String(result.elapsedMs) + "ms，TTL=" + String(result.ttl) + "，" + String(result.transmitted) + "发/" + String(result.received) + "收）";
}

void printWiFiDiagnostics(const String& source) {
  if (!showWiFiDiagnostics) return;

  wl_status_t status = WiFi.status();
  Serial.println("=== WiFi诊断：" + source + " ===");
  Serial.println("运行时间(ms): " + String(millis()));
  Serial.println("状态: " + wifiStatusText(status) + " (" + String((int)status) + ")");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("网关: " + WiFi.gatewayIP().toString());
  Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
  Serial.println("信道: " + String(WiFi.channel()));
  if (pingGatewayInWiFiDiagnostics) {
    Serial.println("网关Ping: " + pingGatewayForWiFiDiagnostics(WiFi.gatewayIP()));
  }
  Serial.println("====================");
}
