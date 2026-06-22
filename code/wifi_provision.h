#pragma once

#define WIFI_SCAN_CACHE_MAX 20

struct WiFiScanCacheEntry {
  String ssid;
  int32_t rssi;
  wifi_auth_mode_t encryption;
};

WiFiScanCacheEntry wifiScanCache[WIFI_SCAN_CACHE_MAX];
int wifiScanCacheCount = 0;
bool wifiScanCacheValid = false;
bool wifiScanRunning = false;
unsigned long wifiScanStartedAtMs = 0;
unsigned long wifiScanDeferredStartAtMs = 0;
const unsigned long WIFI_SCAN_AFTER_PORTAL_DELAY_MS = 6000;

bool startWiFiScanAsync(bool force);
void pollWiFiScanCache();

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
  systemLogSerialOnly(ok ? LOG_LEVEL_INFO : LOG_LEVEL_ERROR,
                      LOG_MODULE_WIFI,
                      String(ok ? "config portal started" : "config portal start failed") +
                        " ap=" + apSsid + " ip=" + WiFi.softAPIP().toString());
  if (ok) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI, "已进入配网模式：请用手机或电脑连接 WiFi 热点 " + apSsid);
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI, "连接后打开浏览器访问 http://" + WiFi.softAPIP().toString() + "/wifi 配置 WiFi");
    wifiScanDeferredStartAtMs = millis() + WIFI_SCAN_AFTER_PORTAL_DELAY_MS;
  }
}

void processWiFiConfigPortalDns() {
  if (wifiConfigPortalActive) {
    dnsServer.processNextRequest();
    pollWiFiScanCache();
  }
}

String wifiDisconnectReasonText(uint8_t reason) {
  switch (reason) {
    case 2: return "AUTH_EXPIRE";
    case 4: return "ASSOC_EXPIRE";
    case 5: return "ASSOC_TOOMANY";
    case 8: return "ASSOC_LEAVE";
    case 15: return "4WAY_HANDSHAKE_TIMEOUT";
    case 200: return "BEACON_TIMEOUT";
    case 201: return "NO_AP_FOUND";
    case 202: return "AUTH_FAIL";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    case 205: return "CONNECTION_FAIL";
    case 206: return "AP_TSF_RESET";
    case 207: return "ROAMING";
    default: return "UNKNOWN";
  }
}

String wifiEventSsid(const uint8_t* ssid, uint8_t len) {
  char buf[33];
  uint8_t copyLen = len;
  if (copyLen > 32) copyLen = 32;
  memcpy(buf, ssid, copyLen);
  buf[copyLen] = '\0';
  return String(buf);
}

void setupWiFiEventLogging() {
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_START:
        systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "event STA_START");
        break;
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_WIFI,
                         "event STA_CONNECTED ssid=" +
                           wifiEventSsid(info.wifi_sta_connected.ssid, info.wifi_sta_connected.ssid_len) +
                           " channel=" + String(info.wifi_sta_connected.channel));
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_WIFI,
                         "event STA_GOT_IP ip=" + WiFi.localIP().toString() +
                           " gateway=" + WiFi.gatewayIP().toString());
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
        uint8_t reason = info.wifi_sta_disconnected.reason;
        systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI,
                         "event STA_DISCONNECTED reason=" + String(reason) +
                           " (" + wifiDisconnectReasonText(reason) + ")");
        break;
      }
      case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI, "event STA_LOST_IP");
        break;
      default:
        break;
    }
  });
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

  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI, "connect start ssid=" + ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    blink_short();
  }

  if (WiFi.status() == WL_CONNECTED) {
    // 连接成功后再次确保关闭休眠（mode/begin 可能重置该标志）
    WiFi.setSleep(false);
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_WIFI,
                     "connected ssid=" + ssid + " ip=" + WiFi.localIP().toString());
    return true;
  }

  systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI, "connect timeout or failed ssid=" + ssid);
  return false;
}

bool connectWiFiOrStartConfigPortal() {
  String ssid;
  String pass;

  if (loadSavedWiFiCredentials(ssid, pass) && connectWiFiStation(ssid, pass, 20000)) {
    return true;
  }

  if (ssid.length() == 0) {
    systemLogSerialOnly(LOG_LEVEL_WARN, LOG_MODULE_WIFI, "no saved WiFi credentials, entering config portal");
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI, "未保存 WiFi 信息，设备将启动配网页面");
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
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI,
                     "skip reconnect throttled reason=" + String(reason));
    return;
  }
  lastForce = now;

  String ssid, pass;
  if (loadSavedWiFiCredentials(ssid, pass) && ssid.length() > 0) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI,
                     "force reconnect reason=" + String(reason) + " ssid=" + ssid);
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), pass.c_str(), 0, nullptr, true);
  } else {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_WIFI, "force reconnect failed: no saved credentials");
  }
}

// WiFi 重连看门狗：在 loop 中周期调用。
// 当已配置过WiFi且不在配网门户模式时，检测掉线并主动重连，避免“假在线”后无法恢复。
void forceWiFiFullReset(const char* reason) {
  if (wifiConfigPortalActive) return;

  static unsigned long lastFullReset = 0;
  unsigned long now = millis();
  if (lastFullReset != 0 && now - lastFullReset < 60000) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI,
                     "skip full reset throttled reason=" + String(reason));
    return;
  }
  lastFullReset = now;

  String ssid, pass;
  if (!loadSavedWiFiCredentials(ssid, pass) || ssid.length() == 0) {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_WIFI, "full reset failed: no saved credentials");
    return;
  }

  systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_WIFI,
                   "full reset reason=" + String(reason) + " ssid=" + ssid);
  WiFi.disconnect(true);
  delay(300);
  WiFi.mode(WIFI_OFF);
  delay(800);
  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str(), 0, nullptr, true);
}

bool ensureWiFiGatewayReachable(const String& source, bool reconnectOnFailure) {
  if (wifiConfigPortalActive) return true;

  wl_status_t st = WiFi.status();
  if (st != WL_CONNECTED) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI,
                     source + " detected not connected status=" + String((int)st));
    WiFi.reconnect();
    return false;
  }

  String pingResult = pingGatewayForWiFiDiagnostics(WiFi.gatewayIP());
  bool ok = pingResult.startsWith("成功");
  systemLogSerialOnly(ok ? LOG_LEVEL_INFO : LOG_LEVEL_WARN,
                      LOG_MODULE_WIFI,
                      source + " gateway reachability: " + pingResult);

  if (!ok && reconnectOnFailure) {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_WIFI,
                     source + " gateway unreachable: " + pingResult);
    forceWiFiReconnect(source.c_str());
  } else if (!ok) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI,
                     source + " gateway unreachable: " + pingResult);
  }
  return ok;
}

void handleWiFiNetworkFailure(const String& source) {
  ensureWiFiGatewayReachable(source, true);
}

void wifiReachabilityWatchdog() {
  if (wifiConfigPortalActive) return;

  static unsigned long lastCheck = 0;
  static int consecutiveGatewayFailures = 0;
  unsigned long now = millis();

  if (now - lastCheck < 60000) return;
  lastCheck = now;

  wl_status_t st = WiFi.status();
  if (st != WL_CONNECTED) {
    consecutiveGatewayFailures = 0;
    return;
  }

  bool ok = ensureWiFiGatewayReachable("可达性看门狗", false);
  if (ok) {
    consecutiveGatewayFailures = 0;
    return;
  }

  consecutiveGatewayFailures++;
  systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI,
                   "reachability watchdog consecutive failures=" + String(consecutiveGatewayFailures));
  if (consecutiveGatewayFailures >= 2) {
    forceWiFiReconnect("可达性看门狗检测到网关不可达");
    consecutiveGatewayFailures = 0;
  }
}

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
    systemLogPrintln(st == WL_CONNECTED ? LOG_LEVEL_INFO : LOG_LEVEL_WARN, LOG_MODULE_WIFI,
                     "status change " + String((int)lastStatus) + " -> " + String((int)st) +
                       (st == WL_CONNECTED ? (" ip=" + WiFi.localIP().toString()) : ""));
    lastStatus = st;
  }

  if (st == WL_CONNECTED) {
    disconnectedSince = 0;
    return;
  }

  // 记录首次检测到掉线的时间
  if (disconnectedSince == 0) {
    disconnectedSince = now;
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_WIFI,
                     "watchdog detected disconnect status=" + String((int)st) + ", reconnecting");
    WiFi.reconnect();
    return;
  }

  // 持续掉线超过 30 秒，做一次更彻底的重连
  if (now - disconnectedSince > 30000) {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_WIFI,
                     "watchdog long disconnect, force reconnect");
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

    systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_WIFI,
                        "roam scan start currentRssi=" + String(curRssi));
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
      systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_WIFI,
                       "roam switch better AP bestRssi=" + String(bestRssi) +
                       " currentRssi=" + String(curRssi) +
                       " channel=" + String(WiFi.channel(bestIdx)));
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

bool startWiFiScanAsync(bool force) {
  if (wifiScanRunning) return true;
  if (!force && wifiScanCacheValid) return true;

  WiFi.scanDelete();
  int result = WiFi.scanNetworks(true, true);
  if (result == WIFI_SCAN_FAILED) {
    wifiScanRunning = false;
    return false;
  }

  wifiScanRunning = true;
  wifiScanStartedAtMs = millis();
  return true;
}

void pollWiFiScanCache() {
  if (!wifiScanRunning && wifiScanDeferredStartAtMs != 0 &&
      (long)(millis() - wifiScanDeferredStartAtMs) >= 0) {
    wifiScanDeferredStartAtMs = 0;
    startWiFiScanAsync(false);
  }

  if (!wifiScanRunning) return;

  int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) {
    if (millis() - wifiScanStartedAtMs > 15000) {
      WiFi.scanDelete();
      wifiScanRunning = false;
      wifiScanCacheValid = true;
      wifiScanCacheCount = 0;
    }
    return;
  }

  wifiScanRunning = false;
  wifiScanCacheCount = 0;

  if (count > 0) {
    for (int i = 0; i < count && wifiScanCacheCount < WIFI_SCAN_CACHE_MAX; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;

      bool duplicate = false;
      for (int j = 0; j < wifiScanCacheCount; j++) {
        if (wifiScanCache[j].ssid == ssid) {
          duplicate = true;
          if (WiFi.RSSI(i) > wifiScanCache[j].rssi) {
            wifiScanCache[j].rssi = WiFi.RSSI(i);
            wifiScanCache[j].encryption = WiFi.encryptionType(i);
          }
          break;
        }
      }
      if (duplicate) continue;

      wifiScanCache[wifiScanCacheCount].ssid = ssid;
      wifiScanCache[wifiScanCacheCount].rssi = WiFi.RSSI(i);
      wifiScanCache[wifiScanCacheCount].encryption = WiFi.encryptionType(i);
      wifiScanCacheCount++;
    }
  }

  WiFi.scanDelete();
  wifiScanCacheValid = true;
}

String scannedWiFiOptions(const String& selectedSsid) {
  String options = "<option value=''>手动输入 WiFi 名称</option>";

  if (wifiScanRunning) {
    options += "<option value=''>正在扫描附近 WiFi...</option>";
    return options;
  }

  if (!wifiScanCacheValid) {
    options += "<option value=''>尚无扫描结果，请点击扫描</option>";
    return options;
  }

  if (wifiScanCacheCount <= 0) {
    options += "<option value=''>未扫描到 WiFi，可手动输入</option>";
    return options;
  }

  for (int i = 0; i < wifiScanCacheCount; i++) {
    String security = wifiScanCache[i].encryption == WIFI_AUTH_OPEN ? "开放" : "加密";
    String selected = wifiScanCache[i].ssid == selectedSsid ? " selected" : "";
    options += "<option value='" + htmlEscape(wifiScanCache[i].ssid) + "'" + selected + ">";
    options += htmlEscape(wifiScanCache[i].ssid) + " (" + String(wifiScanCache[i].rssi) + " dBm, " + security + ")";
    options += "</option>";
  }

  return options;
}

void handleWiFiScan() {
  bool refresh = server.arg("refresh") == "1";
  if (refresh) {
    startWiFiScanAsync(true);
  }
  pollWiFiScanCache();

  String json = "{";
  json += "\"running\":" + String(wifiScanRunning ? "true" : "false") + ",";
  json += "\"valid\":" + String(wifiScanCacheValid ? "true" : "false") + ",";
  json += "\"count\":" + String(wifiScanCacheCount) + ",";
  json += "\"networks\":[";
  for (int i = 0; i < wifiScanCacheCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + jsonEscape(wifiScanCache[i].ssid) + "\",";
    json += "\"rssi\":" + String(wifiScanCache[i].rssi) + ",";
    json += "\"security\":\"" + String(wifiScanCache[i].encryption == WIFI_AUTH_OPEN ? "开放" : "加密") + "\"";
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleWiFiConfigPage() {
  if (wifiScanRunning) {
    WiFi.scanDelete();
    wifiScanRunning = false;
  }

  String savedSsid;
  String savedPass;
  bool hasSaved = loadSavedWiFiCredentials(savedSsid, savedPass);
  String ssidOptions = scannedWiFiOptions(savedSsid);

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>WiFi配网</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}.box{max-width:460px;margin:0 auto;background:#fff;padding:18px;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,.12)}label{display:block;margin-top:12px;font-weight:bold}input,select{width:100%;box-sizing:border-box;padding:10px;margin-top:6px;border:1px solid #ccc;border-radius:5px;background:white}button{width:100%;padding:12px;margin-top:16px;border:0;border-radius:5px;background:#2196F3;color:white;font-size:16px}.hint{font-size:12px;color:#666;margin-top:10px;line-height:1.5}.row{display:flex;gap:8px;align-items:center}.row select{flex:1}.row button{width:auto;white-space:nowrap;margin-top:6px;padding:10px 12px;font-size:14px}</style>";
  html += "<script>";
  html += "function pickSsid(sel){if(sel.value){document.getElementById('ssid').value=sel.value;}}";
  html += "function togglePass(){var p=document.getElementById('wifiPass');var b=document.getElementById('passToggle');var show=p.type==='password';p.type=show?'text':'password';b.textContent=show?'隐藏':'显示';}";
  html += "function setScanText(t){var e=document.getElementById('scanStatus');if(e)e.textContent=t;}";
  html += "function renderScan(d){var s=document.getElementById('wifiSelect');var cur=document.getElementById('ssid').value;s.innerHTML='<option value=\"\">手动输入 WiFi 名称</option>';if(d.running){s.innerHTML+='<option value=\"\">正在扫描附近 WiFi...</option>';setScanText('正在扫描...');return false;}if(!d.valid){s.innerHTML+='<option value=\"\">尚无扫描结果，请点击刷新</option>';setScanText('尚无扫描结果');return true;}if(!d.networks.length){s.innerHTML+='<option value=\"\">未扫描到 WiFi，可手动输入</option>';setScanText('未扫描到 WiFi');return true;}d.networks.forEach(function(n){var o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm, '+n.security+')';if(n.ssid===cur)o.selected=true;s.appendChild(o);});setScanText('已扫描到 '+d.networks.length+' 个 WiFi');return true;}";
  html += "function loadScan(refresh){fetch('/wifiscan'+(refresh?'?refresh=1':''),{cache:'no-store'}).then(function(r){return r.json();}).then(function(d){var done=renderScan(d);if(!done)setTimeout(function(){loadScan(false);},1200);}).catch(function(){setScanText('扫描状态获取失败');});}";
  html += "window.addEventListener('load',function(){loadScan(false);});";
  html += "</script>";
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
  html += "<label>扫描到的WiFi</label><div class='row'><select id='wifiSelect' onchange='pickSsid(this)'>" + ssidOptions + "</select><button type='button' onclick='loadScan(true)'>刷新</button></div>";
  html += "<div id='scanStatus' class='hint'>页面已加载，可直接手动输入或点击刷新</div>";
  html += "<label>WiFi名称</label><input id='ssid' name='ssid' value='" + htmlEscape(savedSsid) + "' required>";
  html += "<label>WiFi密码</label><div class='row'><input id='wifiPass' name='pass' type='password' value=''><button id='passToggle' type='button' onclick='togglePass()'>显示</button></div>";
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
    if (server.uri() == "/favicon.ico") {
      server.send(204, "text/plain", "");
      return;
    }
    handleCaptivePortalRedirect();
    return;
  }
  server.send(404, "text/plain", "Not Found");
}
