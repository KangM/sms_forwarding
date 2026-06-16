#pragma once

#include <SPIFFS.h>

enum SystemLogLevel {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR,
};

enum SystemLogModule {
  LOG_MODULE_SYSTEM = 0,
  LOG_MODULE_WIFI,
  LOG_MODULE_KEEPALIVE,
  LOG_MODULE_PUSH,
  LOG_MODULE_SMS,
  LOG_MODULE_MODEM,
  LOG_MODULE_NETDIAG,
  LOG_MODULE_SERIAL,
};

const size_t SYSTEM_LOG_RAM_CAPACITY = 200;
const size_t SYSTEM_LOG_DEFAULT_PRINT = 40;
const size_t SYSTEM_LOG_CONTEXT_BEFORE_ERROR = 20;
const size_t SYSTEM_LOG_CONTEXT_AFTER_ERROR = 40;
const unsigned long SYSTEM_LOG_CONTEXT_AFTER_ERROR_MS = 120000UL;
const size_t SYSTEM_LOG_MAX_MESSAGE_CHARS = 220;
const size_t SYSTEM_LOG_FLASH_MAX_BYTES = 65536;
const char* const SYSTEM_LOG_FLASH_PATH = "/syslog.log";
const char* const SYSTEM_LOG_FLASH_PREV_PATH = "/syslog.prev.log";
const size_t SYSTEM_LOG_FLASH_RESERVE_ERROR_CONTEXT_BYTES = 24576;
const long SYSTEM_LOG_LOCAL_TIME_OFFSET_SEC = 8L * 3600L;
bool systemLogSerialIncludeTime = false;

bool systemLogFlashReady = false;
String systemLogRing[SYSTEM_LOG_RAM_CAPACITY];
String systemLogRingMessage[SYSTEM_LOG_RAM_CAPACITY];
SystemLogLevel systemLogRingLevel[SYSTEM_LOG_RAM_CAPACITY];
SystemLogModule systemLogRingModule[SYSTEM_LOG_RAM_CAPACITY];
unsigned long systemLogRingUptimeMs[SYSTEM_LOG_RAM_CAPACITY];
time_t systemLogRingEpochSec[SYSTEM_LOG_RAM_CAPACITY];
size_t systemLogRingStart = 0;
size_t systemLogRingCount = 0;
bool systemLogFlashCaptureActive = false;
size_t systemLogFlashCaptureRemaining = 0;
unsigned long systemLogFlashCaptureUntilMs = 0;

String systemLogClockText() {
  unsigned long totalSeconds = millis() / 1000UL;
  unsigned long hours = totalSeconds / 3600UL;
  unsigned long minutes = (totalSeconds / 60UL) % 60UL;
  unsigned long seconds = totalSeconds % 60UL;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(buf);
}

const char* systemLogLevelText(SystemLogLevel level) {
  switch (level) {
    case LOG_LEVEL_DEBUG: return "DEBUG";
    case LOG_LEVEL_INFO: return "INFO";
    case LOG_LEVEL_WARN: return "WARN";
    case LOG_LEVEL_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

const char* systemLogModuleText(SystemLogModule module) {
  switch (module) {
    case LOG_MODULE_SYSTEM: return "SYSTEM";
    case LOG_MODULE_WIFI: return "WIFI";
    case LOG_MODULE_KEEPALIVE: return "KEEPALIVE";
    case LOG_MODULE_PUSH: return "PUSH";
    case LOG_MODULE_SMS: return "SMS";
    case LOG_MODULE_MODEM: return "MODEM";
    case LOG_MODULE_NETDIAG: return "NETDIAG";
    case LOG_MODULE_SERIAL: return "SERIAL";
    default: return "UNKNOWN";
  }
}

String sanitizeSystemLogMessage(const String& raw) {
  String msg = raw;
  msg.replace("\r", " ");
  msg.replace("\n", " ");
  if (msg.length() > SYSTEM_LOG_MAX_MESSAGE_CHARS) {
    msg = msg.substring(0, SYSTEM_LOG_MAX_MESSAGE_CHARS) + "...";
  }
  return msg;
}

String systemLogClockTextFromMillis(unsigned long uptimeMs) {
  unsigned long totalSeconds = uptimeMs / 1000UL;
  unsigned long hours = totalSeconds / 3600UL;
  unsigned long minutes = (totalSeconds / 60UL) % 60UL;
  unsigned long seconds = totalSeconds % 60UL;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(buf);
}

String formatSystemLogEntry(SystemLogLevel level, SystemLogModule module, const String& message) {
  return "[" + systemLogClockText() + "] [" + String(systemLogLevelText(level)) + "] [" +
         String(systemLogModuleText(module)) + "] " + sanitizeSystemLogMessage(message);
}

String formatSystemLogEntryFromUptime(unsigned long uptimeMs,
                                      SystemLogLevel level,
                                      SystemLogModule module,
                                      const String& message) {
  return "[" + systemLogClockTextFromMillis(uptimeMs) + "] [" + String(systemLogLevelText(level)) + "] [" +
         String(systemLogModuleText(module)) + "] " + sanitizeSystemLogMessage(message);
}

String systemLogRealTimeText(time_t epochSec) {
  if (epochSec < 100000) return "";

  time_t localEpochSec = epochSec + SYSTEM_LOG_LOCAL_TIME_OFFSET_SEC;
  struct tm timeInfo;
  if (gmtime_r(&localEpochSec, &timeInfo) == nullptr) return "";

  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeInfo);
  return String(buf);
}

String formatSystemLogFlashEntry(unsigned long uptimeMs,
                                 time_t epochSec,
                                 SystemLogLevel level,
                                 SystemLogModule module,
                                 const String& message) {
  String timestamp = systemLogRealTimeText(epochSec);
  if (timestamp.length() == 0) {
    timestamp = "uptime " + systemLogClockTextFromMillis(uptimeMs);
  }
  return "[" + timestamp + "] [" + String(systemLogLevelText(level)) + "] [" +
         String(systemLogModuleText(module)) + "] " + sanitizeSystemLogMessage(message);
}

String formatSystemLogSerialEntry(SystemLogLevel level, SystemLogModule module, const String& message) {
  String prefix = "[" + String(systemLogLevelText(level)) + "] [" + String(systemLogModuleText(module)) + "] ";
  if (systemLogSerialIncludeTime) {
    prefix = "[" + systemLogClockText() + "] " + prefix;
  }
  return prefix + sanitizeSystemLogMessage(message);
}

void appendSystemLogToRing(const String& entry) {
  if (SYSTEM_LOG_RAM_CAPACITY == 0) return;

  size_t index = (systemLogRingStart + systemLogRingCount) % SYSTEM_LOG_RAM_CAPACITY;
  systemLogRing[index] = entry;
  if (systemLogRingCount < SYSTEM_LOG_RAM_CAPACITY) {
    systemLogRingCount++;
  } else {
    systemLogRingStart = (systemLogRingStart + 1) % SYSTEM_LOG_RAM_CAPACITY;
  }
}

void appendSystemLogRecordToRing(unsigned long uptimeMs,
                                 time_t epochSec,
                                 SystemLogLevel level,
                                 SystemLogModule module,
                                 const String& message) {
  if (SYSTEM_LOG_RAM_CAPACITY == 0) return;

  size_t index = (systemLogRingStart + systemLogRingCount) % SYSTEM_LOG_RAM_CAPACITY;
  systemLogRing[index] = formatSystemLogEntryFromUptime(uptimeMs, level, module, message);
  systemLogRingMessage[index] = sanitizeSystemLogMessage(message);
  systemLogRingLevel[index] = level;
  systemLogRingModule[index] = module;
  systemLogRingUptimeMs[index] = uptimeMs;
  systemLogRingEpochSec[index] = epochSec;
  if (systemLogRingCount < SYSTEM_LOG_RAM_CAPACITY) {
    systemLogRingCount++;
  } else {
    systemLogRingStart = (systemLogRingStart + 1) % SYSTEM_LOG_RAM_CAPACITY;
  }
}

String getSystemLogRingEntry(size_t logicalIndex) {
  if (logicalIndex >= systemLogRingCount) return "";
  size_t index = (systemLogRingStart + logicalIndex) % SYSTEM_LOG_RAM_CAPACITY;
  return systemLogRing[index];
}

String getSystemLogRingFlashEntry(size_t logicalIndex) {
  if (logicalIndex >= systemLogRingCount) return "";
  size_t index = (systemLogRingStart + logicalIndex) % SYSTEM_LOG_RAM_CAPACITY;
  return formatSystemLogFlashEntry(systemLogRingUptimeMs[index],
                                   systemLogRingEpochSec[index],
                                   systemLogRingLevel[index],
                                   systemLogRingModule[index],
                                   systemLogRingMessage[index]);
}

void rotateSystemLogFlashIfNeeded(size_t incomingBytes) {
  if (!systemLogFlashReady) return;

  size_t currentSize = 0;
  File current = SPIFFS.open(SYSTEM_LOG_FLASH_PATH, "r");
  if (current) {
    currentSize = current.size();
    current.close();
  }

  if (currentSize + incomingBytes <= SYSTEM_LOG_FLASH_MAX_BYTES) return;

  if (SPIFFS.exists(SYSTEM_LOG_FLASH_PREV_PATH)) {
    SPIFFS.remove(SYSTEM_LOG_FLASH_PREV_PATH);
  }
  if (SPIFFS.exists(SYSTEM_LOG_FLASH_PATH)) {
    SPIFFS.rename(SYSTEM_LOG_FLASH_PATH, SYSTEM_LOG_FLASH_PREV_PATH);
  }
}

void reserveSystemLogFlashSpace(size_t reserveBytes) {
  if (!systemLogFlashReady) return;
  rotateSystemLogFlashIfNeeded(reserveBytes);
}

void appendSystemLogToFlash(const String& entry) {
  if (!systemLogFlashReady) return;
  rotateSystemLogFlashIfNeeded(entry.length() + 2);
  File file = SPIFFS.open(SYSTEM_LOG_FLASH_PATH, FILE_APPEND);
  if (!file) return;
  file.println(entry);
  file.close();
}

void persistSystemLogErrorContextBeforeLatestError() {
  if (!systemLogFlashReady) return;

  size_t endExclusive = systemLogRingCount > 0 ? systemLogRingCount - 1 : 0;
  size_t start = endExclusive > SYSTEM_LOG_CONTEXT_BEFORE_ERROR
                   ? endExclusive - SYSTEM_LOG_CONTEXT_BEFORE_ERROR
                   : 0;
  for (size_t i = start; i < endExclusive; i++) {
    appendSystemLogToFlash(getSystemLogRingFlashEntry(i));
  }
}

void writeSystemLogEntry(SystemLogLevel level, SystemLogModule module, const String& message, bool printToSerial) {
  unsigned long uptimeMs = millis();
  time_t epochSec = time(nullptr);
  appendSystemLogRecordToRing(uptimeMs, epochSec, level, module, message);
  if (printToSerial) {
    Serial.println(formatSystemLogSerialEntry(level, module, message));
  }

  String flashEntry = formatSystemLogFlashEntry(uptimeMs, epochSec, level, module, message);
  if (level == LOG_LEVEL_ERROR) {
    if (!systemLogFlashCaptureActive) {
      reserveSystemLogFlashSpace(SYSTEM_LOG_FLASH_RESERVE_ERROR_CONTEXT_BYTES);
      appendSystemLogToFlash("=== ERROR CONTEXT START ===");
      persistSystemLogErrorContextBeforeLatestError();
    }
    appendSystemLogToFlash(flashEntry);
    systemLogFlashCaptureActive = true;
    systemLogFlashCaptureRemaining = SYSTEM_LOG_CONTEXT_AFTER_ERROR;
    systemLogFlashCaptureUntilMs = millis() + SYSTEM_LOG_CONTEXT_AFTER_ERROR_MS;
    return;
  }

  if (systemLogFlashCaptureActive) {
    if (millis() <= systemLogFlashCaptureUntilMs && systemLogFlashCaptureRemaining > 0) {
      appendSystemLogToFlash(flashEntry);
      systemLogFlashCaptureRemaining--;
    }
    if (millis() > systemLogFlashCaptureUntilMs || systemLogFlashCaptureRemaining == 0) {
      appendSystemLogToFlash("=== ERROR CONTEXT END ===");
      systemLogFlashCaptureActive = false;
      systemLogFlashCaptureRemaining = 0;
      systemLogFlashCaptureUntilMs = 0;
    }
  }
}

void systemLog(SystemLogLevel level, SystemLogModule module, const String& message) {
  writeSystemLogEntry(level, module, message, false);
}

void systemLogPrintln(SystemLogLevel level, SystemLogModule module, const String& message) {
  writeSystemLogEntry(level, module, message, true);
}

void systemLogSerialOnly(SystemLogLevel level, SystemLogModule module, const String& message) {
  Serial.println(formatSystemLogSerialEntry(level, module, message));
}

void initSystemLog() {
  systemLogFlashReady = SPIFFS.begin(true);
  if (systemLogFlashReady) {
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    systemLog(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM,
              "flash log ready total=" + String(total) + " used=" + String(used));
  } else {
    systemLog(LOG_LEVEL_WARN, LOG_MODULE_SYSTEM, "flash log unavailable: SPIFFS mount failed");
  }
}

void printSystemLogRing(size_t limit = SYSTEM_LOG_DEFAULT_PRINT) {
  Serial.println("=== Runtime Log ===");
  Serial.println("Entries: " + String(systemLogRingCount));
  if (systemLogRingCount == 0) {
    Serial.println("No runtime logs.");
    Serial.println("===================");
    return;
  }

  size_t start = systemLogRingCount > limit ? systemLogRingCount - limit : 0;
  for (size_t i = start; i < systemLogRingCount; i++) {
    Serial.println(getSystemLogRingEntry(i));
  }
  Serial.println("===================");
}

void clearSystemLogRing() {
  systemLogRingStart = 0;
  systemLogRingCount = 0;
  for (size_t i = 0; i < SYSTEM_LOG_RAM_CAPACITY; i++) {
    systemLogRing[i] = "";
    systemLogRingMessage[i] = "";
    systemLogRingLevel[i] = LOG_LEVEL_INFO;
    systemLogRingModule[i] = LOG_MODULE_SYSTEM;
    systemLogRingUptimeMs[i] = 0;
    systemLogRingEpochSec[i] = 0;
  }
}

void printSystemLogFlash(const char* path, const String& title) {
  Serial.println(title);
  if (!systemLogFlashReady) {
    Serial.println("Flash log unavailable.");
    Serial.println("===================");
    return;
  }

  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.println("No flash log file.");
    Serial.println("===================");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) Serial.println(line);
  }
  file.close();
  Serial.println("===================");
}

void clearSystemLogFlash() {
  if (!systemLogFlashReady) return;
  if (SPIFFS.exists(SYSTEM_LOG_FLASH_PATH)) SPIFFS.remove(SYSTEM_LOG_FLASH_PATH);
  if (SPIFFS.exists(SYSTEM_LOG_FLASH_PREV_PATH)) SPIFFS.remove(SYSTEM_LOG_FLASH_PREV_PATH);
  systemLogFlashCaptureActive = false;
  systemLogFlashCaptureRemaining = 0;
  systemLogFlashCaptureUntilMs = 0;
}
