#pragma once

#define WIFI_PROVISION_NAMESPACE "wifi_cfg"
#define WIFI_PROVISION_DNS_PORT 53

// wifiConfigPortalActive 定义在主程序全局，供多个模块共享

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
  // 关闭WiFi省电休眠，避免射频休眠导致连接“假在线”、推送超时和掉线不恢复
  WiFi.setSleep(false);
  // 断开后自动重连，并在重启后自动连接已保存的AP
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  // 同SSID多AP（漫游）场景：全信道扫描 + 按信号强度排序，优先连最强的AP
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.begin(ssid.c_str(), pass.c_str(), 0, nullptr, true);

  Serial.println("连接WiFi: " + ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    blink_short();
  }

  if (WiFi.status() == WL_CONNECTED) {
    // 连接成功后再次确保关闭休眠（mode/begin 可能重置该标志）
    WiFi.setSleep(false);
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

// 强制断开并用已保存凭据重连（供掉线检测在“假在线”时调用）
void forceWiFiReconnect(const char* reason) {
  if (wifiConfigPortalActive) return;  // 配网模式不干预

  static unsigned long lastForce = 0;
  unsigned long now = millis();
  // 限频：30 秒内最多强制重连一次，避免频繁打断
  if (lastForce != 0 && now - lastForce < 30000) {
    Serial.println("[WiFi] 跳过强制重连（30秒内已执行过）原因: " + String(reason));
    return;
  }
  lastForce = now;

  String ssid, pass;
  if (loadSavedWiFiCredentials(ssid, pass) && ssid.length() > 0) {
    Serial.println("[WiFi] 强制重连(" + String(reason) + ") SSID: " + ssid);
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), pass.c_str(), 0, nullptr, true);
  } else {
    Serial.println("[WiFi] 强制重连失败：无已保存凭据");
  }
}

// WiFi 重连看门狗：在 loop 中周期调用。
// 当已配置过WiFi且不在配网门户模式时，检测掉线并主动重连，避免“假在线”后无法恢复。
void wifiReconnectWatchdog() {
  if (wifiConfigPortalActive) return;  // 配网模式不干预

  static unsigned long lastCheck = 0;
  static unsigned long disconnectedSince = 0;
  static wl_status_t lastStatus = WL_IDLE_STATUS;
  unsigned long now = millis();

  // 每 5 秒检查一次
  if (now - lastCheck < 5000) return;
  lastCheck = now;

  wl_status_t st = WiFi.status();
  // 状态变化时打印，便于排查
  if (st != lastStatus) {
    Serial.println("[WiFi] 状态变化: " + String((int)lastStatus) + " -> " + String((int)st) +
                   (st == WL_CONNECTED ? (" 已连接 IP=" + WiFi.localIP().toString()) : ""));
    lastStatus = st;
  }

  if (st == WL_CONNECTED) {
    disconnectedSince = 0;
    return;
  }

  // 记录首次检测到掉线的时间
  if (disconnectedSince == 0) {
    disconnectedSince = now;
    Serial.println("[WiFi] 检测到掉线(status=" + String((int)st) + ")，尝试重连...");
    WiFi.reconnect();
    return;
  }

  // 持续掉线超过 30 秒，做一次更彻底的重连
  if (now - disconnectedSince > 30000) {
    Serial.println("[WiFi] 长时间掉线，执行强制重连");
    forceWiFiReconnect("看门狗检测长时间掉线");
    disconnectedSince = now;  // 重置计时，避免频繁重连
  }
}

// 漫游看门狗：同SSID多AP场景下，若当前AP信号过弱，扫描并切换到更强的AP。
// 使用异步扫描，避免阻塞主循环。基础 Arduino WiFi 不支持 802.11k/v/r，
// 因此这里手动实现“弱信号触发重选AP”。
#define WIFI_ROAM_RSSI_THRESHOLD -75   // 当前信号弱于此值才考虑漫游(dBm)
#define WIFI_ROAM_RSSI_IMPROVE 8       // 候选AP至少强这么多才切换(dB)
#define WIFI_ROAM_CHECK_INTERVAL 60000 // 检查间隔(ms)

void wifiRoamWatchdog() {
  if (wifiConfigPortalActive) return;
  if (WiFi.status() != WL_CONNECTED) return;

  static unsigned long lastCheck = 0;
  static bool scanning = false;
  unsigned long now = millis();

  if (!scanning) {
    if (now - lastCheck < WIFI_ROAM_CHECK_INTERVAL) return;
    lastCheck = now;

    int curRssi = WiFi.RSSI();
    if (curRssi >= WIFI_ROAM_RSSI_THRESHOLD) return;  // 信号够好，不折腾

    Serial.println("[Roam] 当前信号较弱(" + String(curRssi) + "dBm)，发起异步扫描寻找更强AP");
    WiFi.scanNetworks(true);  // 异步扫描
    scanning = true;
    return;
  }

  // 扫描进行中：检查是否完成
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  scanning = false;

  if (n <= 0) {
    WiFi.scanDelete();
    return;
  }

  String curSsid = WiFi.SSID();
  int curRssi = WiFi.RSSI();
  uint8_t* curBssid = WiFi.BSSID();

  int bestIdx = -1;
  int bestRssi = curRssi;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) != curSsid) continue;        // 只看同名AP
    if (WiFi.RSSI(i) > bestRssi) {
      bestRssi = WiFi.RSSI(i);
      bestIdx = i;
    }
  }

  if (bestIdx >= 0 && bestRssi - curRssi >= WIFI_ROAM_RSSI_IMPROVE) {
    uint8_t* newBssid = WiFi.BSSID(bestIdx);
    bool sameAp = curBssid && newBssid && memcmp(curBssid, newBssid, 6) == 0;
    if (!sameAp) {
      Serial.println("[Roam] 发现更强AP: " + String(bestRssi) + "dBm (当前 " + String(curRssi) +
                     "dBm)，切换到信道 " + String(WiFi.channel(bestIdx)));
      String ssid, pass;
      if (loadSavedWiFiCredentials(ssid, pass) && ssid.length() > 0) {
        WiFi.disconnect();
        delay(100);
        // 指定 BSSID 连接到更强的那台AP
        WiFi.begin(ssid.c_str(), pass.c_str(), WiFi.channel(bestIdx), newBssid, true);
      }
    }
  }

  WiFi.scanDelete();
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
