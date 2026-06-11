#pragma once

#define WIFI_PROVISION_NAMESPACE "wifi_cfg"
#define WIFI_PROVISION_DNS_PORT 53

bool wifiConfigPortalActive = false;

bool loadSavedWiFiCredentials(String& ssid, String& pass) {
  preferences.begin(WIFI_PROVISION_NAMESPACE, true);
  ssid = preferences.getString("ssid", "");
  pass = preferences.getString("pass", "");
  preferences.end();
  return ssid.length() > 0;
}

void saveWiFiCredentials(const String& ssid, const String& pass) {
  preferences.begin(WIFI_PROVISION_NAMESPACE, false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
}

String wifiConfigApSsid() {
  uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF);
  return "SMS-Setup-" + String(chipId, HEX);
}

void startWiFiConfigPortal() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);

  String apSsid = wifiConfigApSsid();
  bool ok = WiFi.softAP(apSsid.c_str());
  wifiConfigPortalActive = ok;
  if (ok) {
    dnsServer.start(WIFI_PROVISION_DNS_PORT, "*", WiFi.softAPIP());
  }
  Serial.println(ok ? "WiFi配网AP已启动" : "WiFi配网AP启动失败");
  Serial.println("AP名称: " + apSsid);
  Serial.println("AP密码: 无（开放网络）");
  Serial.println("配网页面: http://" + WiFi.softAPIP().toString() + "/wifi");
}

void processWiFiConfigPortalDns() {
  if (wifiConfigPortalActive) {
    dnsServer.processNextRequest();
  }
}

bool connectWiFiStation(const String& ssid, const String& pass, unsigned long timeoutMs) {
  if (ssid.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.begin(ssid.c_str(), pass.c_str(), 0, nullptr, true);

  Serial.println("连接WiFi: " + ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    blink_short();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi已连接");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi连接失败或超时: " + ssid);
  return false;
}

bool connectWiFiOrStartConfigPortal() {
  String ssid;
  String pass;

  if (loadSavedWiFiCredentials(ssid, pass) && connectWiFiStation(ssid, pass, 20000)) {
    return true;
  }

  if (ssid.length() == 0) {
    Serial.println("未保存WiFi，进入配网模式");
  }

  startWiFiConfigPortal();
  return false;
}

String scannedWiFiOptions(const String& selectedSsid) {
  String options = "<option value=''>选择扫描到的WiFi，或手动输入</option>";
  int count = WiFi.scanNetworks(false, true);

  if (count <= 0) {
    options += "<option value=''>未扫描到WiFi</option>";
    WiFi.scanDelete();
    return options;
  }

  for (int i = 0; i < count; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;

    String security = WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "开放" : "加密";
    String selected = ssid == selectedSsid ? " selected" : "";
    options += "<option value='" + htmlEscape(ssid) + "'" + selected + ">";
    options += htmlEscape(ssid) + " (" + String(WiFi.RSSI(i)) + " dBm, " + security + ")";
    options += "</option>";
  }

  WiFi.scanDelete();
  return options;
}

void handleWiFiConfigPage() {
  String savedSsid;
  String savedPass;
  bool hasSaved = loadSavedWiFiCredentials(savedSsid, savedPass);
  String ssidOptions = scannedWiFiOptions(savedSsid);

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>WiFi配网</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}.box{max-width:460px;margin:0 auto;background:#fff;padding:18px;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,.12)}label{display:block;margin-top:12px;font-weight:bold}input,select{width:100%;box-sizing:border-box;padding:10px;margin-top:6px;border:1px solid #ccc;border-radius:5px;background:white}button{width:100%;padding:12px;margin-top:16px;border:0;border-radius:5px;background:#2196F3;color:white;font-size:16px}.hint{font-size:12px;color:#666;margin-top:10px;line-height:1.5}</style>";
  html += "<script>function pickSsid(sel){if(sel.value){document.getElementById('ssid').value=sel.value;}}</script>";
  html += "</head><body><div class='box'><h2>WiFi配网</h2>";
  html += "<div class='hint'>当前模式: ";
  if (WiFi.status() == WL_CONNECTED) {
    html += "已连接 " + htmlEscape(WiFi.SSID()) + "，IP " + WiFi.localIP().toString();
  } else {
    html += "配网AP，地址 " + WiFi.softAPIP().toString();
  }
  html += "</div>";
  if (hasSaved) {
    html += "<div class='hint'>已保存WiFi: " + htmlEscape(savedSsid) + "</div>";
  }
  html += "<form method='POST' action='/wifisave'>";
  html += "<label>扫描到的WiFi</label><select onchange='pickSsid(this)'>" + ssidOptions + "</select>";
  html += "<label>WiFi名称</label><input id='ssid' name='ssid' value='" + htmlEscape(savedSsid) + "' required>";
  html += "<label>WiFi密码</label><input name='pass' type='password' value=''>";
  html += "<button type='submit'>保存并重启</button>";
  html += "</form><div class='hint'>保存后设备会重启并尝试连接新WiFi。忘记密码或连接失败时，会重新开启配网AP。</div>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleWiFiConfigSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();

  if (ssid.length() == 0) {
    server.send(400, "text/plain", "WiFi名称不能为空");
    return;
  }

  String savedSsid;
  String savedPass;
  if (pass.length() == 0 && loadSavedWiFiCredentials(savedSsid, savedPass) && savedSsid == ssid && savedPass.length() > 0) {
    pass = savedPass;
  }

  saveWiFiCredentials(ssid, pass);
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h3>WiFi已保存，设备即将重启...</h3></body></html>");
  delay(1000);
  ESP.restart();
}

void clearWiFiCredentials() {
  preferences.begin(WIFI_PROVISION_NAMESPACE, false);
  preferences.clear();
  preferences.end();
}

void handleWiFiConfigClear() {
  if (!checkAuth()) return;

  clearWiFiCredentials();
  server.send(200, "application/json", "{\"success\":true,\"message\":\"WiFi连接记录已清除，设备即将重启进入配网测试流程。\"}");
  delay(1000);
  ESP.restart();
}

void handleCaptivePortalRedirect() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/wifi", true);
  server.send(302, "text/plain", "");
}

void handleRootOrWiFiConfig() {
  if (wifiConfigPortalActive) {
    handleCaptivePortalRedirect();
    return;
  }
  handleRoot();
}

void handleCaptivePortalNotFound() {
  if (wifiConfigPortalActive) {
    handleCaptivePortalRedirect();
    return;
  }
  server.send(404, "text/plain", "Not Found");
}
