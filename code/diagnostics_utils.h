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

String chipTemperatureLevelText(float celsius) {
  if (isnan(celsius) || celsius < -40.0f || celsius > 125.0f) return "unavailable";
  if (celsius < 55.0f) return "NORMAL";
  if (celsius < 70.0f) return "WARM";
  if (celsius < 85.0f) return "HOT";
  return "VERY HOT";
}

String chipTemperatureStatusText() {
  float celsius = temperatureRead();
  String level = chipTemperatureLevelText(celsius);
  if (level == "unavailable") return "unavailable";
  return String(celsius, 1) + " C (" + level + ")";
}

struct ParsedWiFiDiagUrl {
  bool valid;
  String scheme;
  String host;
  uint16_t port;
  String path;
};

bool parseUrlForWiFiDiagnostics(const String& url, ParsedWiFiDiagUrl& parsed) {
  parsed = {};
  int schemeSep = url.indexOf("://");
  if (schemeSep <= 0) return false;

  parsed.scheme = url.substring(0, schemeSep);
  parsed.scheme.toLowerCase();

  int hostStart = schemeSep + 3;
  int pathStart = url.indexOf('/', hostStart);
  String authority = pathStart >= 0 ? url.substring(hostStart, pathStart) : url.substring(hostStart);
  parsed.path = pathStart >= 0 ? url.substring(pathStart) : "/";

  if (authority.length() == 0) return false;
  if (parsed.path.length() == 0) parsed.path = "/";

  int colon = authority.lastIndexOf(':');
  if (colon > 0 && authority.indexOf(']') < 0) {
    parsed.host = authority.substring(0, colon);
    parsed.port = (uint16_t)authority.substring(colon + 1).toInt();
  } else {
    parsed.host = authority;
    parsed.port = parsed.scheme == "https" ? 443 : 80;
  }

  if (parsed.host.length() == 0 || parsed.port == 0) return false;
  parsed.valid = true;
  return true;
}

String resolveHostForWiFiDiagnostics(const String& host, IPAddress* resolvedIp = nullptr) {
  if (WiFi.status() != WL_CONNECTED) return "失败（WiFi未连接）";
  if (host.length() == 0) return "失败（host为空）";

  IPAddress ip;
  unsigned long t0 = millis();
  int rc = WiFi.hostByName(host.c_str(), ip);
  unsigned long cost = millis() - t0;
  if (rc == 1) {
    if (resolvedIp) *resolvedIp = ip;
    return "成功（" + host + " -> " + ip.toString() + "，耗时" + String(cost) + "ms）";
  }
  return "失败（hostByName=" + String(rc) + "，耗时" + String(cost) + "ms）";
}

String tcpConnectForWiFiDiagnostics(const String& host, uint16_t port) {
  if (WiFi.status() != WL_CONNECTED) return "失败（WiFi未连接）";
  if (host.length() == 0 || port == 0) return "失败（host或port无效）";

  IPAddress ip;
  String dnsResult = resolveHostForWiFiDiagnostics(host, &ip);
  if (!dnsResult.startsWith("成功")) {
    return "失败（DNS未通过: " + dnsResult + "）";
  }

  WiFiClient client;
  unsigned long t0 = millis();
  bool ok = client.connect(ip, port);
  unsigned long cost = millis() - t0;
  if (ok) {
    client.stop();
    return "成功（" + ip.toString() + ":" + String(port) + "，耗时" + String(cost) + "ms）";
  }
  return "失败（" + ip.toString() + ":" + String(port) + "，耗时" + String(cost) + "ms）";
}

String httpProbeForWiFiDiagnostics(const String& url, const String& method = "GET", const String& body = "") {
  if (WiFi.status() != WL_CONNECTED) return "失败（WiFi未连接）";
  if (url.length() == 0) return "失败（URL为空）";

  ParsedWiFiDiagUrl parsed;
  if (!parseUrlForWiFiDiagnostics(url, parsed)) {
    return "失败（URL格式无效）";
  }

  unsigned long t0 = millis();
  int httpCode = 0;
  HTTPClient http;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;

  if (parsed.scheme == "https") {
    secureClient.setInsecure();
    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    if (!http.begin(secureClient, url)) return "失败（HTTPS begin失败）";
  } else {
    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    if (!http.begin(plainClient, url)) return "失败（HTTP begin失败）";
  }

  if (method == "POST") {
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(body);
  } else {
    httpCode = http.GET();
  }

  unsigned long cost = millis() - t0;
  String result;
  if (httpCode > 0) {
    result = "成功（HTTP " + String(httpCode) + "，耗时" + String(cost) + "ms）";
  } else {
    result = "失败（" + http.errorToString(httpCode) + "，耗时" + String(cost) + "ms）";
  }
  http.end();
  return result;
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
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "diag start source=" + source);
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "uptimeMs=" + String(millis()));
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "status=" + wifiStatusText(status) + " (" + String((int)status) + ")");
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "SSID=" + WiFi.SSID());
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "IP=" + WiFi.localIP().toString());
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "Gateway=" + WiFi.gatewayIP().toString());
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "Subnet=" + WiFi.subnetMask().toString());
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "DNS=" + WiFi.dnsIP().toString());
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "RSSI=" + String(WiFi.RSSI()) + " dBm");
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "Channel=" + String(WiFi.channel()));
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "BSSID=" + WiFi.BSSIDstr());
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "MAC=" + WiFi.macAddress());
  if (pingGatewayInWiFiDiagnostics) {
    systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "Gateway ping=" + pingGatewayForWiFiDiagnostics(WiFi.gatewayIP()));
  }
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "diag end source=" + source);
}

void runKeepAliveDeepDiagnostics(const String& source) {
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG, "deep diag start source=" + source);
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG,
                   "wifi status=" + wifiStatusText(WiFi.status()) + " (" + String((int)WiFi.status()) + ")");
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG,
                   "gateway ping=" + pingGatewayForWiFiDiagnostics(WiFi.gatewayIP()));

  if (config.keepAliveUrl.length() == 0) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_NETDIAG, "keepalive url not configured");
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG, "deep diag end source=" + source);
    return;
  }

  ParsedWiFiDiagUrl parsed;
  if (!parseUrlForWiFiDiagnostics(config.keepAliveUrl, parsed)) {
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG, "keepalive url=" + config.keepAliveUrl);
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_NETDIAG, "url parse failed: invalid format");
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG, "deep diag end source=" + source);
    return;
  }

  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG, "keepalive url=" + config.keepAliveUrl);
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG,
                   "url parse scheme=" + parsed.scheme + " host=" + parsed.host +
                   " port=" + String(parsed.port) + " path=" + parsed.path);
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG,
                   "DNS(" + parsed.host + ")=" + resolveHostForWiFiDiagnostics(parsed.host));
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG,
                   "TCP(1.1.1.1:443)=" + tcpConnectForWiFiDiagnostics("1.1.1.1", 443));
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG,
                   "TCP(" + parsed.host + ":" + String(parsed.port) + ")=" +
                   tcpConnectForWiFiDiagnostics(parsed.host, parsed.port));
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG,
                   "HTTP(" + config.keepAliveMethod + ")=" +
                   httpProbeForWiFiDiagnostics(config.keepAliveUrl, config.keepAliveMethod, config.keepAliveBody));
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_NETDIAG, "deep diag end source=" + source);
}
