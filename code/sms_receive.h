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
  Serial.println("=== 处理短信内容 ===");
  Serial.println("发送者: " + String(sender));
  Serial.println("时间戳: " + formattedTimestamp);
  Serial.println("内容: " + String(text));
  Serial.println("====================");
  if (pushDebugEnabled) {
    printWiFiDiagnostics("短信到达");
  }

  // 检查是否在号码黑名单中
  if (isInNumberBlackList(sender)) {
    Serial.println("发送者在号码黑名单中，忽略该短信");
    return;
  }

  // 检查是否为管理员命令
  if (isAdmin(sender)) {
    Serial.println("收到管理员短信，检查命令...");
    String smsText = String(text);
    smsText.trim();

    // 检查是否为命令格式
    if (smsText.startsWith("SMS:") || smsText.equals("RESET")) {
      processAdminCommand(sender, text);
      // 命令已处理，不再发送普通通知邮件
      return;
    }
  }

  // 发送通知http（推送到所有启用的通道）
  ledSetState(LED_BUSY_PUSHING);
  sendSMSToServer(sender, text, formattedTimestamp.c_str());
  // 发送通知邮件
  String subject = ""; subject+="短信";subject+=sender;subject+=",";subject+=text;
  String body = ""; body+="来自：";body+=sender;body+="，时间：";body+=formattedTimestamp;body+="，内容：";body+=text;
  sendEmailNotification(subject.c_str(), body.c_str());
  ledRestoreNormal();
}

bool submitIncomingPdu(const String& pduLine, const String& source, int storageIndex) {
  Serial.println("收到PDU数据来源: " + source + (storageIndex >= 0 ? ("，存储索引: " + String(storageIndex)) : ""));
  Serial.println("收到PDU数据: " + pduLine);
  Serial.println("PDU长度: " + String(pduLine.length()) + " 字符");

  if (!isHexString(pduLine)) {
    Serial.println("PDU数据不是有效的十六进制字符串");
    return false;
  }

  if (!pdu.decodePDU(pduLine.c_str())) {
    Serial.println("PDU解析失败！");
    return false;
  }

  Serial.println("PDU解析成功");
  Serial.println("=== 短信内容 ===");
  Serial.println("发送者: " + String(pdu.getSender()));
  Serial.println("时间戳: " + String(pdu.getTimeStamp()));
  Serial.println("内容: " + String(pdu.getText()));

  int* concatInfo = pdu.getConcatInfo();
  int refNumber = concatInfo[0];
  int partNumber = concatInfo[1];
  int totalParts = concatInfo[2];

  Serial.printf("长短信信息: 参考号=%d, 当前=%d, 总计=%d\n", refNumber, partNumber, totalParts);
  Serial.println("===============");

  if (totalParts > 1 && partNumber > 0) {
    Serial.printf("收到长短信分段 %d/%d\n", partNumber, totalParts);

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

        Serial.printf("  已缓存分段 %d，当前已收到 %d/%d\n",
                     partNumber,
                     concatBuffer[slot].receivedParts,
                     totalParts);
      } else {
        Serial.printf("  分段 %d 已存在，跳过\n", partNumber);
      }
    }

    if (concatBuffer[slot].receivedParts >= totalParts) {
      Serial.println("长短信已收齐，开始合并转发");
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
  Serial.println("CMGR响应: " + resp);

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
    Serial.println("读取存储短信失败，索引: " + String(smsIndex));
    return false;
  }

  Serial.println("读取存储短信成功，索引: " + String(smsIndex));
  bool messageReady = submitIncomingPdu(pduLine, "CMTI", smsIndex);
  if (messageReady) {
    cleanupSmsStorageReserve();
  }
  return messageReady;
}

// 处理URC和PDU
void checkSerial1URC() {
  static enum { IDLE,
                WAIT_PDU } state = IDLE;

  String line = readSerialLine(Serial1);
  if (line.length() == 0) return;

  // 打印到调试串口
  Serial.println("Debug> " + line);

  if (state == IDLE) {
    // 检测到短信上报URC头
    if (line.startsWith("+CMT:")) {
      Serial.println("检测到+CMT，等待PDU数据...");
      state = WAIT_PDU;
    } else if (line.startsWith("+CMTI:")) {
      int commaIdx = line.lastIndexOf(',');
      if (commaIdx >= 0) {
        int smsIndex = line.substring(commaIdx + 1).toInt();
        Serial.println("检测到+CMTI，新短信索引: " + String(smsIndex));
        readStoredSmsByIndex(smsIndex);
      } else {
        Serial.println("检测到+CMTI，但无法解析索引: " + line);
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
      Serial.println("收到非PDU数据，返回IDLE状态");
      state = IDLE;
    }
  }
}
