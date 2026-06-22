#pragma once

// 模组串口读取、PDU 解析和 CMT/CMTI 接收处理。
// 读取串口一行（含回车换行），返回行字符串，无新行时返回空
String readSerialLine(HardwareSerial& port) {
  static char lineBuf[SERIAL_BUFFER_SIZE];
  static int linePos = 0;

  while (port.available()) {
    char c = port.read();
    if (c == '\n') {
      lineBuf[linePos] = 0;
      String res = String(lineBuf);
      linePos = 0;
      return res;
    } else if (c != '\r') {  // 跳过\r
      if (linePos < SERIAL_BUFFER_SIZE - 1)
        lineBuf[linePos++] = c;
      else
        linePos = 0;  //超长报错保护，重头计
    }
  }
  return "";
}

// 检查字符串是否为有效的十六进制PDU数据
bool isHexString(const String& str) {
  if (str.length() == 0) return false;
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

// 处理最终的短信内容（管理员命令检查和转发）
void processSmsContent(const char* sender, const char* text, const char* timestamp) {
  String formattedTimestamp = formatSmsTimestamp(String(timestamp));
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                      "process sms detail sender=" + String(sender) +
                      " timestamp=" + formattedTimestamp +
                      " text=" + String(text));
  if (pushDebugEnabled) {
    printWiFiDiagnostics("短信到达");
  }
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                   "process sms sender=" + String(sender) + " timestamp=" + formattedTimestamp);

  // 检查是否在号码黑名单中
  if (isInNumberBlackList(sender)) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SMS,
                     "sms ignored by blacklist sender=" + String(sender));
    return;
  }

  // 检查是否为管理员命令
  if (isAdmin(sender)) {
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                     "admin sms detected sender=" + String(sender));
    String smsText = String(text);
    smsText.trim();

    // 检查是否为命令格式
    if (smsText.startsWith("SMS:") || smsText.equals("RESET")) {
      processAdminCommand(sender, text);
      // 命令已处理，不再发送普通通知邮件
      return;
    }
  }

  // 投递到网络后台任务异步处理（推送 + 邮件），避免阻塞主循环收短信
  ledSetState(LED_BUSY_PUSHING);
  enqueueSmsNotify(String(sender), String(text), formattedTimestamp);
  ledRestoreNormal();
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                   "sms enqueued sender=" + String(sender));
}

bool submitIncomingPdu(const String& pduLine, const String& source, int storageIndex) {
  String storageIndexInfo = storageIndex >= 0 ? (" index=" + String(storageIndex)) : String("");
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                   "incoming pdu source=" + source +
                   storageIndexInfo +
                   " len=" + String(pduLine.length()));

  if (!isHexString(pduLine)) {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_SMS,
                     "invalid pdu hex source=" + source);
    return false;
  }

  if (!pdu.decodePDU(pduLine.c_str())) {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_SMS,
                     "pdu decode failed source=" + source);
    return false;
  }

  int* concatInfo = pdu.getConcatInfo();
  int refNumber = concatInfo[0];
  int partNumber = concatInfo[1];
  int totalParts = concatInfo[2];

  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                   "pdu decode ok sender=" + String(pdu.getSender()) +
                   " parts=" + String(partNumber) + "/" + String(totalParts) +
                   " ref=" + String(refNumber));
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                      "pdu text timestamp=" + String(pdu.getTimeStamp()) +
                      " text=" + String(pdu.getText()));

  if (totalParts > 1 && partNumber > 0) {
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                     "concat part received ref=" + String(refNumber) +
                     " part=" + String(partNumber) + "/" + String(totalParts));

    int slot = findOrCreateConcatSlot(refNumber, pdu.getSender(), totalParts);
    int partIndex = partNumber - 1;
    if (partIndex >= 0 && partIndex < MAX_CONCAT_PARTS) {
      if (!concatBuffer[slot].parts[partIndex].valid) {
        concatBuffer[slot].parts[partIndex].valid = true;
        concatBuffer[slot].parts[partIndex].text = String(pdu.getText());
        concatBuffer[slot].receivedParts++;

        if (concatBuffer[slot].receivedParts == 1) {
          concatBuffer[slot].timestamp = String(pdu.getTimeStamp());
        }

        systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                         "concat cached ref=" + String(refNumber) +
                         " received=" + String(concatBuffer[slot].receivedParts) +
                         "/" + String(totalParts));
      } else {
        systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SMS,
                         "concat duplicate ref=" + String(refNumber) +
                         " part=" + String(partNumber));
      }
    }

    if (concatBuffer[slot].receivedParts >= totalParts) {
      systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                       "concat assembled ref=" + String(refNumber) +
                       " total=" + String(totalParts));
      String fullText = assembleConcatSms(slot);
      processSmsContent(concatBuffer[slot].sender.c_str(),
                       fullText.c_str(),
                       concatBuffer[slot].timestamp.c_str());
      clearConcatSlot(slot);
      return true;
    }

    return false;
  } else {
    processSmsContent(pdu.getSender(), pdu.getText(), pdu.getTimeStamp());
    return true;
  }
}

bool readStoredSmsByIndex(int smsIndex) {
  String resp = sendATCommand(("AT+CMGR=" + String(smsIndex)).c_str(), 5000);
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                   "read stored sms index=" + String(smsIndex) +
                   " respLen=" + String(resp.length()));

  String pduLine = "";
  int lineStart = 0;
  for (int i = 0; i <= resp.length(); i++) {
    if (i == resp.length() || resp.charAt(i) == '\n') {
      String line = resp.substring(lineStart, i);
      line.trim();
      if (isLikelyPduLine(line)) {
        pduLine = line;
        break;
      }
      lineStart = i + 1;
    }
  }

  if (pduLine.length() == 0) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SMS,
                     "read stored sms failed index=" + String(smsIndex));
    return false;
  }

  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                   "read stored sms ok index=" + String(smsIndex));
  bool messageReady = submitIncomingPdu(pduLine, "CMTI", smsIndex);
  if (messageReady) {
    cleanupSmsStorageReserve();
  }
  return messageReady;
}

int pendingStoredSmsIndexes[MODEM_AT_QUEUE_LEN];
int pendingStoredSmsHead = 0;
int pendingStoredSmsCount = 0;
bool processingStoredSmsQueue = false;

bool enqueueStoredSmsIndex(int smsIndex) {
  if (pendingStoredSmsCount >= MODEM_AT_QUEUE_LEN) {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_SMS,
                     "stored sms queue full index=" + String(smsIndex));
    return false;
  }
  int pos = (pendingStoredSmsHead + pendingStoredSmsCount) % MODEM_AT_QUEUE_LEN;
  pendingStoredSmsIndexes[pos] = smsIndex;
  pendingStoredSmsCount++;
  return true;
}

unsigned long smsRxWatchdogLastCheckMs = 0;
unsigned long smsRxWatchdogLastConfigMs = 0;
unsigned long smsRxWatchdogRecoveries = 0;
const unsigned long SMS_RX_WATCHDOG_INTERVAL_MS = 10UL * 60UL * 1000UL;

bool smsRxCnmiLooksGood(const String& resp) {
  int idx = resp.indexOf("+CNMI:");
  if (idx < 0) return false;

  String line = resp.substring(idx + 6);
  int lineEnd = line.indexOf('\n');
  if (lineEnd >= 0) line = line.substring(0, lineEnd);
  line.trim();
  return line.startsWith("2,2,");
}

void reconfigureSmsReceiveMode(const char* reason) {
  systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SMS,
                   "sms rx reconfigure reason=" + String(reason));
  sendATCommand("AT+CMGF=0", 2000);
  String cnmiResp = sendATCommand("AT+CNMI=2,2,0,0,0", 3000);
  smsRxWatchdogLastConfigMs = millis();
  if (cnmiResp.indexOf("OK") >= 0) {
    smsRxWatchdogRecoveries++;
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                     "sms rx reconfigure ok recoveries=" + String(smsRxWatchdogRecoveries));
  } else {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SMS,
                     "sms rx reconfigure failed resp=" + cnmiResp);
  }
}

void pollSmsReceiveWatchdog() {
  unsigned long now = millis();
  if (smsRxWatchdogLastCheckMs != 0 &&
      now - smsRxWatchdogLastCheckMs < SMS_RX_WATCHDOG_INTERVAL_MS) {
    return;
  }
  if (modemAtIsBusy() || modemAtQueueDepth() > 0 || pendingStoredSmsCount > 0) return;

  smsRxWatchdogLastCheckMs = now;
  String resp = sendATCommand("AT+CNMI?", 3000);
  if (!smsRxCnmiLooksGood(resp)) {
    reconfigureSmsReceiveMode("cnmi_mismatch");
  } else {
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS, "sms rx watchdog ok");
  }
}

void pollStoredSmsQueue() {
  if (processingStoredSmsQueue || pendingStoredSmsCount <= 0) return;
  if (modemAtIsBusy()) return;

  processingStoredSmsQueue = true;
  int smsIndex = pendingStoredSmsIndexes[pendingStoredSmsHead];
  pendingStoredSmsHead = (pendingStoredSmsHead + 1) % MODEM_AT_QUEUE_LEN;
  pendingStoredSmsCount--;
  readStoredSmsByIndex(smsIndex);
  processingStoredSmsQueue = false;
}

void processModemUrcLine(const String& line) {
  static enum { IDLE,
                WAIT_PDU } state = IDLE;

  if (line.length() == 0) return;

  // 打印到调试串口
  systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "urc line=" + line);

  if (state == IDLE) {
    // 检测到短信上报URC头
    if (line.startsWith("+CMT:")) {
      systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS, "urc +CMT detected");
      state = WAIT_PDU;
    } else if (line.startsWith("+CMTI:")) {
      int commaIdx = line.lastIndexOf(',');
      if (commaIdx >= 0) {
        int smsIndex = line.substring(commaIdx + 1).toInt();
        systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SMS,
                         "urc +CMTI detected index=" + String(smsIndex));
        enqueueStoredSmsIndex(smsIndex);
      } else {
        systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SMS,
                         "urc +CMTI parse failed line=" + line);
      }
    }
  } else if (state == WAIT_PDU) {
    // 跳过空行
    if (line.length() == 0) {
      return;
    }

    // 如果是十六进制字符串，认为是PDU数据
    if (isHexString(line)) {
      submitIncomingPdu(line, "CMT", -1);
      state = IDLE;
    }
    // 如果是其他内容（OK、ERROR等），也返回IDLE
    else {
      systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SMS,
                       "expected pdu after +CMT but got line=" + line);
      state = IDLE;
    }
  }
}

void handleModemUrc(const String& line, void* userData) {
  (void)userData;
  processModemUrcLine(line);
}

// 兼容旧调用名；真实串口读取已经移到 modem_at。
void checkSerial1URC() {
  modemAtPoll();
}
