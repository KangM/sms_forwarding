#pragma once

// 邮件通知发送函数。
// 发送邮件通知函数
void sendEmailNotification(const char* subject, const char* body) {
  if (config.smtpServer.length() == 0 || config.smtpUser.length() == 0 ||
      config.smtpPass.length() == 0 || config.smtpSendTo.length() == 0) {
    systemLogPrintln(LOG_LEVEL_WARN, LOG_MODULE_SYSTEM, "email config incomplete, skip send");
    return;
  }

  auto statusCallback = [](SMTPStatus status) {
    systemLogSerialOnly(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "smtp status: " + String(status.text));
  };
  smtp.connect(config.smtpServer.c_str(), config.smtpPort, statusCallback);
  if (smtp.isConnected()) {
    smtp.authenticate(config.smtpUser.c_str(), config.smtpPass.c_str(), readymail_auth_password);

    SMTPMessage msg;
    String from = "sms notify <"; from += config.smtpUser; from += ">";
    msg.headers.add(rfc822_from, from.c_str());
    String to = "your_email <"; to += config.smtpSendTo; to += ">";
    msg.headers.add(rfc822_to, to.c_str());
    msg.headers.add(rfc822_subject, subject);
    msg.text.body(body);
    msg.timestamp = time(nullptr);
    smtp.send(msg);
    systemLogPrintln(LOG_LEVEL_INFO, LOG_MODULE_SYSTEM, "email send complete");
  } else {
    systemLogPrintln(LOG_LEVEL_ERROR, LOG_MODULE_SYSTEM, "email server connect failed");
  }
}
