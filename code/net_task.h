#pragma once

// 网络后台任务：把会阻塞的 HTTP 推送、SMTP 邮件和掉线检测放到独立 FreeRTOS 任务，
// 避免阻塞主循环（收短信、Web 服务、串口 URC）。
// C3 单核也适用：任务在等待 socket/TLS 时会让出 CPU，主循环不会被整段卡死。

// 通知任务类型
struct NotifyJob {
  bool doPush;        // 是否推送到 HTTP 通道
  bool doEmail;       // 是否发邮件
  bool emailFromSms;  // 邮件内容是否由短信字段拼装（否则用 subject/body）
  String sender;
  String message;
  String timestamp;
  String subject;
  String body;
};

QueueHandle_t notifyQueue = nullptr;       // 元素为 NotifyJob*
SemaphoreHandle_t configMutex = nullptr;   // 保护 config 跨任务读写
TaskHandle_t netTaskHandle = nullptr;

#define NOTIFY_QUEUE_LEN 8

// config 跨任务访问锁：主循环（含Web处理）与网络任务共享 config。
// 注意：loop() 与 Web 处理函数同属一个任务，彼此不会并发；
// 只有网络任务读取 config 与 handleSave 写入 config 之间需要加锁。
void lockConfig() {
  if (configMutex) xSemaphoreTake(configMutex, portMAX_DELAY);
}
void unlockConfig() {
  if (configMutex) xSemaphoreGive(configMutex);
}

bool enqueueNotifyJob(NotifyJob* job) {
  if (!notifyQueue) {
    delete job;
    return false;
  }
  if (xQueueSend(notifyQueue, &job, 0) != pdTRUE) {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_SYSTEM, "notify queue full, drop job");
    delete job;
    return false;
  }
  return true;
}

// 短信通知：推送 + 邮件
bool enqueueSmsNotify(const String& sender, const String& message, const String& timestamp) {
  NotifyJob* job = new NotifyJob();
  job->doPush = true;
  job->doEmail = true;
  job->emailFromSms = true;
  job->sender = sender;
  job->message = message;
  job->timestamp = timestamp;
  return enqueueNotifyJob(job);
}

// 测试推送：仅推送，不发邮件
bool enqueueTestPush(const String& sender, const String& message, const String& timestamp) {
  NotifyJob* job = new NotifyJob();
  job->doPush = true;
  job->doEmail = false;
  job->emailFromSms = false;
  job->sender = sender;
  job->message = message;
  job->timestamp = timestamp;
  return enqueueNotifyJob(job);
}

// 普通邮件（如配置更新通知）
bool enqueueEmailNotify(const String& subject, const String& body) {
  NotifyJob* job = new NotifyJob();
  job->doPush = false;
  job->doEmail = true;
  job->emailFromSms = false;
  job->subject = subject;
  job->body = body;
  return enqueueNotifyJob(job);
}

static void processNotifyJob(NotifyJob* job) {
  lockConfig();
  if (job->doPush) {
    sendSMSToServer(job->sender.c_str(), job->message.c_str(), job->timestamp.c_str());
  }
  if (job->doEmail) {
    if (job->emailFromSms) {
      String subject = "短信" + job->sender + "," + job->message;
      String body = "来自：" + job->sender + "，时间：" + job->timestamp + "，内容：" + job->message;
      sendEmailNotification(subject.c_str(), body.c_str());
    } else {
      sendEmailNotification(job->subject.c_str(), job->body.c_str());
    }
  }
  unlockConfig();
}

void netTask(void* param) {
  systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "network task started");
  for (;;) {
    NotifyJob* job = nullptr;
    // 等待通知任务，最多阻塞 200ms，以便周期性执行掉线检测
    if (xQueueReceive(notifyQueue, &job, pdMS_TO_TICKS(200)) == pdTRUE && job) {
      processNotifyJob(job);
      delete job;
    }

    // 掉线检测（读取 config 需加锁）
    lockConfig();
    handleKeepAlive();
    unlockConfig();
  }
}

void startNetTask() {
  notifyQueue = xQueueCreate(NOTIFY_QUEUE_LEN, sizeof(NotifyJob*));
  configMutex = xSemaphoreCreateMutex();
  if (!notifyQueue || !configMutex) {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_SYSTEM, "queue or mutex creation failed, net task not started");
    return;
  }
  // 栈 8KB；HTTPS/TLS 较吃栈。不绑定核心，单核(C3)/双核(S3)均可。
  xTaskCreatePinnedToCore(netTask, "netTask", 8192, nullptr, 1, &netTaskHandle, tskNO_AFFINITY);
}
