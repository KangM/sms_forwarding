#pragma once

// USB serial command console.
//
// This replaces raw character-by-character passthrough. Lines beginning with
// "AT" are forwarded to the modem as complete AT commands; other lines are
// handled locally for ESP32 diagnostics and recovery.

String usbConsoleLine = "";
unsigned long usbConsoleLastInputAt = 0;
const unsigned long USB_CONSOLE_IDLE_SUBMIT_MS = 250;

void printSerialConsoleHelp() {
  Serial.println();
  Serial.println("=== SMS Forwarding Serial Console ===");
  Serial.println("AT...        Forward one AT command to modem");
  Serial.println("WIFI         Print WiFi diagnostics and gateway ping");
  Serial.println("PINGGW       Ping WiFi gateway once");
  Serial.println("RECONNECT    Force WiFi reconnect");
  Serial.println("WIFIRESET    Full WiFi radio reset and reconnect");
  Serial.println("MODEMRESET   Power-cycle modem and wait for AT");
  Serial.println("STATUS       Print brief ESP32 runtime status");
  Serial.println("RESTART      Restart ESP32");
  Serial.println("HELP or ?    Show this help");
  Serial.println("=====================================");
}

void printSerialConsoleStatus() {
  Serial.println("=== ESP32 Status ===");
  Serial.println("Uptime(ms): " + String(millis()));
  Serial.println("Free heap: " + String(ESP.getFreeHeap()));
  Serial.println("Min free heap: " + String(ESP.getMinFreeHeap()));
  Serial.println("WiFi status: " + wifiStatusText(WiFi.status()) + " (" + String((int)WiFi.status()) + ")");
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("Gateway: " + WiFi.gatewayIP().toString());
  Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
  Serial.println("Config valid: " + String(configValid ? "yes" : "no"));
  Serial.println("====================");
}

void handleSerialConsoleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  String upper = cmd;
  upper.toUpperCase();

  if (upper == "HELP" || upper == "?") {
    printSerialConsoleHelp();
  } else if (upper == "WIFI") {
    printWiFiDiagnostics("SerialConsole");
  } else if (upper == "PINGGW") {
    Serial.println("Gateway ping: " + pingGatewayForWiFiDiagnostics(WiFi.gatewayIP()));
  } else if (upper == "RECONNECT") {
    Serial.println("[SerialConsole] Force WiFi reconnect requested");
    forceWiFiReconnect("serial console");
  } else if (upper == "WIFIRESET") {
    Serial.println("[SerialConsole] Full WiFi reset requested");
    forceWiFiFullReset("serial console");
  } else if (upper == "MODEMRESET") {
    Serial.println("[SerialConsole] Modem reset requested");
    resetModule();
  } else if (upper == "STATUS") {
    printSerialConsoleStatus();
  } else if (upper == "RESTART") {
    Serial.println("[SerialConsole] Restarting ESP32...");
    delay(200);
    ESP.restart();
  } else if (upper.startsWith("AT")) {
    Serial.println("[SerialConsole] Forward AT: " + cmd);
    String resp = sendATCommand(cmd.c_str(), 5000);
    if (resp.length() > 0) {
      Serial.println(resp);
    } else {
      Serial.println("[SerialConsole] AT response timeout or empty response");
    }
  } else {
    Serial.println("[SerialConsole] Unknown command: " + cmd);
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
      Serial.println("[SerialConsole] Input line too long, discarded");
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
