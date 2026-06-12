#pragma once

// Web 配置页和工具页入口处理函数。
String htmlAttributeEscape(const String& value) {
  String out = value;
  out.replace("&", "&amp;");
  out.replace("\"", "&quot;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  return out;
}

// 处理配置页面请求
void handleRoot() {
  if (!checkAuth()) return;

  String html = String(htmlPage);
  html.replace("%IP%", WiFi.localIP().toString());
  html.replace("%WEB_USER%", config.webUser);
  html.replace("%WEB_PASS%", config.webPass);
  html.replace("%SMTP_SERVER%", config.smtpServer);
  html.replace("%SMTP_PORT%", String(config.smtpPort));
  html.replace("%SMTP_USER%", config.smtpUser);
  html.replace("%SMTP_PASS%", config.smtpPass);
  html.replace("%SMTP_SEND_TO%", config.smtpSendTo);
  html.replace("%STARTUP_MAIL_CHECKED%", config.startupMailEnabled ? "checked" : "");
  html.replace("%ADMIN_PHONE%", config.adminPhone);
  html.replace("%NUMBER_BLACK_LIST%", config.numberBlackList);
  html.replace("%PUSH_FILTER_CHECKED%", config.pushFilterEnabled ? "checked" : "");
  html.replace("%PUSH_FILTER_TARGET_SENDER_SEL%", config.pushFilterTarget == PUSH_FILTER_TARGET_SENDER ? " selected" : "");
  html.replace("%PUSH_FILTER_TARGET_MESSAGE_SEL%", config.pushFilterTarget == PUSH_FILTER_TARGET_MESSAGE ? " selected" : "");
  html.replace("%PUSH_FILTER_MODE_CONTAINS_SEL%", config.pushFilterMode == PUSH_FILTER_MODE_CONTAINS ? " selected" : "");
  html.replace("%PUSH_FILTER_MODE_NOT_CONTAINS_SEL%", config.pushFilterMode == PUSH_FILTER_MODE_NOT_CONTAINS ? " selected" : "");
  html.replace("%PUSH_FILTER_MODE_STARTS_WITH_SEL%", config.pushFilterMode == PUSH_FILTER_MODE_STARTS_WITH ? " selected" : "");
  html.replace("%PUSH_FILTER_MODE_ENDS_WITH_SEL%", config.pushFilterMode == PUSH_FILTER_MODE_ENDS_WITH ? " selected" : "");
  html.replace("%PUSH_FILTER_EXPR%", htmlAttributeEscape(config.pushFilterExpr));

  // 掉线检测配置
  html.replace("%KA_CHECKED%", config.keepAliveEnabled ? "checked" : "");
  html.replace("%KA_URL%", config.keepAliveUrl);
  html.replace("%KA_METHOD_GET_SEL%", config.keepAliveMethod == "POST" ? "" : " selected");
  html.replace("%KA_METHOD_POST_SEL%", config.keepAliveMethod == "POST" ? " selected" : "");
  html.replace("%KA_BODY%", config.keepAliveBody);
  html.replace("%KA_INTERVAL%", String(config.keepAliveInterval));

  // 生成推送通道HTML
  String channelsHtml = "";
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    String idx = String(i);
    String enabledClass = config.pushChannels[i].enabled ? " enabled" : "";
    String checked = config.pushChannels[i].enabled ? " checked" : "";

    channelsHtml += "<div class=\"push-channel" + enabledClass + "\" id=\"channel" + idx + "\">";
    channelsHtml += "<div class=\"push-channel-header\">";
    channelsHtml += "<input type=\"checkbox\" name=\"push" + idx + "en\" id=\"push" + idx + "en\" onchange=\"toggleChannel(" + idx + ")\"" + checked + ">";
    channelsHtml += "<label for=\"push" + idx + "en\" class=\"label-inline\">启用推送通道 " + String(i + 1) + "</label>";
    channelsHtml += "</div>";
    channelsHtml += "<div class=\"push-channel-body\">";

    // 通道名称
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label>通道名称</label>";
    channelsHtml += "<input type=\"text\" name=\"push" + idx + "name\" value=\"" + config.pushChannels[i].name + "\" placeholder=\"自定义名称\">";
    channelsHtml += "</div>";

    // 推送类型
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label>推送方式</label>";
    channelsHtml += "<select name=\"push" + idx + "type\" id=\"push" + idx + "type\" onchange=\"updateTypeHint(" + idx + ")\">";
    channelsHtml += "<option value=\"1\"" + String(config.pushChannels[i].type == PUSH_TYPE_POST_JSON ? " selected" : "") + ">POST JSON（通用格式）</option>";
    channelsHtml += "<option value=\"2\"" + String(config.pushChannels[i].type == PUSH_TYPE_BARK ? " selected" : "") + ">Bark（iOS推送）</option>";
    channelsHtml += "<option value=\"3\"" + String(config.pushChannels[i].type == PUSH_TYPE_GET ? " selected" : "") + ">GET请求（参数在URL中）</option>";
    channelsHtml += "<option value=\"4\"" + String(config.pushChannels[i].type == PUSH_TYPE_DINGTALK ? " selected" : "") + ">钉钉机器人</option>";
    channelsHtml += "<option value=\"5\"" + String(config.pushChannels[i].type == PUSH_TYPE_PUSHPLUS ? " selected" : "") + ">PushPlus</option>";
    channelsHtml += "<option value=\"6\"" + String(config.pushChannels[i].type == PUSH_TYPE_SERVERCHAN ? " selected" : "") + ">Server酱</option>";
    channelsHtml += "<option value=\"7\"" + String(config.pushChannels[i].type == PUSH_TYPE_CUSTOM ? " selected" : "") + ">自定义模板</option>";
    channelsHtml += "<option value=\"8\"" + String(config.pushChannels[i].type == PUSH_TYPE_FEISHU ? " selected" : "") + ">飞书机器人</option>";
    channelsHtml += "<option value=\"9\"" + String(config.pushChannels[i].type == PUSH_TYPE_GOTIFY ? " selected" : "") + ">Gotify</option>";
    channelsHtml += "<option value=\"10\"" + String(config.pushChannels[i].type == PUSH_TYPE_TELEGRAM ? " selected" : "") + ">Telegram Bot</option>";
    channelsHtml += "</select>";
    channelsHtml += "<div class=\"push-type-hint\" id=\"hint" + idx + "\"></div>";
    channelsHtml += "</div>";

    // URL
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label>推送URL/Webhook</label>";
    channelsHtml += "<input type=\"text\" name=\"push" + idx + "url\" value=\"" + config.pushChannels[i].url + "\" placeholder=\"http://your-server.com/api 或 webhook地址\">";
    channelsHtml += "</div>";

    // 额外参数区域（钉钉/PushPlus/Server酱等需要）
    channelsHtml += "<div id=\"extra" + idx + "\" style=\"display:none;\">";
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label id=\"key1label" + idx + "\">参数1</label>";
    channelsHtml += "<input type=\"text\" name=\"push" + idx + "key1\" id=\"key1" + idx + "\" value=\"" + config.pushChannels[i].key1 + "\">";
    channelsHtml += "</div>";
    channelsHtml += "<div class=\"form-group\" id=\"key2group" + idx + "\">";
    channelsHtml += "<label id=\"key2label" + idx + "\">参数2</label>";
    channelsHtml += "<input type=\"text\" name=\"push" + idx + "key2\" id=\"key2" + idx + "\" value=\"" + config.pushChannels[i].key2 + "\">";
    channelsHtml += "</div>";
    channelsHtml += "</div>";

    // 自定义模板区域
    channelsHtml += "<div id=\"custom" + idx + "\" style=\"display:none;\">";
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label>请求体模板（使用 {sender} {message} {timestamp} 占位符）</label>";
    channelsHtml += "<textarea name=\"push" + idx + "body\" rows=\"4\" style=\"width:100%;font-family:monospace;\">" + config.pushChannels[i].customBody + "</textarea>";
    channelsHtml += "</div>";
    channelsHtml += "</div>";

    channelsHtml += "</div></div>";
  }
  html.replace("%PUSH_CHANNELS%", channelsHtml);

  server.send(200, "text/html", html);
}

// 处理工具箱页面请求
void handleToolsPage() {
  if (!checkAuth()) return;

  String html = String(htmlToolsPage);
  html.replace("%IP%", WiFi.localIP().toString());
  server.send(200, "text/html", html);
}
