#pragma once

// 推送日志开关、测试推送和请求/响应日志函数。
String formatUptimeClock() {
  unsigned long totalSeconds = millis() / 1000;
  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds / 60) % 60;
  unsigned long seconds = totalSeconds % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(buf);
}

void appendPushDebugLog(const String& line) {
  if (!pushDebugEnabled) return;

  String entry = "[" + formatUptimeClock() + "] " + line;
  Serial.println("[PushDebug] " + entry);
  pushDebugLog += entry + "\n";

  if (pushDebugLog.length() > PUSH_DEBUG_LOG_MAX) {
    int trimPos = pushDebugLog.indexOf('\n', pushDebugLog.length() - PUSH_DEBUG_LOG_MAX);
    if (trimPos >= 0) {
      pushDebugLog = pushDebugLog.substring(trimPos + 1);
    } else {
      pushDebugLog = pushDebugLog.substring(pushDebugLog.length() - PUSH_DEBUG_LOG_MAX);
    }
  }
}

void handlePushDebug() {
  if (!checkAuth()) return;

  String action = server.arg("action");
  if (action == "toggle") {
    pushDebugEnabled = !pushDebugEnabled;
    appendPushDebugLog(String("推送日志已") + (pushDebugEnabled ? "开启" : "关闭"));
  } else if (action == "clear") {
    pushDebugLog = "";
  }

  String json = "{";
  json += "\"success\":true,";
  json += "\"enabled\":" + String(pushDebugEnabled ? "true" : "false") + ",";
  json += "\"message\":\"" + jsonEscape(pushDebugLog.length() ? pushDebugLog : "暂无推送日志") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleTestPush() {
  if (!checkAuth()) return;

  appendPushDebugLog("手动触发测试推送");
  String timestamp = formatSmsTimestamp("26010112000032");
  sendSMSToServer("TEST_PUSH", "这是一条推送通道测试消息", timestamp.c_str());

  String json = "{";
  json += "\"success\":true,";
  json += "\"message\":\"测试推送已触发。";
  if (!pushDebugEnabled) {
    json += "推送日志当前关闭，如需查看请求和响应请先开启日志开关。";
  } else {
    json += "是否实际发出HTTP请求请查看下方推送日志。";
  }
  json += "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void logPushRequest(const String& channelName, const String& method, const String& url, const String& body) {
  if (!pushDebugEnabled) return;
  appendPushDebugLog("[" + channelName + "] REQUEST " + method + " " + url);
  if (body.length() > 0) {
    appendPushDebugLog("[" + channelName + "] REQUEST BODY: " + body);
  }
}

void logPushResponse(const String& channelName, int httpCode, const String& response) {
  if (!pushDebugEnabled) return;
  appendPushDebugLog("[" + channelName + "] RESPONSE CODE: " + String(httpCode));
  if (response.length() > 0) {
    appendPushDebugLog("[" + channelName + "] RESPONSE BODY: " + response);
  }
}
