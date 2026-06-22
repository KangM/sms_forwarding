#pragma once

// AT 命令发送辅助函数。
// 发送AT命令并获取响应
String sendATCommand(const char* cmd, unsigned long timeout) {
  return modemAtSendCommandSync(String(cmd), timeout);
}
