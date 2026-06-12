#pragma once

// 推送日志开关、测试推送和请求/响应日志函数。
// enqueueTestPush 定义在 net_task.h（包含顺序在后），此处前置声明。
bool enqueueTestPush(const String& sender, const String& message, const String& timestamp);
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
  enqueueTestPush("TEST_PUSH", "这是一条推送通道测试消息", timestamp);

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

void handlePushFilterTest() {
  if (!checkAuth()) return;

  PushFilterRule rule;
  rule.enabled = server.arg("enabled") == "on" || server.arg("enabled") == "true" || server.arg("enabled") == "1";
  rule.target = server.arg("target") == "sender" ? PUSH_FILTER_TARGET_SENDER : PUSH_FILTER_TARGET_MESSAGE;

  String mode = server.arg("mode");
  if (mode == "not_contains") {
    rule.mode = PUSH_FILTER_MODE_NOT_CONTAINS;
  } else if (mode == "starts_with") {
    rule.mode = PUSH_FILTER_MODE_STARTS_WITH;
  } else if (mode == "ends_with") {
    rule.mode = PUSH_FILTER_MODE_ENDS_WITH;
  } else {
    rule.mode = PUSH_FILTER_MODE_CONTAINS;
  }

  rule.expr = server.arg("expr");
  String sender = server.arg("sender");
  String message = server.arg("message");

  PushFilterEvalResult result = evaluatePushFilter(rule, sender, message);
  String json = "{";
  json += "\"success\":" + String(result.valid ? "true" : "false") + ",";
  json += "\"allowed\":" + String(result.allowed ? "true" : "false") + ",";
  json += "\"message\":\"" + jsonEscape(result.reason) + "\",";
  json += "\"target\":\"" + jsonEscape(result.targetName) + "\",";
  json += "\"mode\":\"" + jsonEscape(result.modeName) + "\",";
  json += "\"expr\":\"" + jsonEscape(result.expr) + "\"";
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
