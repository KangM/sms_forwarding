#pragma once

// Web 配置保存处理函数。
// 这些函数定义在 net_task.h（包含顺序在本文件之后），此处前置声明。
void lockConfig();
void unlockConfig();
bool enqueueEmailNotify(const String& subject, const String& body);

// 处理保存配置请求
void handleSave() {
  if (!checkAuth()) return;

  // 获取新的Web账号密码
  String newWebUser = server.arg("webUser");
  String newWebPass = server.arg("webPass");

  // 验证Web账号密码不能为空
  if (newWebUser.length() == 0) newWebUser = DEFAULT_WEB_USER;
  if (newWebPass.length() == 0) newWebPass = DEFAULT_WEB_PASS;

  // 写 config 期间加锁，避免与网络任务读取 config 并发
  lockConfig();
  config.webUser = newWebUser;
  config.webPass = newWebPass;
  config.smtpServer = server.arg("smtpServer");
  config.smtpPort = server.arg("smtpPort").toInt();
  if (config.smtpPort == 0) config.smtpPort = 465;
  config.smtpUser = server.arg("smtpUser");
  config.smtpPass = server.arg("smtpPass");
  config.smtpSendTo = server.arg("smtpSendTo");
  config.startupMailEnabled = server.arg("startupMail") == "on";
  config.adminPhone = server.arg("adminPhone");
  config.numberBlackList = server.arg("numberBlackList");

  // 掉线检测配置
  config.keepAliveEnabled = server.arg("kaEnabled") == "on";
  config.keepAliveUrl = server.arg("kaUrl");
  config.keepAliveMethod = server.arg("kaMethod") == "POST" ? "POST" : "GET";
  config.keepAliveBody = server.arg("kaBody");
  config.keepAliveInterval = server.arg("kaInterval").toInt();
  if (config.keepAliveInterval < KEEP_ALIVE_MIN_INTERVAL) {
    config.keepAliveInterval = KEEP_ALIVE_DEFAULT_INTERVAL;
  }

  // 保存推送通道配置
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    String idx = String(i);
    config.pushChannels[i].enabled = server.arg("push" + idx + "en") == "on";
    config.pushChannels[i].type = (PushType)server.arg("push" + idx + "type").toInt();
    config.pushChannels[i].url = server.arg("push" + idx + "url");
    config.pushChannels[i].name = server.arg("push" + idx + "name");
    config.pushChannels[i].key1 = server.arg("push" + idx + "key1");
    config.pushChannels[i].key2 = server.arg("push" + idx + "key2");
    config.pushChannels[i].customBody = server.arg("push" + idx + "body");
    if (config.pushChannels[i].name.length() == 0) {
      config.pushChannels[i].name = "通道" + String(i + 1);
    }
  }

  saveConfig();
  unlockConfig();
  printConfigValidationResult("保存配置");

  // 保存后未处于配网状态时，按最新配置有效性更新 LED 常态
  if (!wifiConfigPortalActive) {
    ledSetNormalState(configValid ? LED_RUNNING_IDLE : LED_ERROR);
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta http-equiv="refresh" content="3;url=/">
  <title>保存成功</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; padding-top: 100px; background: #f5f5f5; }
    .success { background: #4CAF50; color: white; padding: 20px; border-radius: 10px; display: inline-block; }
  </style>
</head>
<body>
  <div class="success">
    <h2>✅ 配置保存成功！</h2>
    <p>3秒后返回配置页面...</p>
    <p>如果修改了账号密码，请使用新的账号密码登录</p>
  </div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);

  // 如果配置有效，发送配置更新邮件通知（投递到后台任务，避免阻塞Web响应）
  if (configValid) {
    Serial.println("配置有效，排队发送配置更新邮件通知...");
    String subject = "短信转发器配置已更新";
    String body = "设备配置已更新\n设备地址: " + getDeviceUrl();
    enqueueEmailNotify(subject, body);
  }
}
