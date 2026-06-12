#pragma once

// Web 页面模板，保持在单独文件中以降低主程序阅读负担。
// HTML配置页面
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>短信转发配置</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
    .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h1 { color: #333; text-align: center; }
    .form-group { margin-bottom: 15px; }
    label { display: block; margin-bottom: 5px; font-weight: bold; color: #555; }
    input[type="text"], input[type="password"], input[type="number"], textarea, select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }
    textarea { resize: vertical; min-height: 80px; }
    button { width: 100%; padding: 12px; background: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 10px; }
    button:hover { background: #45a049; }
    .label-inline { display:inline; font-weight:normal; margin-left: 5px; }
    .btn-send { background: #2196F3; }
    .btn-send:hover { background: #1976D2; }
    .section { border: 1px solid #ddd; padding: 15px; margin-bottom: 20px; border-radius: 5px; }
    .section-title { font-size: 18px; color: #333; margin-bottom: 10px; }
    .status { padding: 10px; background: #e7f3fe; border-left: 4px solid #2196F3; margin-bottom: 20px; }
    .warning { padding: 10px; background: #fff3cd; border-left: 4px solid #ffc107; margin-bottom: 20px; font-size: 12px; }
    .hint { font-size: 12px; color: #888; }
    .nav { display: flex; gap: 10px; margin-bottom: 20px; }
    .nav a { flex: 1; text-align: center; padding: 10px; background: #eee; border-radius: 5px; text-decoration: none; color: #333; }
    .nav a.active { background: #4CAF50; color: white; }
    .push-channel { border: 1px solid #e0e0e0; padding: 12px; margin-bottom: 15px; border-radius: 5px; background: #fafafa; }
    .push-channel-header { display: flex; align-items: center; margin-bottom: 10px; }
    .push-channel-header input[type="checkbox"] { width: auto; margin-right: 8px; }
    .push-channel-header label { margin: 0; font-weight: bold; }
    .push-channel-body { display: none; }
    .push-channel.enabled .push-channel-body { display: block; }
    .push-type-hint { font-size: 11px; color: #666; margin-top: 5px; padding: 8px; background: #f0f0f0; border-radius: 3px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>📱 短信转发器</h1>
    <div class="nav">
      <a href="/" class="active">⚙️ 系统配置</a>
      <a href="/tools">🧰 工具箱</a>
    </div>
    <div class="status" id="status">设备IP: <strong>%IP%</strong></div>

    <form action="/save" method="POST">
      <div class="section">
        <div class="section-title">🔐 Web管理账号设置</div>
        <div class="warning">⚠️ 首次使用请修改默认密码！默认账号: )rawliteral" DEFAULT_WEB_USER "，默认密码: " DEFAULT_WEB_PASS R"rawliteral(
        </div>
        <div class="form-group">
          <label>管理账号</label>
          <input type="text" name="webUser" value="%WEB_USER%" placeholder="admin">
        </div>
        <div class="form-group">
          <label>管理密码</label>
          <input type="password" name="webPass" value="%WEB_PASS%" placeholder="请设置复杂密码">
        </div>
      </div>

      <div class="section">
        <div class="section-title">📧 邮件通知设置</div>
        <div class="form-group">
          <label>SMTP服务器</label>
          <input type="text" name="smtpServer" value="%SMTP_SERVER%" placeholder="smtp.qq.com">
        </div>
        <div class="form-group">
          <label>SMTP端口</label>
          <input type="number" name="smtpPort" value="%SMTP_PORT%" placeholder="465">
        </div>
        <div class="form-group">
          <label>邮箱账号</label>
          <input type="text" name="smtpUser" value="%SMTP_USER%" placeholder="your@qq.com">
        </div>
        <div class="form-group">
          <label>邮箱密码/授权码</label>
          <input type="password" name="smtpPass" value="%SMTP_PASS%" placeholder="授权码">
        </div>
        <div class="form-group">
          <label>接收邮件地址</label>
          <input type="text" name="smtpSendTo" value="%SMTP_SEND_TO%" placeholder="receiver@example.com">
        </div>
      </div>

      <div class="section">
        <div class="section-title">🔗 HTTP推送通道设置</div>
        <div class="hint" style="margin-bottom:15px;">可同时启用多个推送通道，每个通道独立配置。支持POST JSON、Bark、GET、钉钉、PushPlus、Server酱等多种方式。</div>

        %PUSH_CHANNELS%
      </div>

      <div class="section">
        <div class="section-title">👤 管理员设置</div>
        <div class="form-group">
          <label>管理员手机号</label>
          <input type="text" name="adminPhone" value="%ADMIN_PHONE%" placeholder="13800138000">
        </div>
      </div>

      <div class="section">
        <div class="section-title">🚫 号码黑名单</div>
        <div class="hint" style="margin-bottom:15px;">每行一个号码，来自黑名单号码的短信将被忽略。</div>
        <div class="form-group">
          <label>黑名单号码</label>
          <textarea name="numberBlackList" rows="5">%NUMBER_BLACK_LIST%</textarea>
        </div>
      </div>

      <button type="submit">💾 保存配置</button>
    </form>
  </div>
  <script>
    function toggleChannel(idx) {
      var ch = document.getElementById('channel' + idx);
      var cb = document.getElementById('push' + idx + 'en');
      if (cb.checked) {
        ch.classList.add('enabled');
      } else {
        ch.classList.remove('enabled');
      }
    }
    function updateTypeHint(idx) {
      var sel = document.getElementById('push' + idx + 'type');
      var hint = document.getElementById('hint' + idx);
      var extraFields = document.getElementById('extra' + idx);
      var customFields = document.getElementById('custom' + idx);
      var type = parseInt(sel.value);

      // 隐藏所有额外字段
      extraFields.style.display = 'none';
      customFields.style.display = 'none';
      document.getElementById('key1label' + idx).innerText = '参数1';
      document.getElementById('key2label' + idx).innerText = '参数2';
      document.getElementById('key1' + idx).placeholder = '';
      document.getElementById('key2' + idx).placeholder = '';
      // key2 区域默认隐藏，只在需要用到 key2 的通知方式中显示
      document.getElementById('key2' + idx).closest('.form-group').style.display = 'none';

      if (type == 1) {
        hint.innerHTML = '<b>POST JSON格式：</b><br>{"sender":"发送者号码","message":"短信内容","timestamp":"时间戳"}';
      } else if (type == 2) {
        hint.innerHTML = '<b>Bark格式：</b><br>POST {"title":"发送者号码","body":"短信内容"}';
      } else if (type == 3) {
        hint.innerHTML = '<b>GET请求格式：</b><br>URL?sender=xxx&message=xxx&timestamp=xxx';
      } else if (type == 4) {
        hint.innerHTML = '<b>钉钉机器人：</b><br>填写Webhook地址，如需加签请填Secret';
        extraFields.style.display = 'block';
        document.getElementById('key1label' + idx).innerText = 'Secret（加签密钥，可选）';
        document.getElementById('key1' + idx).placeholder = 'SEC...';
      } else if (type == 5) {
        hint.innerHTML = '<b>PushPlus：</b><br>填写Token，URL留空使用默认';
        extraFields.style.display = 'block';
        document.getElementById('key1label' + idx).innerText = 'Token';
        document.getElementById('key1' + idx).placeholder = 'pushplus的token';
        // 显示 key2 区域
        document.getElementById('key2' + idx).closest('.form-group').style.display = 'block';
        document.getElementById('key2label' + idx).innerText = '发送渠道';
        document.getElementById('key2' + idx).placeholder = 'wechat(default), extension, app';
      } else if (type == 6) {
        hint.innerHTML = '<b>Server酱：</b><br>填写SendKey，URL留空使用默认';
        extraFields.style.display = 'block';
        document.getElementById('key1label' + idx).innerText = 'SendKey';
        document.getElementById('key1' + idx).placeholder = 'SCT...';
      } else if (type == 7) {
        hint.innerHTML = '<b>自定义模板：</b><br>在请求体模板中使用 {sender} {message} {timestamp} 作为占位符';
        customFields.style.display = 'block';
      } else if (type == 8) {
        hint.innerHTML = '<b>飞书机器人：</b><br>填写Webhook地址，如需签名验证请填Secret';
        extraFields.style.display = 'block';
        document.getElementById('key1label' + idx).innerText = 'Secret（签名密钥，可选）';
        document.getElementById('key1' + idx).placeholder = '飞书机器人的签名密钥';
      } else if (type == 9) {
        hint.innerHTML = '<b>Gotify：</b><br>填写服务器地址（如 http://gotify.example.com），Token填写应用Token';
        extraFields.style.display = 'block';
        document.getElementById('key1label' + idx).innerText = 'Token（应用Token）';
        document.getElementById('key1' + idx).placeholder = 'A...';
      } else if (type == 10) {
        hint.innerHTML = '<b>Telegram Bot：</b><br>填写Chat ID（参数1）和Bot Token（参数2），URL留空默认使用官方API';
        extraFields.style.display = 'block';
        document.getElementById('key1label' + idx).innerText = 'Chat ID';
        document.getElementById('key1' + idx).placeholder = '123456789';
        document.getElementById('key2label' + idx).innerText = 'Bot Token';
        document.getElementById('key2' + idx).placeholder = '12345678:ABC...';
      }
    }
    document.addEventListener('DOMContentLoaded', function() {
      for (var i = 0; i < 5; i++) {
        toggleChannel(i);
        updateTypeHint(i);
      }
    });
  </script>
</body>
</html>
)rawliteral";

// HTML工具箱页面
const char* htmlToolsPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>工具箱</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
    .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h1 { color: #333; text-align: center; }
    .form-group { margin-bottom: 15px; }
    label { display: block; margin-bottom: 5px; font-weight: bold; color: #555; }
    input[type="text"], textarea, select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; background: white; }
    textarea { resize: vertical; min-height: 100px; }
    button { width: 100%; padding: 12px; background: #2196F3; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 10px; }
    button:hover { background: #1976D2; }
    .btn-query { background: #9C27B0; }
    .btn-query:hover { background: #7B1FA2; }
    .btn-ping { background: #FF9800; }
    .btn-ping:hover { background: #F57C00; }
    .btn-info { background: #607D8B; }
    .btn-info:hover { background: #455A64; }
    button:disabled { background: #ccc; cursor: not-allowed; }
    .section { border: 1px solid #ddd; padding: 15px; margin-bottom: 20px; border-radius: 5px; }
    .section-title { font-size: 18px; color: #333; margin-bottom: 10px; }
    .status { padding: 10px; background: #e7f3fe; border-left: 4px solid #2196F3; margin-bottom: 20px; }
    .nav { display: flex; gap: 10px; margin-bottom: 20px; }
    .nav a { flex: 1; text-align: center; padding: 10px; background: #eee; border-radius: 5px; text-decoration: none; color: #333; }
    .nav a.active { background: #2196F3; color: white; }
    .char-count { font-size: 12px; color: #888; text-align: right; }
    .hint { font-size: 12px; color: #888; margin-top: 5px; }
    .result-box { margin-top: 10px; padding: 10px; border-radius: 5px; display: none; }
    .result-success { background: #e8f5e9; border-left: 4px solid #4CAF50; color: #2e7d32; }
    .result-error { background: #ffebee; border-left: 4px solid #f44336; color: #c62828; }
    .result-loading { background: #fff3e0; border-left: 4px solid #FF9800; color: #e65100; }
    .result-info { background: #e3f2fd; border-left: 4px solid #2196F3; color: #1565c0; }
    .info-table { width: 100%; border-collapse: collapse; margin-top: 8px; }
    .info-table td { padding: 5px 8px; border-bottom: 1px solid #ddd; }
    .info-table td:first-child { font-weight: bold; width: 40%; color: #555; }
    .sms-card { background: #fff; border: 1px solid #ddd; border-left: 4px solid #2196F3; border-radius: 5px; padding: 10px; margin-top: 10px; color: #333; }
    .sms-meta { font-size: 12px; color: #555; margin-bottom: 6px; line-height: 1.6; }
    .sms-badge { display: inline-block; padding: 2px 6px; border-radius: 3px; color: white; font-size: 12px; margin-right: 5px; }
    .sms-read { background: #607D8B; }
    .sms-unread { background: #E91E63; }
    .sms-body { white-space: pre-wrap; word-break: break-word; color: #222; }
    .btn-group { display: flex; gap: 10px; flex-wrap: wrap; }
    .btn-group button { flex: 1; min-width: 120px; }
    #atLog { background: #333; color: #00ff00; font-family: 'Courier New', Courier, monospace; min-height: 150px; max-height: 300px; overflow-y: auto; padding: 10px; border-radius: 5px; margin-bottom: 10px; font-size: 13px; white-space: pre-wrap; word-break: break-all; }
    .at-input-group { display: flex; gap: 10px; }
    .at-input-group input { flex: 1; font-family: monospace; }
    .at-input-group button { width: auto; min-width: 80px; margin-top: 0; }
  </style>
</head>
<body>
  <div class="container">
    <h1>📱 短信转发器</h1>
    <div class="nav">
      <a href="/">⚙️ 系统配置</a>
      <a href="/tools" class="active">🧰 工具箱</a>
    </div>
    <div class="status" id="status">设备IP: <strong>%IP%</strong></div>

    <form action="/sendsms" method="POST">
      <div class="section">
        <div class="section-title">📤 发送短信</div>
        <div class="form-group">
          <label>目标号码</label>
          <input type="text" name="phone" placeholder="13800138000" required>
        </div>
        <div class="form-group">
          <label>短信内容</label>
          <textarea name="content" placeholder="请输入短信内容..." required oninput="updateCount(this)"></textarea>
          <div class="char-count">已输入 <span id="charCount">0</span> 字符</div>
        </div>
        <button type="submit">📨 发送短信</button>
      </div>
    </form>

    <div class="section">
      <div class="section-title">📊 模组信息查询</div>
      <div class="btn-group">
        <button type="button" class="btn-query" onclick="queryInfo('ati')">📋 固件信息</button>
        <button type="button" class="btn-query" onclick="queryInfo('signal')">📶 信号质量</button>
      </div>
      <div class="btn-group">
        <button type="button" class="btn-info" onclick="queryInfo('siminfo')">💳 SIM卡信息</button>
        <button type="button" class="btn-info" onclick="queryInfo('network')">🌍 网络状态</button>
      </div>
      <div class="btn-group">
        <button type="button" class="btn-info" onclick="queryInfo('wifi')" style="background:#00BCD4;">📡 WiFi状态</button>
        <button type="button" class="btn-info" onclick="clearWiFiConfig()" style="background:#795548;">清除WiFi记录</button>
      </div>
      <div class="result-box" id="queryResult"></div>
    </div>

    <details class="section" id="pushDebugPanel">
      <summary class="section-title" style="cursor:pointer;">推送测试与调试</summary>
      <div class="btn-group">
        <button type="button" class="btn-info" id="pushDebugToggleBtn" onclick="togglePushDebug()">开启推送日志</button>
        <button type="button" class="btn-query" id="testPushBtn" onclick="testPush()">测试推送</button>
      </div>
      <div class="btn-group">
        <button type="button" class="btn-info" onclick="refreshPushDebug()">刷新日志</button>
        <button type="button" class="btn-info" onclick="clearPushDebug()">清空日志</button>
      </div>
      <div class="hint">开启后会在串口和本页面记录推送请求、请求体、响应码和响应内容；关闭后不记录，避免污染其他调试。</div>
      <div class="result-box" id="testPushResult"></div>
      <div id="pushDebugLog" style="background:#222;color:#d6ffd6;font-family:'Courier New',monospace;min-height:120px;max-height:320px;overflow-y:auto;padding:10px;border-radius:5px;margin-top:10px;font-size:12px;white-space:pre-wrap;word-break:break-all;">暂无推送日志</div>
    </details>

    <div class="section">
      <div class="section-title">短信列表</div>
      <div class="form-group">
        <label>显示数量</label>
        <select id="smsListLimit">
          <option value="20" selected>前 20 条</option>
          <option value="50">前 50 条</option>
          <option value="100">前 100 条</option>
        </select>
      </div>
      <button type="button" class="btn-info" id="smsListBtn" onclick="loadSmsList()">查看短信列表</button>
      <div class="hint">先扫描索引，再用 CMGR 逐条读取，避免 CMGL 批量PDU过长导致截断。</div>
      <div class="result-box" id="smsListResult"></div>
    </div>

    <div class="section">
      <div class="section-title">🌐 网络测试</div>
      <button type="button" class="btn-ping" id="pingBtn" onclick="confirmPing()">📡 点我消耗一点流量</button>
      <div class="hint">将向 8.8.8.8 进行 ping 操作，一次性消耗极少流量费用</div>
      <div class="result-box" id="pingResult"></div>
    </div>

    <div class="section">
      <div class="section-title">✈️ 模组控制</div>
      <div class="btn-group">
        <button type="button" id="flightBtn" onclick="toggleFlightMode()" style="background:#E91E63;">✈️ 切换飞行模式</button>
        <button type="button" onclick="queryFlightMode()" style="background:#9C27B0;">🔍 查询状态</button>
      </div>
      <div class="hint">飞行模式关闭时模组可正常收发短信，开启后将关闭射频无法使用移动网络</div>
      <div class="result-box" id="flightResult"></div>
    </div>

    <div class="section">
      <div class="section-title">💻 AT 指令调试</div>
      <div id="atLog">等待输入指令...</div>
      <div class="at-input-group">
        <input type="text" id="atCmd" placeholder="输入 AT 指令，如: AT+CSQ">
        <button type="button" onclick="sendAT()" id="atBtn">发送</button>
      </div>
      <div class="btn-group" style="margin-top:10px;">
        <button type="button" class="btn-info" onclick="clearATLog()">🧹 清空日志</button>
      </div>
      <div class="hint">直接向模组串口发送指令并接收响应，请谨慎操作</div>
    </div>
  </div>
  <script>
    function updateCount(el) {
      document.getElementById('charCount').textContent = el.value.length;
    }

    function queryInfo(type) {
      var result = document.getElementById('queryResult');
      result.className = 'result-box result-loading';
      result.style.display = 'block';
      result.textContent = '正在查询，请稍候...';

      fetch('/query?type=' + type)
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            result.className = 'result-box result-info';
            result.innerHTML = data.message;
          } else {
            result.className = 'result-box result-error';
            result.innerHTML = '❌ 查询失败<br>' + data.message;
          }
        })
        .catch(error => {
          result.className = 'result-box result-error';
          result.textContent = '❌ 请求失败: ' + error;
        });
    }

    function clearWiFiConfig() {
      if (!confirm('确定清除已保存的WiFi连接记录并重启设备吗？')) return;

      fetch('/wificlear', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
          alert(data.message || 'WiFi连接记录已清除');
        })
        .catch(error => {
          alert('请求失败: ' + error);
        });
    }

    function setPushDebugLog(data) {
      var log = document.getElementById('pushDebugLog');
      var toggleBtn = document.getElementById('pushDebugToggleBtn');
      log.textContent = data.message || '暂无推送日志';
      log.scrollTop = log.scrollHeight;
      toggleBtn.textContent = data.enabled ? '关闭推送日志' : '开启推送日志';
      toggleBtn.style.background = data.enabled ? '#E91E63' : '#607D8B';
    }

    function refreshPushDebug() {
      fetch('/pushdebug?action=query')
        .then(response => response.json())
        .then(data => setPushDebugLog(data));
    }

    function togglePushDebug() {
      fetch('/pushdebug?action=toggle')
        .then(response => response.json())
        .then(data => setPushDebugLog(data));
    }

    function clearPushDebug() {
      fetch('/pushdebug?action=clear')
        .then(response => response.json())
        .then(data => setPushDebugLog(data));
    }

    function testPush() {
      var btn = document.getElementById('testPushBtn');
      var result = document.getElementById('testPushResult');
      btn.disabled = true;
      result.className = 'result-box result-loading';
      result.style.display = 'block';
      result.textContent = '正在发送测试推送...';

      fetch('/testpush', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
          result.className = data.success ? 'result-box result-success' : 'result-box result-error';
          result.textContent = data.message;
          refreshPushDebug();
        })
        .catch(error => {
          result.className = 'result-box result-error';
          result.textContent = '请求失败: ' + error;
        })
        .finally(() => {
          btn.disabled = false;
        });
    }

    function smsStatusText(status) {
      if (status === 0) return '未读';
      if (status === 1) return '已读';
      if (status === 2) return '未发送';
      if (status === 3) return '已发送';
      return '未知';
    }

    function smsStatusClass(status) {
      return status === 0 ? 'sms-unread' : 'sms-read';
    }

    function htmlEscapeClient(text) {
      return String(text == null ? '' : text)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
    }

    function cleanPduHex(pdu) {
      var hex = String(pdu || '').replace(/[^0-9a-fA-F]/g, '').toUpperCase();
      if (hex.length % 2) throw new Error('PDU长度不是偶数');
      return hex;
    }

    function pduByte(pdu, offset) {
      if (offset + 2 > pdu.length) throw new Error('PDU偏移越界: ' + offset);
      return parseInt(pdu.substr(offset, 2), 16);
    }

    function swapBcd(hex, maxDigits) {
      var out = '';
      for (var i = 0; i < hex.length; i += 2) {
        out += hex.charAt(i + 1) + hex.charAt(i);
      }
      out = out.replace(/F/gi, '');
      return maxDigits == null ? out : out.slice(0, maxDigits);
    }

    var GSM7_DEFAULT = '@£$¥èéùìòÇ\nØø\rÅåΔ_ΦΓΛΩΠΨΣΘΞ' +
      "\\x1bÆæßÉ !\\\"#¤%&'()*+,-./" +
      '0123456789:;<=>?¡ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÑÜ§' +
      '¿abcdefghijklmnopqrstuvwxyzäöñüà';
    var GSM7_EXT = {10:'\f',20:'^',40:'{',41:'}',47:'\\',60:'[',61:'~',62:']',64:'|',101:'€'};

    function unpackGsm7(hex, septetCount, skipBits) {
      var bytes = [];
      for (var i = 0; i < hex.length; i += 2) bytes.push(parseInt(hex.substr(i, 2), 16));
      var out = '';
      var esc = false;
      skipBits = skipBits || 0;
      for (var s = 0; s < septetCount; s++) {
        var bit = skipBits + s * 7;
        var val = 0;
        for (var b = 0; b < 7; b++) {
          var absolute = bit + b;
          var byteIndex = Math.floor(absolute / 8);
          if (byteIndex >= bytes.length) break;
          if (bytes[byteIndex] & (1 << (absolute % 8))) val |= (1 << b);
        }
        if (esc) {
          out += GSM7_EXT[val] || '�';
          esc = false;
        } else if (val === 0x1B) {
          esc = true;
        } else {
          out += GSM7_DEFAULT.charAt(val) || '�';
        }
      }
      return out;
    }

    function decodeUcs2(hex) {
      var out = '';
      for (var i = 0; i + 3 < hex.length; i += 4) {
        out += String.fromCharCode(parseInt(hex.substr(i, 4), 16));
      }
      return out;
    }

    function decodePduAddress(pdu, offset, lengthType) {
      var length = pduByte(pdu, offset);
      if (lengthType === 'octets' && length === 0) {
        return { text: '', next: offset + 2, type: 0, ton: 0 };
      }
      var addressLength = lengthType === 'nibbles' ? length : (length - 1) * 2;
      var type = pduByte(pdu, offset + 2);
      var ton = (type & 0x70) >> 4;
      var start = offset + 4;
      var paddedLength = addressLength + (addressLength & 1);
      var raw = pdu.substr(start, paddedLength);
      var text = '';
      if ((type & 0x80) !== 0) {
        if (ton === 0 || ton === 1 || ton === 2) {
          text = (ton === 1 ? '+' : '') + swapBcd(raw, addressLength);
        } else if (ton === 5) {
          text = unpackGsm7(raw, Math.floor(addressLength * 4 / 7), 0);
        }
      }
      return { text: text, next: start + paddedLength, type: type, ton: ton };
    }

    function decodePduTimestamp(pdu, offset) {
      var parts = [];
      for (var i = 0; i < 7; i++) parts.push(swapBcd(pdu.substr(offset + i * 2, 2)));
      return '20' + parts[0] + '-' + parts[1] + '-' + parts[2] + ' ' + parts[3] + ':' + parts[4] + ':' + parts[5];
    }

    function decodeSmsDeliverPdu(input) {
      var pdu = cleanPduHex(input);
      var pos = 0;
      var sca = decodePduAddress(pdu, pos, 'octets');
      pos = sca.next;
      var first = pduByte(pdu, pos);
      var udhi = (first & 0x40) !== 0;
      pos += 2;
      var sender = decodePduAddress(pdu, pos, 'nibbles');
      pos = sender.next;
      var pid = pduByte(pdu, pos); pos += 2;
      var dcs = pduByte(pdu, pos); pos += 2;
      var timestamp = decodePduTimestamp(pdu, pos); pos += 14;
      var udl = pduByte(pdu, pos); pos += 2;
      var bodyStart = pos;
      var concat = null;
      var text = '';

      if (udhi) {
        var udhl = pduByte(pdu, pos);
        var headerStart = pos + 2;
        var header = pdu.substr(headerStart, udhl * 2);
        if (header.length >= 10 && header.substr(0, 2) === '00' && header.substr(2, 2) === '03') {
          concat = {
            ref: parseInt(header.substr(4, 2), 16),
            total: parseInt(header.substr(6, 2), 16),
            part: parseInt(header.substr(8, 2), 16)
          };
        } else if (header.length >= 12 && header.substr(0, 2) === '08' && header.substr(2, 2) === '04') {
          concat = {
            ref: parseInt(header.substr(4, 4), 16),
            total: parseInt(header.substr(8, 2), 16),
            part: parseInt(header.substr(10, 2), 16)
          };
        }
        bodyStart = pos + (udhl + 1) * 2;
        if ((dcs & 0x0C) === 0x08) {
          var ucsOctets = Math.max(0, udl - (udhl + 1));
          text = decodeUcs2(pdu.substr(bodyStart, ucsOctets * 2));
        } else if ((dcs & 0x0C) === 0x00) {
          var headerSeptets = Math.ceil(((udhl + 1) * 8) / 7);
          var septets = Math.max(0, udl - headerSeptets);
          var skipBits = ((udhl + 1) * 8) % 7;
          text = unpackGsm7(pdu.substr(bodyStart), septets, skipBits);
        } else {
          text = '[暂不支持的DCS 0x' + dcs.toString(16).toUpperCase() + ']';
        }
      } else if ((dcs & 0x0C) === 0x08) {
        text = decodeUcs2(pdu.substr(bodyStart, udl * 2));
      } else if ((dcs & 0x0C) === 0x00) {
        text = unpackGsm7(pdu.substr(bodyStart), udl, 0);
      } else {
        text = '[暂不支持的DCS 0x' + dcs.toString(16).toUpperCase() + ']';
      }

      return { sender: sender.text, timestamp: timestamp, text: text, concat: concat, dcs: dcs, pid: pid };
    }

    function smsCard(indexLabel, status, sender, timestamp, text) {
      return "<div class='sms-card'><div class='sms-meta'>" +
        "<span class='sms-badge " + smsStatusClass(status) + "'>" + smsStatusText(status) + "</span>" +
        '索引: ' + htmlEscapeClient(indexLabel) +
        ' | 收信时间: ' + htmlEscapeClient(timestamp || '-') +
        ' | 发信人: ' + htmlEscapeClient(sender || '-') +
        "</div><div class='sms-body'>" + htmlEscapeClient(text || '(空短信)') + '</div></div>';
    }

    function renderDecodedSms(storageHtml, limit, entries, decodedItems) {
      var html = storageHtml || '';
      html += '<div>本次显示前 ' + limit + ' 条，实际读取 ' + entries.length + ' 条。内容由浏览器逐条解析。</div>';
      if (!entries.length) return html + '模块短信存储中没有短信。';

      var groups = {};
      var order = [];
      decodedItems.forEach(function(item) {
        if (!item.decoded || !item.decoded.concat || item.decoded.concat.total <= 1) {
          order.push({ type: 'single', item: item });
          return;
        }
        var c = item.decoded.concat;
        var key = item.decoded.sender + '|' + c.ref + '|' + c.total;
        if (!groups[key]) {
          groups[key] = {
            sender: item.decoded.sender,
            timestamp: item.decoded.timestamp,
            status: item.entry.status,
            indexes: [],
            total: c.total,
            parts: {}
          };
          order.push({ type: 'group', key: key });
        }
        groups[key].indexes.push(item.entry.index);
        if (item.entry.status === 0) groups[key].status = 0;
        groups[key].parts[c.part] = item.decoded.text;
      });

      order.forEach(function(row) {
        if (row.type === 'single') {
          var item = row.item;
          if (item.decoded) {
            html += smsCard(String(item.entry.index), item.entry.status, item.decoded.sender, item.decoded.timestamp, item.decoded.text);
          } else {
            html += smsCard(String(item.entry.index), item.entry.status, '-', '-', item.error || '读取失败');
          }
        } else {
          var g = groups[row.key];
          var text = '';
          var received = 0;
          for (var i = 1; i <= g.total; i++) {
            if (g.parts[i] != null) {
              text += g.parts[i];
              received++;
            } else {
              text += '[缺失分段' + i + ']';
            }
          }
          if (received < g.total) text = '[长短信未收齐 ' + received + '/' + g.total + '] ' + text;
          html += smsCard(g.indexes.join(','), g.status, g.sender, g.timestamp, text);
        }
      });
      return html;
    }

    window.loadSmsList = async function loadSmsList() {
      var btn = document.getElementById('smsListBtn');
      var result = document.getElementById('smsListResult');
      var limit = document.getElementById('smsListLimit').value || '20';
      btn.disabled = true;
      btn.textContent = '正在读取...';
      result.className = 'result-box result-loading';
      result.style.display = 'block';
      result.textContent = '正在读取前 ' + limit + ' 条短信，请稍候...';

      try {
        var listResp = await fetch('/smslist?limit=' + encodeURIComponent(limit));
        var listData = await listResp.json();
        if (!listData.success) throw new Error(listData.message || '索引读取失败');

        var decodedItems = [];
        for (var i = 0; i < listData.entries.length; i++) {
          var entry = listData.entries[i];
          result.textContent = '正在读取短信 ' + (i + 1) + '/' + listData.entries.length + '，索引 ' + entry.index + '...';
          try {
            var pduResp = await fetch('/smspdu?index=' + encodeURIComponent(entry.index));
            var pduData = await pduResp.json();
            if (!pduData.success) throw new Error(pduData.message || 'PDU读取失败');
            decodedItems.push({ entry: entry, decoded: decodeSmsDeliverPdu(pduData.pdu) });
          } catch (err) {
            decodedItems.push({ entry: entry, error: err.message || String(err) });
          }
        }

        result.className = 'result-box result-info';
        result.innerHTML = renderDecodedSms(listData.storageHtml, listData.limit, listData.entries, decodedItems);
      } catch (error) {
        result.className = 'result-box result-error';
        result.textContent = '请求失败: ' + error;
      } finally {
        btn.disabled = false;
        btn.textContent = '查看短信列表';
      }
    };

    function confirmPing() {
      if (confirm("确定要执行 Ping 操作吗？\n\n这将消耗少量流量。")) {
        doPing();
      }
    }

    function doPing() {
      var btn = document.getElementById('pingBtn');
      var result = document.getElementById('pingResult');

      btn.disabled = true;
      btn.textContent = '⏳ 正在 Ping...';
      result.className = 'result-box result-loading';
      result.style.display = 'block';
      result.textContent = '正在执行 Ping 操作，请稍候（最长等待30秒）...';

      fetch('/ping', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
          btn.disabled = false;
          btn.textContent = '📡 点我消耗一点流量';
          if (data.success) {
            result.className = 'result-box result-success';
            result.innerHTML = '✅ Ping 成功！<br>' + data.message;
          } else {
            result.className = 'result-box result-error';
            result.innerHTML = '❌ Ping 失败<br>' + data.message;
          }
        })
        .catch(error => {
          btn.disabled = false;
          btn.textContent = '📡 点我消耗一点流量';
          result.className = 'result-box result-error';
          result.textContent = '❌ 请求失败: ' + error;
        });
    }

    function queryFlightMode() {
      var result = document.getElementById('flightResult');
      result.className = 'result-box result-loading';
      result.style.display = 'block';
      result.textContent = '正在查询飞行模式状态...';

      fetch('/flight?action=query')
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            result.className = 'result-box result-info';
            result.innerHTML = data.message;
          } else {
            result.className = 'result-box result-error';
            result.innerHTML = '❌ 查询失败: ' + data.message;
          }
        })
        .catch(error => {
          result.className = 'result-box result-error';
          result.textContent = '❌ 请求失败: ' + error;
        });
    }

    function toggleFlightMode() {
      if (!confirm('确定要切换飞行模式吗？\n\n开启飞行模式后模组将无法收发短信。')) return;

      var btn = document.getElementById('flightBtn');
      var result = document.getElementById('flightResult');
      btn.disabled = true;
      result.className = 'result-box result-loading';
      result.style.display = 'block';
      result.textContent = '正在切换飞行模式...';

      fetch('/flight?action=toggle')
        .then(response => response.json())
        .then(data => {
          btn.disabled = false;
          if (data.success) {
            result.className = 'result-box result-success';
            result.innerHTML = '✅ ' + data.message;
          } else {
            result.className = 'result-box result-error';
            result.innerHTML = '❌ 切换失败: ' + data.message;
          }
        })
        .catch(error => {
          btn.disabled = false;
          result.className = 'result-box result-error';
          result.textContent = '❌ 请求失败: ' + error;
        });
    }

    function addLog(msg, type = 'resp') {
      var log = document.getElementById('atLog');
      var div = document.createElement('div');
      var b = document.createElement('b');

      if (type === 'user') {
        b.style.color = '#fff';
        b.textContent = '> ';
      } else if (type === 'error') {
        b.style.color = '#f44336';
        b.textContent = '❌ ';
      } else {
        b.style.color = '#4CAF50';
        b.textContent = '[RESP] ';
      }

      div.appendChild(b);
      var textNode = document.createTextNode(msg);
      div.appendChild(textNode);

      log.appendChild(div);
      log.scrollTop = log.scrollHeight;
    }

    function sendAT() {
      var input = document.getElementById('atCmd');
      var cmd = input.value.trim();
      if (!cmd) return;

      var btn = document.getElementById('atBtn');
      btn.disabled = true;
      btn.textContent = '...';

      addLog(cmd, 'user');
      input.value = '';

      fetch('/at?cmd=' + encodeURIComponent(cmd))
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            addLog(data.message);
          } else {
            addLog(data.message, 'error');
          }
        })
        .catch(error => {
          addLog('网络错误: ' + error, 'error');
        })
        .finally(() => {
          btn.disabled = false;
          btn.textContent = '发送';
        });
    }

    function clearATLog() {
      document.getElementById('atLog').innerHTML = '';
    }
    document.getElementById('atCmd').addEventListener('keydown', function(event) {
      if (event.key === 'Enter') {
        sendAT();
      }
    });
    refreshPushDebug();
  </script>
</body>
</html>
)rawliteral";

// 检查HTTP Basic认证
bool checkAuth() {
  if (!server.authenticate(config.webUser.c_str(), config.webPass.c_str())) {
    server.requestAuthentication(BASIC_AUTH, "SMS Forwarding", "请输入管理员账号密码");
    return false;
  }
  return true;
}
