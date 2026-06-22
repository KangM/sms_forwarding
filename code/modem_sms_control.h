#pragma once

// 短信发送、模组重启、黑名单和管理员短信命令处理。
bool sendATandWaitOK(const char* cmd, unsigned long timeout);
// enqueueEmailNotify 定义在 net_task.h（包含顺序在后），此处前置声明。
bool enqueueEmailNotify(const String& subject, const String& body);

// 发送短信（PDU模式）
bool sendSMS(const char* phoneNumber, const char* message) {
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM,
                   "send sms start phone=" + String(phoneNumber));
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_MODEM,
                      "send sms text=" + String(message));

  // 使用pdulib编码PDU
  pdu.setSCAnumber();  // 使用默认短信中心
  int pduLen = pdu.encodePDU(phoneNumber, message);

  if (pduLen < 0) {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_MODEM,
                     "encode pdu failed code=" + String(pduLen) +
                     " phone=" + String(phoneNumber));
    return false;
  }

  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_MODEM,
                      "encoded pdu len=" + String(pduLen));

  // 发送AT+CMGS命令
  String cmgsCmd = "AT+CMGS=";
  cmgsCmd += pduLen;

  String resp = "";
  bool ok = modemAtSendPduSmsSync(String(phoneNumber), String(pdu.getSMS()), pduLen, 30000, resp);
  if (ok) {
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM,
                     "send sms ok phone=" + String(phoneNumber));
    return true;
  }
  systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_MODEM,
                   "send sms failed phone=" + String(phoneNumber) +
                   " resp=" + resp);
  return false;
}

// 新增“模组断电重启”函数
void modemPowerCycle() {
  pinMode(MODEM_EN_PIN, OUTPUT);
  systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "modem power cycle start");
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "EN low: modem off");
  digitalWrite(MODEM_EN_PIN, LOW);
  delay(1200);  // 关机时间给够

  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "EN high: modem on");
  digitalWrite(MODEM_EN_PIN, HIGH);
  delay(6000);  // 等模组完全启动再发AT（关键）
}


// 重启模组
void resetModule() {
  systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "reset module start");

  modemPowerCycle();

  // 硬重启后做 AT 握手确认（最多等 10 秒）
  bool ok = false;
  for (int i = 0; i < 10; i++) {
    if (sendATandWaitOK("AT", 1000)) {
      ok = true;
      break;
    }
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_MODEM,
                     "reset module waiting for AT attempt=" + String(i + 1));
  }

  if (ok) {
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "reset module recovered");
  } else {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_MODEM, "reset module AT still unavailable");
  }
}


// 检查发送者是否在号码黑名单中
bool isInNumberBlackList(const char* sender) {
  if (config.numberBlackList.length() == 0) return false;

  String originalSender = String(sender);
  bool has86 = originalSender.startsWith("+86");
  String strippedSender = has86 ? originalSender.substring(3) : "";

  int listLen = (int)config.numberBlackList.length();

  int start = 0;
  while (start <= listLen) {
    int end = config.numberBlackList.indexOf('\n', start);
    if (end == -1) end = listLen;

    String line = config.numberBlackList.substring(start, end);
    line.trim();

    if (line.length() > 0 && (line.equals(originalSender) || (has86 && line.equals(strippedSender)))) {
      return true;
    }

    start = end + 1;
  }

  return false;
}

// 检查发送者是否为管理员
bool isAdmin(const char* sender) {
  if (config.adminPhone.length() == 0) return false;

  // 去除可能的国际区号前缀进行比较
  String senderStr = String(sender);
  String adminStr = config.adminPhone;

  // 去除+86前缀
  if (senderStr.startsWith("+86")) {
    senderStr = senderStr.substring(3);
  }
  if (adminStr.startsWith("+86")) {
    adminStr = adminStr.substring(3);
  }

  return senderStr.equals(adminStr);
}

// 处理管理员命令
void processAdminCommand(const char* sender, const char* text) {
  String cmd = String(text);
  cmd.trim();

  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM,
                   "admin command sender=" + String(sender) + " cmd=" + cmd);

  // 处理 SMS:号码:内容 命令
  if (cmd.startsWith("SMS:")) {
    int firstColon = cmd.indexOf(':');
    int secondColon = cmd.indexOf(':', firstColon + 1);

    if (secondColon > firstColon + 1) {
      String targetPhone = cmd.substring(firstColon + 1, secondColon);
      String smsContent = cmd.substring(secondColon + 1);

      targetPhone.trim();
      smsContent.trim();

      systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_MODEM,
                       "admin sms command target=" + targetPhone);
      systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_MODEM,
                          "admin sms command text=" + smsContent);

      bool success = sendSMS(targetPhone.c_str(), smsContent.c_str());

      // 发送邮件通知结果
      String subject = success ? "短信发送成功" : "短信发送失败";
      String body = "管理员命令执行结果:\n";
      body += "命令: " + cmd + "\n";
      body += "目标号码: " + targetPhone + "\n";
      body += "短信内容: " + smsContent + "\n";
      body += "执行结果: " + String(success ? "成功" : "失败");

      enqueueEmailNotify(subject, body);
    } else {
      systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_MODEM,
                       "admin sms command invalid format");
      enqueueEmailNotify("命令执行失败", "SMS命令格式错误，正确格式: SMS:号码:内容");
    }
  }
  // 处理 RESET 命令
  else if (cmd.equals("RESET")) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "admin reset command");

    // 先发送邮件通知（因为重启后就发不了了）
    sendEmailNotification("重启命令已执行", "收到RESET命令，即将重启模组和ESP32...");

    // 重启模组
    resetModule();

    // 重启ESP32
    systemLogSerialOnly(LOG_LEVEL_WARN, LOG_MODULE_SYSTEM, "ESP32 restart requested by admin RESET");
    delay(1000);
    ESP.restart();
  }
  else {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_MODEM,
                     "admin unknown command=" + cmd);
  }
}
