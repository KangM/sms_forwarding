#pragma once

// 配置保存、加载、校验和设备URL辅助函数。
String getDeviceUrl();

// 保存配置到NVS
void saveConfig() {
  preferences.begin("sms_config", false);
  preferences.putString("smtpServer", config.smtpServer);
  preferences.putInt("smtpPort", config.smtpPort);
  preferences.putString("smtpUser", config.smtpUser);
  preferences.putString("smtpPass", config.smtpPass);
  preferences.putString("smtpSendTo", config.smtpSendTo);
  preferences.putString("adminPhone", config.adminPhone);
  preferences.putString("webUser", config.webUser);
  preferences.putString("webPass", config.webPass);
  preferences.putString("numBlkList", config.numberBlackList);

  // 保存推送通道配置
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    String prefix = "push" + String(i);
    preferences.putBool((prefix + "en").c_str(), config.pushChannels[i].enabled);
    preferences.putUChar((prefix + "type").c_str(), (uint8_t)config.pushChannels[i].type);
    preferences.putString((prefix + "url").c_str(), config.pushChannels[i].url);
    preferences.putString((prefix + "name").c_str(), config.pushChannels[i].name);
    preferences.putString((prefix + "k1").c_str(), config.pushChannels[i].key1);
    preferences.putString((prefix + "k2").c_str(), config.pushChannels[i].key2);
    preferences.putString((prefix + "body").c_str(), config.pushChannels[i].customBody);
  }

  preferences.end();
  Serial.println("配置已保存");
}

// 从NVS加载配置
void loadConfig() {
  preferences.begin("sms_config", true);
  config.smtpServer = preferences.getString("smtpServer", "");
  config.smtpPort = preferences.getInt("smtpPort", 465);
  config.smtpUser = preferences.getString("smtpUser", "");
  config.smtpPass = preferences.getString("smtpPass", "");
  config.smtpSendTo = preferences.getString("smtpSendTo", "");
  config.adminPhone = preferences.getString("adminPhone", "");
  config.webUser = preferences.getString("webUser", DEFAULT_WEB_USER);
  config.webPass = preferences.getString("webPass", DEFAULT_WEB_PASS);
  config.numberBlackList = preferences.getString("numBlkList", "");

  // 加载推送通道配置
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    String prefix = "push" + String(i);
    config.pushChannels[i].enabled = preferences.getBool((prefix + "en").c_str(), false);
    config.pushChannels[i].type = (PushType)preferences.getUChar((prefix + "type").c_str(), PUSH_TYPE_POST_JSON);
    config.pushChannels[i].url = preferences.getString((prefix + "url").c_str(), "");
    config.pushChannels[i].name = preferences.getString((prefix + "name").c_str(), "通道" + String(i + 1));
    config.pushChannels[i].key1 = preferences.getString((prefix + "k1").c_str(), "");
    config.pushChannels[i].key2 = preferences.getString((prefix + "k2").c_str(), "");
    config.pushChannels[i].customBody = preferences.getString((prefix + "body").c_str(), "");
  }

  // 兼容旧配置：如果有旧的httpUrl配置，迁移到第一个通道
  String oldHttpUrl = preferences.getString("httpUrl", "");
  if (oldHttpUrl.length() > 0 && !config.pushChannels[0].enabled) {
    config.pushChannels[0].enabled = true;
    config.pushChannels[0].url = oldHttpUrl;
    config.pushChannels[0].type = preferences.getUChar("barkMode", 0) != 0 ? PUSH_TYPE_BARK : PUSH_TYPE_POST_JSON;
    config.pushChannels[0].name = "迁移通道";
    Serial.println("已迁移旧HTTP配置到推送通道1");
  }

  preferences.end();
  Serial.println("配置已加载");
}

// 检查推送通道是否有效配置
bool isPushChannelValid(const PushChannel& ch) {
  if (!ch.enabled) return false;

  switch (ch.type) {
    case PUSH_TYPE_POST_JSON:
    case PUSH_TYPE_BARK:
    case PUSH_TYPE_GET:
    case PUSH_TYPE_DINGTALK:
    case PUSH_TYPE_FEISHU:
      return ch.url.length() > 0;
    case PUSH_TYPE_CUSTOM:
      return ch.url.length() > 0 && ch.customBody.length() > 0;
    case PUSH_TYPE_PUSHPLUS:
    case PUSH_TYPE_SERVERCHAN:
      return ch.key1.length() > 0;  // 这两个主要靠key1（token/sendkey）
    case PUSH_TYPE_GOTIFY:
      return ch.url.length() > 0 && ch.key1.length() > 0;  // 需要URL和Token
    case PUSH_TYPE_TELEGRAM:
      return ch.key1.length() > 0 && ch.key2.length() > 0; // 需要Chat ID和Token
    default:
      return false;
  }
}

String pushTypeName(PushType type) {
  switch (type) {
    case PUSH_TYPE_POST_JSON: return "POST JSON";
    case PUSH_TYPE_BARK: return "Bark";
    case PUSH_TYPE_GET: return "GET请求";
    case PUSH_TYPE_DINGTALK: return "钉钉机器人";
    case PUSH_TYPE_PUSHPLUS: return "PushPlus";
    case PUSH_TYPE_SERVERCHAN: return "Server酱";
    case PUSH_TYPE_CUSTOM: return "自定义模板";
    case PUSH_TYPE_FEISHU: return "飞书机器人";
    case PUSH_TYPE_GOTIFY: return "Gotify";
    case PUSH_TYPE_TELEGRAM: return "Telegram Bot";
    default: return "未知类型";
  }
}

void appendMissingField(String& reason, const String& field) {
  if (reason.length() > 0) reason += "、";
  reason += field;
}

String getEmailInvalidReason() {
  String missing = "";
  if (config.smtpServer.length() == 0) appendMissingField(missing, "SMTP服务器");
  if (config.smtpUser.length() == 0) appendMissingField(missing, "邮箱账号");
  if (config.smtpPass.length() == 0) appendMissingField(missing, "邮箱密码/授权码");
  if (config.smtpSendTo.length() == 0) appendMissingField(missing, "收件邮箱");
  if (missing.length() == 0) return "";
  return "邮箱通知不完整，缺少：" + missing;
}

String getPushChannelInvalidReason(const PushChannel& ch) {
  if (!ch.enabled) return "";

  String missing = "";
  switch (ch.type) {
    case PUSH_TYPE_POST_JSON:
    case PUSH_TYPE_BARK:
    case PUSH_TYPE_GET:
    case PUSH_TYPE_DINGTALK:
    case PUSH_TYPE_FEISHU:
      if (ch.url.length() == 0) appendMissingField(missing, "URL/Webhook");
      break;
    case PUSH_TYPE_CUSTOM:
      if (ch.url.length() == 0) appendMissingField(missing, "URL/Webhook");
      if (ch.customBody.length() == 0) appendMissingField(missing, "请求体模板");
      break;
    case PUSH_TYPE_PUSHPLUS:
      if (ch.key1.length() == 0) appendMissingField(missing, "Token");
      break;
    case PUSH_TYPE_SERVERCHAN:
      if (ch.key1.length() == 0) appendMissingField(missing, "SendKey");
      break;
    case PUSH_TYPE_GOTIFY:
      if (ch.url.length() == 0) appendMissingField(missing, "服务器URL");
      if (ch.key1.length() == 0) appendMissingField(missing, "Token");
      break;
    case PUSH_TYPE_TELEGRAM:
      if (ch.key1.length() == 0) appendMissingField(missing, "Chat ID");
      if (ch.key2.length() == 0) appendMissingField(missing, "Bot Token");
      break;
    default:
      appendMissingField(missing, "有效推送类型");
      break;
  }

  if (missing.length() == 0) return "";
  String name = ch.name.length() > 0 ? ch.name : "未命名通道";
  return name + "（" + pushTypeName(ch.type) + "）缺少：" + missing;
}

bool validateConfig(String& reason) {
  reason = "";
  bool emailValid = config.smtpServer.length() > 0 &&
                    config.smtpUser.length() > 0 &&
                    config.smtpPass.length() > 0 &&
                    config.smtpSendTo.length() > 0;

  bool pushValid = false;
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    if (isPushChannelValid(config.pushChannels[i])) {
      pushValid = true;
      break;
    }
  }

  if (emailValid || pushValid) return true;

  reason = "没有可用的通知通道。至少需要完整邮箱配置，或启用并填写一个有效推送通道。";
  String emailReason = getEmailInvalidReason();
  if (emailReason.length() > 0) {
    reason += "\n- " + emailReason;
  }

  bool hasEnabledPush = false;
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    if (config.pushChannels[i].enabled) {
      hasEnabledPush = true;
      String channelReason = getPushChannelInvalidReason(config.pushChannels[i]);
      if (channelReason.length() > 0) {
        reason += "\n- 推送通道" + String(i + 1) + "：" + channelReason;
      }
    }
  }
  if (!hasEnabledPush) {
    reason += "\n- 没有启用任何推送通道";
  }
  return false;
}

bool isConfigValid() {
  String reason;
  return validateConfig(reason);
}

void printConfigValidationResult(const String& source) {
  configValid = validateConfig(configInvalidReason);
  Serial.println("=== 配置检查（" + source + "）===");
  if (configValid) {
    Serial.println("配置有效：已配置至少一个可用通知通道");
  } else {
    Serial.println("配置无效：");
    Serial.println(configInvalidReason);
    Serial.println("请访问 " + getDeviceUrl() + " 配置系统参数");
  }
  Serial.println("====================");
}

// 获取当前设备URL
String getDeviceUrl() {
  if (WiFi.status() == WL_CONNECTED) {
    return "http://" + WiFi.localIP().toString() + "/";
  }
  IPAddress apIp = WiFi.softAPIP();
  if (apIp != IPAddress(0, 0, 0, 0)) {
    return "http://" + apIp.toString() + "/";
  }
  return "http://" + WiFi.localIP().toString() + "/";
}
