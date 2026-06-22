#pragma once

// Single owner for the modem UART.
// All Serial1 reads/writes must go through this module so async URCs such as
// +CMT/+CMTI cannot be accidentally cleared by diagnostics or web handlers.

enum ModemAtResult {
  MODEM_AT_OK,
  MODEM_AT_ERROR,
  MODEM_AT_TIMEOUT,
  MODEM_AT_QUEUE_FULL
};

typedef void (*ModemAtCallback)(ModemAtResult result, const String& response, void* userData);
typedef void (*ModemUrcCallback)(const String& line, void* userData);

struct ModemAtJob {
  bool inUse;
  String command;
  unsigned long timeoutMs;
  String waitFor;
  ModemAtCallback callback;
  void* userData;
};

#define MODEM_AT_QUEUE_LEN 8
#define MODEM_AT_LINE_MAX SERIAL_BUFFER_SIZE
#define MODEM_AT_RESPONSE_MAX 4096

ModemAtJob modemAtQueue[MODEM_AT_QUEUE_LEN];
int modemAtQueueHead = 0;
int modemAtQueueCount = 0;
bool modemAtActive = false;
ModemAtJob modemAtCurrent;
String modemAtResponse = "";
String modemAtLine = "";
bool modemAtLineOverflow = false;
bool modemAtAwaitingCmtPdu = false;
unsigned long modemAtDeadlineMs = 0;
ModemUrcCallback modemAtUrcCallback = nullptr;
void* modemAtUrcUserData = nullptr;

void modemAtPoll();

bool modemAtStartsWith(const String& value, const char* prefix) {
  return value.startsWith(prefix);
}

bool modemAtIsTerminalError(const String& line) {
  return line.startsWith("+CME ERROR:") || line.startsWith("+CMS ERROR:") || line == "ERROR";
}

bool modemAtIsUrcLine(const String& line) {
  return line.startsWith("+CMT:") ||
         line.startsWith("+CMTI:") ||
         line.startsWith("+CDS:") ||
         line.startsWith("+CEREG:") ||
         line.startsWith("+CPIN:") ||
         line == "+MATREADY";
}

bool modemAtCurrentExpectsLine(const String& line) {
  if (!modemAtActive) return false;
  if (line.startsWith("+CEREG:") && modemAtCurrent.command.startsWith("AT+CEREG?")) return true;
  if (line.startsWith("+CPMS:") && modemAtCurrent.command.startsWith("AT+CPMS")) return true;
  if (line.startsWith("+CFUN:") && modemAtCurrent.command.startsWith("AT+CFUN?")) return true;
  if (line.startsWith("+CESQ:") && modemAtCurrent.command.startsWith("AT+CESQ")) return true;
  if (line.startsWith("+CSQ:") && modemAtCurrent.command.startsWith("AT+CSQ")) return true;
  if (line.startsWith("+CIMI") && modemAtCurrent.command.startsWith("AT+CIMI")) return true;
  if (line.startsWith("+ICCID:") && modemAtCurrent.command.startsWith("AT+ICCID")) return true;
  if (line.startsWith("+CNUM:") && modemAtCurrent.command.startsWith("AT+CNUM")) return true;
  if (line.startsWith("+COPS:") && modemAtCurrent.command.startsWith("AT+COPS?")) return true;
  if (line.startsWith("+CGACT:") && modemAtCurrent.command.startsWith("AT+CGACT?")) return true;
  if (line.startsWith("+CGDCONT:") && modemAtCurrent.command.startsWith("AT+CGDCONT?")) return true;
  if (line.startsWith("+CMGR:") && modemAtCurrent.command.startsWith("AT+CMGR=")) return true;
  if (line.startsWith("+CMGL:") && modemAtCurrent.command.startsWith("AT+CMGL=")) return true;
  if (line.startsWith("+CMGS:") && modemAtCurrent.command.startsWith("AT+CMGS=")) return true;
  if (line.startsWith("+MPING:") && modemAtCurrent.command.startsWith("AT+MPING=")) return true;
  return false;
}

void modemAtAppendResponseLine(const String& line) {
  if (modemAtResponse.length() + line.length() + 1 >= MODEM_AT_RESPONSE_MAX) return;
  modemAtResponse += line;
  modemAtResponse += '\n';
}

void modemAtEmitUrc(const String& line) {
  if (modemAtUrcCallback) {
    modemAtUrcCallback(line, modemAtUrcUserData);
  } else {
    systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_MODEM, "modem urc=" + line);
  }
}

void modemAtFinishCurrent(ModemAtResult result) {
  ModemAtCallback callback = modemAtCurrent.callback;
  void* userData = modemAtCurrent.userData;
  String response = modemAtResponse;
  modemAtActive = false;
  modemAtCurrent.inUse = false;
  modemAtResponse = "";

  if (callback) {
    callback(result, response, userData);
  }
}

void modemAtHandleLine(const String& rawLine) {
  String line = rawLine;
  line.trim();
  if (line.length() == 0) return;

  if (modemAtAwaitingCmtPdu) {
    modemAtAwaitingCmtPdu = false;
    modemAtEmitUrc(line);
    return;
  }

  if (modemAtIsUrcLine(line) && !modemAtCurrentExpectsLine(line)) {
    modemAtEmitUrc(line);
    if (line.startsWith("+CMT:")) modemAtAwaitingCmtPdu = true;
    return;
  }

  if (modemAtActive) {
    modemAtAppendResponseLine(line);
    if (modemAtCurrent.waitFor.length() > 0 && modemAtResponse.indexOf(modemAtCurrent.waitFor) >= 0) {
      modemAtFinishCurrent(MODEM_AT_OK);
      return;
    }
    if (line == "OK") {
      if (modemAtCurrent.waitFor.length() == 0) modemAtFinishCurrent(MODEM_AT_OK);
      return;
    }
    if (modemAtIsTerminalError(line)) {
      modemAtFinishCurrent(MODEM_AT_ERROR);
      return;
    }
    return;
  }

  modemAtEmitUrc(line);
}

void modemAtReadSerial() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (!modemAtLineOverflow) {
        modemAtHandleLine(modemAtLine);
      }
      modemAtLine = "";
      modemAtLineOverflow = false;
      continue;
    }

    if (modemAtLine.length() < MODEM_AT_LINE_MAX - 1) {
      modemAtLine += c;
    } else {
      modemAtLine = "";
      modemAtLineOverflow = true;
    }
  }
}

void modemAtStartNext() {
  if (modemAtActive || modemAtQueueCount <= 0) return;

  modemAtCurrent = modemAtQueue[modemAtQueueHead];
  modemAtQueue[modemAtQueueHead].inUse = false;
  modemAtQueueHead = (modemAtQueueHead + 1) % MODEM_AT_QUEUE_LEN;
  modemAtQueueCount--;

  modemAtActive = true;
  modemAtResponse = "";
  modemAtDeadlineMs = millis() + modemAtCurrent.timeoutMs;
  Serial1.println(modemAtCurrent.command);
}

void modemAtBegin() {
  Serial1.begin(115200, SERIAL_8N1, RXD, TXD);
  Serial1.setRxBufferSize(SERIAL_BUFFER_SIZE);
  modemAtQueueHead = 0;
  modemAtQueueCount = 0;
  modemAtActive = false;
  modemAtLine = "";
  modemAtLineOverflow = false;
  modemAtAwaitingCmtPdu = false;
}

void modemAtPoll() {
  modemAtReadSerial();
  if (modemAtActive && (long)(millis() - modemAtDeadlineMs) >= 0) {
    modemAtFinishCurrent(MODEM_AT_TIMEOUT);
  }
  modemAtStartNext();
}

void modemAtSetUrcCallback(ModemUrcCallback callback, void* userData) {
  modemAtUrcCallback = callback;
  modemAtUrcUserData = userData;
}

bool modemAtSubmit(const String& command, unsigned long timeoutMs, ModemAtCallback callback, void* userData, const String& waitFor = "") {
  if (command.length() == 0 || modemAtQueueCount >= MODEM_AT_QUEUE_LEN) return false;

  int index = (modemAtQueueHead + modemAtQueueCount) % MODEM_AT_QUEUE_LEN;
  modemAtQueue[index].inUse = true;
  modemAtQueue[index].command = command;
  modemAtQueue[index].timeoutMs = timeoutMs;
  modemAtQueue[index].waitFor = waitFor;
  modemAtQueue[index].callback = callback;
  modemAtQueue[index].userData = userData;
  modemAtQueueCount++;
  return true;
}

bool modemAtIsBusy() {
  return modemAtActive;
}

int modemAtQueueDepth() {
  return modemAtQueueCount;
}

struct ModemAtSyncContext {
  bool done;
  ModemAtResult result;
  String response;
};

void modemAtSyncCallback(ModemAtResult result, const String& response, void* userData) {
  ModemAtSyncContext* ctx = (ModemAtSyncContext*)userData;
  ctx->result = result;
  ctx->response = response;
  ctx->done = true;
}

String modemAtSendCommandSync(const String& command, unsigned long timeoutMs, const String& waitFor = "") {
  ModemAtSyncContext ctx;
  ctx.done = false;
  ctx.result = MODEM_AT_TIMEOUT;
  ctx.response = "";

  if (!modemAtSubmit(command, timeoutMs, modemAtSyncCallback, &ctx, waitFor)) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "AT queue full command=" + command);
    return "";
  }

  unsigned long deadline = millis() + timeoutMs + 1000;
  while (!ctx.done && (long)(millis() - deadline) < 0) {
    modemAtPoll();
    ledTick();
    yield();
    delay(1);
  }

  if (!ctx.done) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_MODEM, "AT sync wait expired command=" + command);
  }
  return ctx.response;
}

bool modemAtSendPduSmsSync(const String& phoneNumber, const String& encodedPdu, int pduLen, unsigned long timeoutMs, String& response) {
  if (modemAtActive || modemAtQueueCount > 0) {
    unsigned long waitDeadline = millis() + 5000;
    while ((modemAtActive || modemAtQueueCount > 0) && (long)(millis() - waitDeadline) < 0) {
      modemAtPoll();
      yield();
      delay(1);
    }
  }

  if (modemAtActive || modemAtQueueCount > 0) {
    response = "modem busy";
    return false;
  }

  modemAtReadSerial();
  String cmgsCmd = "AT+CMGS=" + String(pduLen);
  Serial1.println(cmgsCmd);

  unsigned long promptDeadline = millis() + 5000;
  bool gotPrompt = false;
  while ((long)(millis() - promptDeadline) < 0) {
    while (Serial1.available() > 0) {
      char c = (char)Serial1.read();
      if (c == '>') {
        gotPrompt = true;
        break;
      }
      if (c == '\n') {
        modemAtHandleLine(modemAtLine);
        modemAtLine = "";
      } else if (c != '\r') {
        modemAtLine += c;
      }
    }
    if (gotPrompt) break;
    yield();
    delay(1);
  }

  if (!gotPrompt) {
    response = "missing prompt";
    return false;
  }

  Serial1.print(encodedPdu);
  Serial1.write(0x1A);

  response = "";
  unsigned long deadline = millis() + timeoutMs;
  while ((long)(millis() - deadline) < 0) {
    while (Serial1.available() > 0) {
      char c = (char)Serial1.read();
      if (c == '\r') continue;
      if (c == '\n') {
        String line = modemAtLine;
        modemAtLine = "";
        line.trim();
        if (line.length() == 0) continue;
        response += line + "\n";
        if (line.startsWith("+CMT:") || line.startsWith("+CMTI:")) {
          modemAtHandleLine(line);
          continue;
        }
        if (line == "OK") return true;
        if (modemAtIsTerminalError(line)) return false;
      } else {
        if (modemAtLine.length() < MODEM_AT_LINE_MAX - 1) modemAtLine += c;
      }
    }
    yield();
    delay(1);
  }

  response += "timeout";
  return false;
}
