#pragma once

// 短信列表、模块短信存储空间显示和列表长短信合并。
String jsonEscape(const String& str);

// 处理飞行模式控制请求
String htmlEscape(const String& str) {
  String result = "";
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c == '&') result += "&amp;";
    else if (c == '<') result += "&lt;";
    else if (c == '>') result += "&gt;";
    else if (c == '"') result += "&quot;";
    else if (c == '\'') result += "&#39;";
    else result += c;
  }
  return result;
}

String getCsvField(String value, int fieldIndex) {
  value.trim();
  int colon = value.indexOf(':');
  if (colon >= 0) value = value.substring(colon + 1);

  int start = 0;
  bool inQuote = false;
  int current = 0;
  for (int i = 0; i <= value.length(); i++) {
    char c = (i < value.length()) ? value.charAt(i) : ',';
    if (c == '"') inQuote = !inQuote;
    if ((c == ',' && !inQuote) || i == value.length()) {
      if (current == fieldIndex) {
        String field = value.substring(start, i);
        field.trim();
        if (field.startsWith("\"") && field.endsWith("\"") && field.length() >= 2) {
          field = field.substring(1, field.length() - 1);
        }
        return field;
      }
      current++;
      start = i + 1;
    }
  }
  return "";
}

String smsStatusText(int status) {
  if (status == 0) return "未读";
  if (status == 1) return "已读";
  if (status == 2) return "未发送";
  if (status == 3) return "已发送";
  return "未知";
}

String smsStatusClass(int status) {
  return status == 0 ? "sms-unread" : "sms-read";
}

bool isLikelyPduLine(const String& line) {
  if (line.length() < 10) return false;
  for (unsigned int i = 0; i < line.length(); i++) {
    char c = line.charAt(i);
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

struct SmsListEntry {
  int index;
  int status;
  int tpduLength;
};

#define SMS_LIST_MAX_LIMIT 100

static SmsListEntry smsListEntries[SMS_LIST_MAX_LIMIT];

int getSmsListLimit() {
  int limit = server.arg("limit").toInt();
  if (limit <= 0) limit = 20;
  if (limit < 1) limit = 1;
  if (limit > SMS_LIST_MAX_LIMIT) limit = SMS_LIST_MAX_LIMIT;
  return limit;
}

bool decodeStoredSmsPdu(const String& pduLine, String& sender, String& text, String& timestamp) {
  if (!pdu.decodePDU(pduLine.c_str())) return false;
  sender = String(pdu.getSender());
  text = String(pdu.getText());
  timestamp = String(pdu.getTimeStamp());
  return true;
}

SmsStorageInfo parseSmsStorageInfo(const String& resp) {
  SmsStorageInfo info = {false, 0, 0};

  int lineStart = 0;
  for (int i = 0; i <= resp.length(); i++) {
    if (i == resp.length() || resp.charAt(i) == '\n') {
      String line = resp.substring(lineStart, i);
      line.trim();
      if (line.startsWith("+CPMS:")) {
        info.used = getCsvField(line, 0).toInt();
        info.total = getCsvField(line, 1).toInt();
        info.valid = info.total > 0;
        return info;
      }
      lineStart = i + 1;
    }
  }

  return info;
}

SmsStorageInfo selectSmsStorage() {
  String cmd = "AT+CPMS=\"" + String(SMS_STORAGE) + "\",\"" + String(SMS_STORAGE) + "\",\"" + String(SMS_STORAGE) + "\"";
  String resp = sendATCommand(cmd.c_str(), 5000);
  Serial.println("CPMS响应: " + resp);
  return parseSmsStorageInfo(resp);
}

String smsStorageHtml(const SmsStorageInfo& info) {
  if (!info.valid) {
    return "<div>模块短信存储空间: 查询失败</div>";
  }

  int freeCount = info.total - info.used;
  if (freeCount < 0) freeCount = 0;

  String html = "<div>";
  html += "模块短信存储(" + String(SMS_STORAGE) + "): 已用 " + String(info.used);
  html += " / 总容量 " + String(info.total);
  html += " | 剩余 " + String(freeCount);
  html += " | 目标保留空位 " + String(SMS_STORAGE_RESERVED_FREE);
  html += "</div>";
  return html;
}

void appendSmsCard(String& html, const String& indexLabel, int status, const String& sender, const String& timestamp, const String& text) {
  String displayTimestamp = timestamp.length() ? timestamp : String("-");
  String displaySender = sender.length() ? sender : String("-");
  String displayText = text.length() ? text : String("(空短信)");

  html += "<div class='sms-card'>";
  html += "<div class='sms-meta'>";
  html += "<span class='sms-badge " + smsStatusClass(status) + "'>" + smsStatusText(status) + "</span>";
  html += "索引: " + htmlEscape(indexLabel);
  html += " | 收信时间: " + htmlEscape(displayTimestamp);
  html += " | 发信人: " + htmlEscape(displaySender);
  html += "</div>";
  html += "<div class='sms-body'>" + htmlEscape(displayText) + "</div>";
  html += "</div>";
}

int findSmsListConcatGroup(SmsListConcatGroup groups[], int refNumber, const String& sender, int totalParts) {
  for (int i = 0; i < MAX_SMS_LIST_CONCAT_GROUPS; i++) {
    if (groups[i].inUse &&
        groups[i].refNumber == refNumber &&
        groups[i].sender == sender &&
        groups[i].totalParts == totalParts) {
      return i;
    }
  }

  for (int i = 0; i < MAX_SMS_LIST_CONCAT_GROUPS; i++) {
    if (!groups[i].inUse) {
      groups[i].inUse = true;
      groups[i].refNumber = refNumber;
      groups[i].sender = sender;
      groups[i].timestamp = "";
      groups[i].totalParts = totalParts;
      groups[i].receivedParts = 0;
      groups[i].status = 1;
      groups[i].indexes = "";
      for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
        groups[i].parts[j] = "";
        groups[i].partValid[j] = false;
      }
      return i;
    }
  }

  return -1;
}

String assembleSmsListConcatText(const SmsListConcatGroup& group) {
  String text = "";
  for (int i = 0; i < group.totalParts && i < MAX_CONCAT_PARTS; i++) {
    if (group.partValid[i]) {
      text += group.parts[i];
    } else {
      text += "[缺失分段" + String(i + 1) + "]";
    }
  }
  return text;
}

void resetSmsListConcatGroups(SmsListConcatGroup groups[]) {
  for (int i = 0; i < MAX_SMS_LIST_CONCAT_GROUPS; i++) {
    groups[i].inUse = false;
    groups[i].refNumber = 0;
    groups[i].sender = "";
    groups[i].timestamp = "";
    groups[i].totalParts = 0;
    groups[i].receivedParts = 0;
    groups[i].status = 1;
    groups[i].indexes = "";
    for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
      groups[i].parts[j] = "";
      groups[i].partValid[j] = false;
    }
  }
}

void appendSmsListConcatGroups(String& html, SmsListConcatGroup groups[], int& count) {
  for (int i = 0; i < MAX_SMS_LIST_CONCAT_GROUPS; i++) {
    if (!groups[i].inUse) continue;

    String text = assembleSmsListConcatText(groups[i]);
    if (groups[i].receivedParts < groups[i].totalParts) {
      text = "[长短信未收齐 " + String(groups[i].receivedParts) + "/" + String(groups[i].totalParts) + "] " + text;
    }
    appendSmsCard(html, groups[i].indexes, groups[i].status, groups[i].sender, groups[i].timestamp, text);
    count++;
  }
}

void collectSmsIndexFromCmgl(const String& resp, int indexes[], int& indexCount) {
  indexCount = 0;
  int lineStart = 0;
  for (int i = 0; i <= resp.length(); i++) {
    if (i == resp.length() || resp.charAt(i) == '\n') {
      String line = resp.substring(lineStart, i);
      line.trim();
      if (line.startsWith("+CMGL:") && indexCount < MAX_SMS_CLEANUP_INDEXES) {
        indexes[indexCount++] = getCsvField(line, 0).toInt();
      }
      lineStart = i + 1;
    }
  }
}

void appendLatestSmsListEntry(const SmsListEntry& entry, int limit, SmsListEntry entries[], int& entryCount) {
  if (limit <= 0) return;

  if (entryCount < limit) {
    entries[entryCount++] = entry;
    return;
  }

  for (int i = 1; i < limit; i++) {
    entries[i - 1] = entries[i];
  }
  entries[limit - 1] = entry;
}

bool collectSmsEntriesFromCmgl(int limit, SmsListEntry entries[], int& entryCount, int& scannedCount) {
  entryCount = 0;
  scannedCount = 0;
  while (Serial1.available()) Serial1.read();
  Serial1.println("AT+CMGL=4");

  unsigned long start = millis();
  String line = "";
  bool lineOverflow = false;

  while (millis() - start < 15000) {
    while (Serial1.available()) {
      char c = Serial1.read();
      if (c == '\r') continue;

      if (c == '\n') {
        line.trim();
        if (line.length() > 0) {
          if (line == "OK") {
            Serial.println("CMGL索引扫描完成，已扫描数量: " + String(scannedCount) + "，保留数量: " + String(entryCount));
            return true;
          }
          if (line.indexOf("ERROR") >= 0) {
            Serial.println("CMGL索引扫描失败: " + line);
            return false;
          }
          if (line.startsWith("+CMGL:")) {
            SmsListEntry entry;
            entry.index = getCsvField(line, 0).toInt();
            entry.status = getCsvField(line, 1).toInt();
            entry.tpduLength = getCsvField(line, 3).toInt();
            if (entry.index > 0) {
              scannedCount++;
              appendLatestSmsListEntry(entry, limit, entries, entryCount);
            }
          }
        }
        line = "";
        lineOverflow = false;
      } else if (!lineOverflow) {
        if (line.length() < SERIAL_BUFFER_SIZE - 1) {
          line += c;
        } else {
          lineOverflow = true;
          line = "";
        }
      }
    }
    delay(1);
  }

  Serial.println("CMGL索引扫描超时，已扫描数量: " + String(scannedCount) + "，保留数量: " + String(entryCount));
  return entryCount > 0;
}

bool readStoredSmsPduLineByIndex(int smsIndex, String& pduLine) {
  String resp = sendATCommand(("AT+CMGR=" + String(smsIndex)).c_str(), 5000);
  Serial.println("CMGR索引 " + String(smsIndex) + " 响应长度: " + String(resp.length()) + " 字符");

  int lineStart = 0;
  for (int i = 0; i <= resp.length(); i++) {
    if (i == resp.length() || resp.charAt(i) == '\n') {
      String line = resp.substring(lineStart, i);
      line.trim();
      if (isLikelyPduLine(line)) {
        pduLine = line;
        return true;
      }
      lineStart = i + 1;
    }
  }

  pduLine = "";
  return false;
}

void appendDecodedSmsFromPdu(String& html, const String& indexLabel, int status, const String& pduLine, SmsListConcatGroup concatGroups[], int& count) {
  String sender, text, timestamp;
  if (decodeStoredSmsPdu(pduLine, sender, text, timestamp)) {
    String displayTimestamp = formatSmsTimestamp(timestamp);
    int* concatInfo = pdu.getConcatInfo();
    int refNumber = concatInfo[0];
    int partNumber = concatInfo[1];
    int totalParts = concatInfo[2];

    if (totalParts > 1 && partNumber > 0 && totalParts <= MAX_CONCAT_PARTS) {
      int groupIndex = findSmsListConcatGroup(concatGroups, refNumber, sender, totalParts);
      if (groupIndex >= 0) {
        SmsListConcatGroup& group = concatGroups[groupIndex];
        int partIndex = partNumber - 1;
        if (!group.partValid[partIndex]) {
          group.partValid[partIndex] = true;
          group.parts[partIndex] = text;
          group.receivedParts++;
        }
        if (group.timestamp.length() == 0) group.timestamp = displayTimestamp;
        if (status == 0) group.status = 0;
        if (group.indexes.length() > 0) group.indexes += ",";
        group.indexes += indexLabel;
      } else {
        appendSmsCard(html, indexLabel, status, sender, displayTimestamp, text);
        count++;
      }
    } else {
      appendSmsCard(html, indexLabel, status, sender, displayTimestamp, text);
      count++;
    }
  } else {
    html += "<div class='sms-card'><div class='sms-meta'>索引: " + htmlEscape(indexLabel) + " | 解码失败</div>";
    html += "<div class='sms-body'>" + htmlEscape(pduLine) + "</div></div>";
    count++;
  }
}

void cleanupSmsStorageReserve() {
  SmsStorageInfo info = selectSmsStorage();
  if (!info.valid) {
    Serial.println("短信存储空间查询失败，跳过清理");
    return;
  }

  int freeCount = info.total - info.used;
  if (freeCount >= SMS_STORAGE_RESERVED_FREE) {
    Serial.println("模块短信存储空位充足: " + String(freeCount));
    return;
  }

  int deleteNeeded = SMS_STORAGE_RESERVED_FREE - freeCount;
  Serial.println("模块短信存储空位不足，准备清理 " + String(deleteNeeded) + " 条短信");

  sendATCommand("AT+CMGF=0", 2000);
  String resp = sendATCommand("AT+CMGL=4", 15000);
  int indexes[MAX_SMS_CLEANUP_INDEXES];
  int indexCount = 0;
  collectSmsIndexFromCmgl(resp, indexes, indexCount);

  int deleted = 0;
  for (int i = 0; i < indexCount && deleted < deleteNeeded; i++) {
    if (indexes[i] <= 0) continue;
    String cmd = "AT+CMGD=" + String(indexes[i]);
    String delResp = sendATCommand(cmd.c_str(), 5000);
    if (delResp.indexOf("OK") >= 0) {
      deleted++;
      Serial.println("已删除模块短信索引: " + String(indexes[i]));
    } else {
      Serial.println("删除模块短信失败，索引: " + String(indexes[i]) + " 响应: " + delResp);
    }
  }

  Serial.println("模块短信清理完成，已删除 " + String(deleted) + " 条");
}

void handleSmsList() {
  if (!checkAuth()) return;

  Serial.println("网页端读取短信索引列表");
  SmsStorageInfo storageInfo = selectSmsStorage();
  sendATCommand("AT+CMGF=0", 2000);
  int limit = getSmsListLimit();
  int entryCount = 0;
  int scannedCount = 0;
  bool success = collectSmsEntriesFromCmgl(limit, smsListEntries, entryCount, scannedCount);

  String json = "{";
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"storageHtml\":\"" + jsonEscape(smsStorageHtml(storageInfo)) + "\",";
  json += "\"limit\":" + String(limit) + ",";
  json += "\"scanned\":" + String(scannedCount) + ",";
  json += "\"count\":" + String(entryCount) + ",";
  json += "\"entries\":[";
  for (int i = 0; i < entryCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"index\":" + String(smsListEntries[i].index) + ",";
    json += "\"status\":" + String(smsListEntries[i].status) + ",";
    json += "\"tpduLength\":" + String(smsListEntries[i].tpduLength);
    json += "}";
  }
  json += "]";
  json += "}";
  server.send(200, "application/json", json);
}

void handleSmsPdu() {
  if (!checkAuth()) return;

  int smsIndex = server.arg("index").toInt();
  if (smsIndex <= 0) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"invalid index\"}");
    return;
  }

  String pduLine;
  bool success = readStoredSmsPduLineByIndex(smsIndex, pduLine);

  String json = "{";
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"index\":" + String(smsIndex) + ",";
  if (success) {
    json += "\"pdu\":\"" + pduLine + "\"";
  } else {
    json += "\"message\":\"CMGR did not return a valid PDU\"";
  }
  json += "}";
  server.send(200, "application/json", json);
}
