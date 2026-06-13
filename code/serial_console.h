#pragma once

// USB serial command console.
//
// This replaces raw character-by-character passthrough. Lines beginning with
// "AT" are forwarded to the modem as complete AT commands; other lines are
// handled locally for ESP32 diagnostics and recovery.

String usbConsoleLine = "";
unsigned long usbConsoleLastInputAt = 0;
const unsigned long USB_CONSOLE_IDLE_SUBMIT_MS = 250;
const unsigned long PERF_LOOP_GAP_BUDGET_MS = 1000;
const unsigned long PERF_LOOP_STACK_TOTAL = 8192;
const unsigned long PERF_NET_STACK_TOTAL = 8192;

void printSerialConsoleHelp() {
  Serial.println();
  Serial.println("=== SMS Forwarding Serial Console ===");
  Serial.println("AT...        Forward one AT command to modem");
  Serial.println("PINGGW       Ping WiFi gateway once");
  Serial.println("DNS <host>   Resolve host via current WiFi DNS");
  Serial.println("TCP <h> [p]  Test TCP connect, default port 443");
  Serial.println("HTTP <url>   Probe one HTTP/HTTPS GET request");
  Serial.println("KADEBUG      Deep-diagnose current KeepAlive URL");
  Serial.println("KAAUTO [ON|OFF] Show or toggle KeepAlive auto-diagnostics");
  Serial.println("LOGTIME [ON|OFF] Show or toggle device uptime prefix in serial logs");
  Serial.println("LOG [N]      Show recent runtime logs");
  Serial.println("LOG CLEAR    Clear runtime log ring");
  Serial.println("LOG FLASH    Show active flash log file");
  Serial.println("LOG FLASH PREV Show previous rotated flash log file");
  Serial.println("LOG FLASH CLEAR Clear flash log files");
  Serial.println("RECONNECT    Force WiFi reconnect");
  Serial.println("WIFIRESET    Full WiFi radio reset and reconnect");
  Serial.println("MODEMRESET   Power-cycle modem and wait for AT");
  Serial.println("STATUS       Print brief ESP32 runtime status");
  Serial.println("PERF         Print lightweight performance counters");
  Serial.println("PERFRESET [s] Reset counters and auto-report after seconds. Default: 300");
  Serial.println("RESTART      Restart ESP32");
  Serial.println("HELP or ?    Show this help");
  Serial.println("=====================================");
}

void printSerialConsoleStatus() {
  Serial.println("=== ESP32 Status ===");
  Serial.println("Uptime(ms): " + String(millis()));
  Serial.println("Free heap: " + String(ESP.getFreeHeap()));
  Serial.println("Min free heap: " + String(ESP.getMinFreeHeap()));
  Serial.println("Config valid: " + String(configValid ? "yes" : "no"));
  Serial.println("WiFi status: " + wifiStatusText(WiFi.status()) + " (" + String((int)WiFi.status()) + ")");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("Gateway: " + WiFi.gatewayIP().toString());
  Serial.println("Subnet mask: " + WiFi.subnetMask().toString());
  Serial.println("DNS: " + WiFi.dnsIP().toString());
  Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
  Serial.println("Channel: " + String(WiFi.channel()));
  Serial.println("BSSID: " + WiFi.BSSIDstr());
  Serial.println("MAC: " + WiFi.macAddress());
  Serial.println("Gateway ping: " + pingGatewayForWiFiDiagnostics(WiFi.gatewayIP()));
  Serial.println("Chip temp: " + chipTemperatureStatusText());
  Serial.println("====================");
}

void handleSerialConsoleLogCommand(const String& cmd, const String& upper) {
  if (upper == "LOG") {
    printSystemLogRing();
    return;
  }
  if (upper == "LOG CLEAR") {
    clearSystemLogRing();
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SERIAL, "runtime log ring cleared from serial console");
    return;
  }
  if (upper == "LOG FLASH") {
    printSystemLogFlash(SYSTEM_LOG_FLASH_PATH, "=== Flash Log (active) ===");
    return;
  }
  if (upper == "LOG FLASH PREV") {
    printSystemLogFlash(SYSTEM_LOG_FLASH_PREV_PATH, "=== Flash Log (previous) ===");
    return;
  }
  if (upper == "LOG FLASH CLEAR") {
    clearSystemLogFlash();
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SERIAL, "flash log files cleared from serial console");
    return;
  }
  if (upper.startsWith("LOG ")) {
    String arg = cmd.substring(4);
    arg.trim();
    if (arg.length() > 0) {
      long requested = arg.toInt();
      if (requested > 0) {
        printSystemLogRing((size_t)requested);
        return;
      }
    }
  }
  Serial.println("[SerialConsole] Unknown LOG usage. Try LOG, LOG 50, LOG CLEAR, LOG FLASH, LOG FLASH PREV, LOG FLASH CLEAR");
}

void resetPerfCounters(unsigned long autoReportSeconds = 300) {
  loopPerfWindowStartMs = millis();
  loopPerfLastTickMs = loopPerfWindowStartMs;
  loopPerfIterations = 0;
  loopPerfMaxGapMs = 0;
  if (autoReportSeconds > 0) {
    perfAutoReportAtMs = loopPerfWindowStartMs + autoReportSeconds * 1000UL;
  } else {
    perfAutoReportAtMs = 0;
  }
}

String formatPercent(float percent) {
  return String(percent, 1) + "%";
}

String formatPercent(unsigned long part, unsigned long total) {
  if (total == 0) return "n/a";
  return formatPercent((float)part * 100.0f / (float)total);
}

String levelForFreePercent(float freePercent) {
  if (freePercent >= 30.0f) return "OK";
  if (freePercent >= 15.0f) return "BUSY";
  if (freePercent >= 8.0f) return "WARNING";
  return "DANGER";
}

String levelForUsedPercent(float usedPercent) {
  if (usedPercent < 50.0f) return "OK";
  if (usedPercent < 75.0f) return "BUSY";
  if (usedPercent < 90.0f) return "WARNING";
  return "DANGER";
}

String levelForLoopGap(unsigned long gapMs) {
  if (gapMs < 1000) return "OK";
  if (gapMs < 3000) return "BUSY";
  if (gapMs < 8000) return "WARNING";
  return "DANGER";
}

String freeMetricSuffix(unsigned long freeValue, unsigned long totalValue) {
  if (totalValue == 0) return "n/a";
  float freePercent = (float)freeValue * 100.0f / (float)totalValue;
  return formatPercent(freePercent) + " free, " + levelForFreePercent(freePercent);
}

String usedMetricSuffix(unsigned long usedValue, unsigned long totalValue) {
  if (totalValue == 0) return "n/a";
  float usedPercent = (float)usedValue * 100.0f / (float)totalValue;
  return formatPercent(usedPercent) + " used, " + levelForUsedPercent(usedPercent);
}

String loopGapMetricSuffix(unsigned long gapMs) {
  float budgetPercent = (float)gapMs * 100.0f / (float)PERF_LOOP_GAP_BUDGET_MS;
  return formatPercent(budgetPercent) + " of 1000ms budget, " + levelForLoopGap(gapMs);
}

void printPerfLevelsLegend() {
  Serial.println("Levels:");
  Serial.println("  OK       heap/stack free >= 30%, queue used < 50%, max gap < 1000ms");
  Serial.println("  BUSY     heap/stack free 15-30%, queue used 50-75%, max gap 1000-3000ms");
  Serial.println("  WARNING  heap/stack free 8-15%, queue used 75-90%, max gap 3000-8000ms");
  Serial.println("  DANGER   heap/stack free < 8%, queue used >= 90%, max gap >= 8000ms");
}

void printSerialConsolePerf() {
  unsigned long now = millis();
  unsigned long windowMs = now - loopPerfWindowStartMs;
  float loopPerSec = 0.0f;
  if (windowMs > 0) {
    loopPerSec = (float)loopPerfIterations * 1000.0f / (float)windowMs;
  }

  Serial.println("=== Performance ===");
  Serial.println("Window(ms): " + String(windowMs));
  Serial.println("Loop iterations: " + String(loopPerfIterations));
  Serial.println("Loop avg/sec: " + String(loopPerSec, 1));
  Serial.println("Loop max gap(ms): " + String(loopPerfMaxGapMs) + " (" + loopGapMetricSuffix(loopPerfMaxGapMs) + ")");

  unsigned long heapSize = ESP.getHeapSize();
  unsigned long freeHeap = ESP.getFreeHeap();
  unsigned long minFreeHeap = ESP.getMinFreeHeap();
  Serial.println("Free heap: " + String(freeHeap) + " / " + String(heapSize) + " (" + freeMetricSuffix(freeHeap, heapSize) + ")");
  Serial.println("Min free heap: " + String(minFreeHeap) + " / " + String(heapSize) + " (" + freeMetricSuffix(minFreeHeap, heapSize) + ")");
  Serial.println("Heap size: " + String(heapSize));
  if (appLoopTaskHandle) {
    unsigned long loopStackFree = uxTaskGetStackHighWaterMark(appLoopTaskHandle);
    Serial.println("Loop stack high water: " + String(loopStackFree) + " / " + String(PERF_LOOP_STACK_TOTAL) +
                   " (" + freeMetricSuffix(loopStackFree, PERF_LOOP_STACK_TOTAL) + ")");
  }
  if (netTaskHandle) {
    unsigned long netStackFree = uxTaskGetStackHighWaterMark(netTaskHandle);
    Serial.println("Net task stack high water: " + String(netStackFree) + " / " + String(PERF_NET_STACK_TOTAL) +
                   " (" + freeMetricSuffix(netStackFree, PERF_NET_STACK_TOTAL) + ")");
  }
  if (notifyQueue) {
    unsigned long queueUsed = uxQueueMessagesWaiting(notifyQueue);
    unsigned long queueFree = uxQueueSpacesAvailable(notifyQueue);
    unsigned long queueTotal = queueUsed + queueFree;
    Serial.println("Notify queue used/free: " + String(queueUsed) + "/" + String(queueTotal) +
                   " (" + usedMetricSuffix(queueUsed, queueTotal) + ")");
  }
  printPerfLevelsLegend();
  Serial.println("===================");
}

void handleSerialConsoleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  String upper = cmd;
  upper.toUpperCase();

  if (upper == "HELP" || upper == "?") {
    printSerialConsoleHelp();
  } else if (upper == "LOG" || upper.startsWith("LOG ")) {
    handleSerialConsoleLogCommand(cmd, upper);
  } else if (upper == "PINGGW") {
    Serial.println("Gateway ping: " + pingGatewayForWiFiDiagnostics(WiFi.gatewayIP()));
  } else if (upper.startsWith("DNS ")) {
    String host = cmd.substring(4);
    host.trim();
    Serial.println("DNS(" + host + "): " + resolveHostForWiFiDiagnostics(host));
  } else if (upper.startsWith("TCP ")) {
    String args = cmd.substring(4);
    args.trim();
    int space = args.indexOf(' ');
    String host = space >= 0 ? args.substring(0, space) : args;
    String portArg = space >= 0 ? args.substring(space + 1) : "";
    host.trim();
    portArg.trim();
    uint16_t port = portArg.length() > 0 ? (uint16_t)portArg.toInt() : 443;
    Serial.println("TCP(" + host + ":" + String(port) + "): " + tcpConnectForWiFiDiagnostics(host, port));
  } else if (upper.startsWith("HTTP ")) {
    String url = cmd.substring(5);
    url.trim();
    Serial.println("HTTP(" + url + "): " + httpProbeForWiFiDiagnostics(url));
  } else if (upper == "KADEBUG") {
    runKeepAliveDeepDiagnostics("SerialConsole");
  } else if (upper == "KAAUTO") {
    Serial.println("KeepAlive auto diagnostics: " + String(keepAliveAutoDiagEnabled ? "ON" : "OFF"));
    Serial.println("KeepAlive failure armed: " + String(keepAliveFailureDiagArmed ? "YES" : "NO"));
  } else if (upper == "LOGTIME") {
    Serial.println("Serial log time prefix: " + String(systemLogSerialIncludeTime ? "ON" : "OFF"));
  } else if (upper == "LOGTIME ON") {
    systemLogSerialIncludeTime = true;
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SERIAL, "serial log time prefix enabled");
  } else if (upper == "LOGTIME OFF") {
    systemLogSerialIncludeTime = false;
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SERIAL, "serial log time prefix disabled");
  } else if (upper == "KAAUTO ON") {
    keepAliveAutoDiagEnabled = true;
    keepAliveFailureDiagArmed = true;
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SERIAL, "keepalive auto diagnostics enabled");
  } else if (upper == "KAAUTO OFF") {
    keepAliveAutoDiagEnabled = false;
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SERIAL, "keepalive auto diagnostics disabled");
  } else if (upper == "RECONNECT") {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SERIAL, "force wifi reconnect requested");
    forceWiFiReconnect("serial console");
  } else if (upper == "WIFIRESET") {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_SERIAL, "full wifi reset requested");
    forceWiFiFullReset("serial console");
  } else if (upper == "MODEMRESET") {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SERIAL, "modem reset requested");
    resetModule();
  } else if (upper == "STATUS") {
    printSerialConsoleStatus();
  } else if (upper == "PERF") {
    printSerialConsolePerf();
  } else if (upper == "PERFRESET" || upper.startsWith("PERFRESET ")) {
    unsigned long seconds = 300;
    int space = cmd.indexOf(' ');
    if (space >= 0) {
      String secondsArg = cmd.substring(space + 1);
      secondsArg.trim();
      if (secondsArg.length() > 0) {
        seconds = secondsArg.toInt();
      }
    }
    resetPerfCounters(seconds);
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SERIAL,
                     "performance counters reset autoReportSec=" + String(seconds));
  } else if (upper == "RESTART") {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SERIAL, "esp32 restart requested");
    delay(200);
    ESP.restart();
  } else if (upper.startsWith("AT")) {
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SERIAL, "forward AT command=" + cmd);
    String resp = sendATCommand(cmd.c_str(), 5000);
    if (resp.length() > 0) {
      Serial.println(resp);
    } else {
      systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SERIAL, "AT response timeout or empty response");
    }
  } else {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SERIAL, "unknown command=" + cmd);
    Serial.println("Type HELP for available commands.");
  }
}

void handleUsbSerialConsole() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    usbConsoleLastInputAt = millis();

    if (c == '\r' || c == '\n') {
      handleSerialConsoleCommand(usbConsoleLine);
      usbConsoleLine = "";
      usbConsoleLastInputAt = 0;
      continue;
    }

    if (usbConsoleLine.length() < 240) {
      usbConsoleLine += c;
    } else {
      usbConsoleLine = "";
      usbConsoleLastInputAt = 0;
      systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SERIAL, "input line too long, discarded");
    }
  }

  if (usbConsoleLine.length() > 0 &&
      usbConsoleLastInputAt > 0 &&
      millis() - usbConsoleLastInputAt >= USB_CONSOLE_IDLE_SUBMIT_MS) {
    handleSerialConsoleCommand(usbConsoleLine);
    usbConsoleLine = "";
    usbConsoleLastInputAt = 0;
  }
}
