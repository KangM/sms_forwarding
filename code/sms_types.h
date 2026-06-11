#pragma once

#include <Arduino.h>

// 长短信合并相关定义
#define MAX_CONCAT_PARTS 10       // 最大支持的长短信分段数
#define CONCAT_TIMEOUT_MS 30000   // 长短信等待超时时间(毫秒)
#define MAX_CONCAT_MESSAGES 5     // 最多同时缓存的长短信组数
#define SMS_STORAGE "ME"          // 默认使用模块内部短信存储，避免占用SIM卡短信容量
#define SMS_STORAGE_RESERVED_FREE 20
#define MAX_SMS_CLEANUP_INDEXES 160
#define MAX_SMS_LIST_CONCAT_GROUPS 20

// 长短信分段结构
struct SmsPart {
  bool valid;           // 该分段是否有效
  String text;          // 分段内容
};

// 长短信缓存结构
struct ConcatSms {
  bool inUse;                           // 是否正在使用
  int refNumber;                        // 参考号
  String sender;                        // 发送者
  String timestamp;                     // 时间戳（使用第一个收到的分段的时间戳）
  int totalParts;                       // 总分段数
  int receivedParts;                    // 已收到的分段数
  unsigned long firstPartTime;          // 收到第一个分段的时间
  SmsPart parts[MAX_CONCAT_PARTS];      // 各分段内容
};

struct SmsStorageInfo {
  bool valid;
  int used;
  int total;
};

struct SmsListConcatGroup {
  bool inUse;
  int refNumber;
  String sender;
  String timestamp;
  int totalParts;
  int receivedParts;
  int status;
  String indexes;
  String parts[MAX_CONCAT_PARTS];
  bool partValid[MAX_CONCAT_PARTS];
};
