#pragma once

#include <Arduino.h>

// 推送通道类型
enum PushType {
  PUSH_TYPE_NONE = 0,      // 未启用
  PUSH_TYPE_POST_JSON = 1, // POST JSON格式 {"sender":"xxx","message":"xxx","timestamp":"xxx"}
  PUSH_TYPE_BARK = 2,      // Bark格式 POST {"title":"xxx","body":"xxx"}
  PUSH_TYPE_GET = 3,       // GET请求，参数放URL中
  PUSH_TYPE_DINGTALK = 4,  // 钉钉机器人
  PUSH_TYPE_PUSHPLUS = 5,  // PushPlus
  PUSH_TYPE_SERVERCHAN = 6,// Server酱
  PUSH_TYPE_CUSTOM = 7,    // 自定义模板
  PUSH_TYPE_FEISHU = 8,    // 飞书机器人
  PUSH_TYPE_GOTIFY = 9,    // Gotify
  PUSH_TYPE_TELEGRAM = 10  // Telegram Bot
};

enum PushFilterTarget {
  PUSH_FILTER_TARGET_SENDER = 0,
  PUSH_FILTER_TARGET_MESSAGE = 1
};

enum PushFilterMode {
  PUSH_FILTER_MODE_CONTAINS = 0,
  PUSH_FILTER_MODE_NOT_CONTAINS = 1,
  PUSH_FILTER_MODE_STARTS_WITH = 2,
  PUSH_FILTER_MODE_ENDS_WITH = 3
};

// 最大推送通道数
#define MAX_PUSH_CHANNELS 5

// 推送通道配置（通用设计，支持多种推送方式）
struct PushChannel {
  bool enabled;           // 是否启用
  PushType type;          // 推送类型
  String name;            // 通道名称（用于显示）
  String url;             // 推送URL（webhook地址）
  String key1;            // 额外参数1（如：钉钉secret、pushplus token等）
  String key2;            // 额外参数2（备用）
  String customBody;      // 自定义请求体模板（使用 {sender} {message} {timestamp} 占位符）
};

// 配置参数结构体
struct Config {
  String smtpServer;
  int smtpPort;
  String smtpUser;
  String smtpPass;
  String smtpSendTo;
  bool startupMailEnabled;  // 启动后是否发送启动通知邮件
  String adminPhone;
  PushChannel pushChannels[MAX_PUSH_CHANNELS];  // 多推送通道
  bool pushFilterEnabled;             // 推送过滤开关
  PushFilterTarget pushFilterTarget;  // 匹配对象
  PushFilterMode pushFilterMode;      // 匹配方式
  String pushFilterExpr;              // 过滤表达式
  String webUser;      // Web管理账号
  String webPass;      // Web管理密码
  String numberBlackList;  // 号码黑名单（换行符分隔）
  // 掉线检测配置
  bool keepAliveEnabled;     // 功能开关（默认关）
  String keepAliveUrl;       // 请求URL
  String keepAliveMethod;    // 请求方式（GET/POST，默认GET）
  String keepAliveBody;      // 请求内容（POST时使用）
  int keepAliveInterval;     // 间隔时间（秒，默认5）
};

// 持续请求最小间隔（秒），防止误填过小值刷爆请求
#define KEEP_ALIVE_MIN_INTERVAL 1
// 持续请求默认间隔（秒）
#define KEEP_ALIVE_DEFAULT_INTERVAL 5

// 默认Web管理账号密码
#define DEFAULT_WEB_USER "admin"
#define DEFAULT_WEB_PASS "admin123"
