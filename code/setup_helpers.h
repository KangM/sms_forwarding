#pragma once

// setup 使用的 LED、AT 等待和网络注册辅助函数。
// 重试循环中的等待：保持当前 LED 状态节奏，非阻塞刷新。
void blink_short(unsigned long gap_time = 500) {
  ledDelay(gap_time);
}

String modemLogEscaped(const String& resp) {
  String out = "";
  size_t limit = resp.length();
  if (limit > 160) limit = 160;

  for (size_t i = 0; i < limit; i++) {
    uint8_t c = (uint8_t)resp.charAt(i);
    if (c == '\r') {
      out += "\\r";
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\t') {
      out += "\\t";
    } else if (c >= 32 && c <= 126) {
      out += (char)c;
    } else {
      char buf[5];
      snprintf(buf, sizeof(buf), "\\x%02X", c);
      out += buf;
    }
  }

  if (resp.length() > limit) out += "...";
  return out;
}

String modemLogHexPreview(const String& resp) {
  String out = "";
  size_t limit = resp.length();
  if (limit > 48) limit = 48;

  for (size_t i = 0; i < limit; i++) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02X", (uint8_t)resp.charAt(i));
    if (i > 0) out += " ";
    out += buf;
  }

  if (resp.length() > limit) out += " ...";
  if (out.length() == 0) out = "<empty>";
  return out;
}

bool sendATandWaitOK(const char* cmd, unsigned long timeout) {
  unsigned long start = millis();
  String resp = modemAtSendCommandSync(String(cmd), timeout);
  if (resp.indexOf("OK") >= 0) return true;

  systemLogSerialOnly(LOG_LEVEL_WARN, LOG_MODULE_MODEM,
                      "AT failed cmd=" + String(cmd) +
                        " elapsedMs=" + String(millis() - start) +
                        " timeoutMs=" + String(timeout) +
                        " bytes=" + String(resp.length()) +
                        " resp=\"" + modemLogEscaped(resp) + "\"" +
                        " hex=" + modemLogHexPreview(resp));
  return false;
}

// 检测网络注册状态（LTE/4G）
// CEREG状态: 1=已注册本地, 5=已注册漫游
bool waitCEREG() {
  String resp = modemAtSendCommandSync("AT+CEREG?", 2000);
  // +CEREG: <n>,<stat> 其中stat=1或5表示已注册
  if (resp.indexOf("+CEREG:") >= 0) {
    if (resp.indexOf(",1") >= 0 || resp.indexOf(",5") >= 0) return true;
    if (resp.indexOf(",0") >= 0 || resp.indexOf(",2") >= 0 ||
        resp.indexOf(",3") >= 0 || resp.indexOf(",4") >= 0) return false;
  }
  return false;
}
