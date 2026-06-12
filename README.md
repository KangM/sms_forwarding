# 低成本短信转发器复刻指南

这是一个基于 ESP32-S3 SuperMini 和蜂窝通信模组的短信转发器。设备接入 WiFi 后，会通过串口控制 4G 模组接收短信，并把短信转发到邮箱、HTTP 接口或常见机器人推送服务。项目也提供 Web 管理页面，可配置通知通道、查看短信、发送短信、查询模组状态和调试 AT 指令。

当前代码默认适配 `ESP32-S3 SuperMini + ML307R-DC` 这类支持 AT 指令的蜂窝模组组合。旧的 ESP32-C3 接线方案仅作为历史参考，不是当前主线。
未额外指定编译宏时，固件默认按 ESP32-S3 SuperMini 引脚编译；如需构建 ESP32-C3 版本，可在构建参数中定义 `SMS_BOARD_C3`。

## 功能概览

- 接收短信并自动转发。
- 支持 PDU 短信解析，支持中文短信。
- 支持长短信自动合并，默认最长等待 30 秒。
- 支持最多 5 个推送通道同时启用。
- 支持邮箱通知。
- 支持 Web 页面配置 WiFi、通知通道、管理员号码和号码黑名单。
- 支持 Web 页面主动发送短信。
- 支持查看模组短信存储中的短信列表。
- 支持查询固件信息、信号质量、SIM 卡信息、网络状态、WiFi 状态。
- 支持 Ping 测试，用极少量流量验证移动网络，可自定义目标地址和包大小。
- 支持飞行模式切换和查询。
- 支持 Web 页面发送 AT 指令进行调试。
- 支持管理员短信命令远程发送短信和重启设备。
- 支持掉线检测：按固定间隔向指定 URL 发起 HTTP 请求，可用于外部监控设备是否在线。
- 支持板载 LED 状态指示，不同运行阶段显示不同状态（S3 用颜色，C3 用闪烁节奏）。
- 模组短信默认存储在模块内部 `ME`，减少占用 SIM 卡短信容量。
- 上电后会执行 `AT+CGACT=0,1` 禁用数据连接，降低误跑流量的风险。

## 需要购买的硬件

最低复刻需要以下硬件：

| 硬件 | 建议 | 说明 |
| --- | --- | --- |
| 主控开发板 | ESP32-S3 SuperMini | 代码默认使用 ESP32-S3，串口引脚为 GPIO43/GPIO44。 |
| 蜂窝通信模组 | ML307R-DC 开发板或兼容 AT 指令的 4G 短信模组 | 需要支持 UART AT 指令、PDU 短信模式和短信上报。 |
| 4G 天线 | 与模组接口匹配的 FPC 或棒状天线 | 没有天线会明显影响注册网络和短信收发。 |
| Nano SIM 卡 | 移动、联通或电信卡，视模组频段支持而定 | 建议先用手机确认 SIM 卡能正常收发短信。 |
| 杜邦线 | 若干 | 用于连接 UART、供电和 EN 控制脚。 |
| USB 数据线 | 支持数据传输 | 用于给 ESP32-S3 烧录和串口日志调试。 |
| 稳定 5V 电源 | 可选但推荐 | 蜂窝模组发射瞬间电流较高，电脑 USB 口不稳定时建议外接供电。 |

如果购买的是已经把 ESP32 和 4G 模组集成在一起的成品板，也可以使用本固件，但需要核对该板子的 UART 和 EN 引脚，并同步修改 [code/pins.h](./code/pins.h)。

## 硬件接线

当前默认引脚定义在 [code/pins.h](./code/pins.h)：

```cpp
#define TXD 43
#define RXD 44
#define MODEM_EN_PIN 5
```

按照下表接线：

| ESP32-S3 SuperMini | ML307R-DC / 4G 模组 | 说明 |
| --- | --- | --- |
| GPIO43 / TX | RX | ESP32-S3 向模组发送 AT 指令。 |
| GPIO44 / RX | TX | ESP32-S3 接收模组返回和短信上报。 |
| GPIO5 | EN | 控制模组断电/上电重启。 |
| 5V | VCC / 5V | 给模组供电。 |
| GND | GND | 必须共地。 |

注意事项：

- TX/RX 必须交叉连接：ESP32 的 TX 接模组 RX，ESP32 的 RX 接模组 TX。
- ESP32 和模组必须共地，否则串口通信会不稳定。
- 如果烧录失败，可以先断开模组的 TX/RX，烧录完成后再接回并重启。
- 模组 EN 不建议直接短接到高电平，当前代码会通过 GPIO5 对模组做上电复位。
- 插入 Nano SIM 卡并接好 4G 天线后再上电测试。

## Arduino IDE 环境准备

### 1. 安装 ESP32 开发板支持

在 Arduino IDE 中安装 Espressif 的 ESP32 开发板包。当前构建记录使用的是：

```text
esp32 by Espressif Systems 3.3.10
```

其它 3.x 版本通常也可以使用。如果遇到编译差异，优先使用 3.3.10 或接近版本。

### 2. 安装依赖库

在 Arduino IDE 的 Library Manager 中安装：

| 库 | 作者 | 用途 |
| --- | --- | --- |
| ReadyMail | Mobizt | SMTP 邮件通知。 |
| pdulib | David Henry | PDU 短信编码和解码。 |

ESP32 核心自带的 `WiFi`、`WebServer`、`Preferences`、`HTTPClient`、`NetworkClientSecure` 等库不需要单独安装。

### 3. 打开工程

用 Arduino IDE 打开：

```text
code/code.ino
```

不要只打开单个 `.h` 文件。主程序会自动包含同目录下的其它头文件。

### 4. 选择开发板和编译选项

推荐配置如下：

| Arduino IDE 选项 | 推荐值 |
| --- | --- |
| Board | `ESP32S3 Dev Module` |
| USB CDC On Boot | `Enabled` |
| CPU Frequency | `240MHz (WiFi)` |
| Flash Size | `4MB (32Mb)` |
| Partition Scheme | `Huge APP (3MB No OTA/1MB SPIFFS)` 或同类 Huge APP / No OTA 方案 |
| PSRAM | `Disabled`，除非你的板子明确带 PSRAM |
| Upload Speed | `921600`，不稳定时改为 `460800` 或 `115200` |
| Port | 选择 ESP32-S3 对应串口 |
| Serial Monitor Baud | `115200` |

建议首次烧录或修改分区方案后启用：

```text
Tools -> Erase All Flash Before Sketch Upload -> Enabled
```

这会清空设备中已保存的 WiFi、Web 密码、邮箱和推送通道配置。烧录完成并配置正常后，可以再改回 Disabled。

## 烧录和首次启动

1. 按照接线表连接 ESP32-S3 和模组。
2. 插入 SIM 卡，接好 4G 天线。
3. 用 USB 数据线连接 ESP32-S3 到电脑。
4. 在 Arduino IDE 中打开 [code/code.ino](./code/code.ino)。
5. 按上面的参数选择开发板和工具菜单选项。
6. 点击 Upload 烧录。
7. 打开 Serial Monitor，波特率选择 `115200`。
8. 等待串口输出模组 AT 响应、短信模式配置、网络注册和 WiFi 状态。

设备启动时会依次做这些事情：

- 初始化 USB 串口日志。
- 初始化与模组通信的 `Serial1`，波特率 `115200`。
- 通过 GPIO5 对模组执行断电重启。
- 等待模组 `AT` 响应。
- 禁用数据连接，降低误耗流量风险。
- 设置短信存储为模块内部 `ME`。
- 开启短信到达上报。
- 切换到 PDU 模式。
- 等待 4G 网络注册。
- 连接已保存 WiFi，或启动 WiFi 配网页面。
- 启动 Web 管理服务，默认 HTTP 端口为 80。

## WiFi 配网

如果设备没有保存过 WiFi，或连接失败，会自动开启开放热点：

```text
SMS-Setup-xxxxxx
```

连接这个热点后，浏览器访问：

```text
http://192.168.4.1/wifi
```

也可以直接打开任意网页，设备会尝试重定向到配网页面。选择或填写 WiFi 名称和密码后保存，设备会自动重启并尝试连接。

连接成功后，从串口监视器查看设备 IP，然后访问：

```text
http://设备IP/
```

## Web 管理页面

默认账号密码：

```text
账号：admin
密码：admin123
```

首次进入后请立刻修改默认密码。

主要页面：

| 页面 | 地址 | 用途 |
| --- | --- | --- |
| 系统配置 | `/` | 配置 Web 账号、邮箱、推送通道、管理员号码、黑名单。 |
| 工具箱 | `/tools` | 发送短信、查看短信、查询模组状态、Ping、飞行模式、AT 调试。 |
| WiFi 配网 | `/wifi` | 修改或重新保存 WiFi。 |

## 通知通道

短信到达后，设备会同时尝试所有配置完整且已启用的通知通道。至少需要配置完整的邮箱通知，或启用一个有效推送通道。

支持的推送类型：

| 类型 | 需要填写 | 说明 |
| --- | --- | --- |
| POST JSON | URL | POST `{"sender":"...","message":"...","timestamp":"..."}`。 |
| Bark | URL | POST `{"title":"发送者号码","body":"短信内容"}`。 |
| GET 请求 | URL | 自动追加 `sender`、`message`、`timestamp` 参数。 |
| 钉钉机器人 | Webhook，可选 Secret | 支持加签。 |
| PushPlus | Token，可选 URL 和发送渠道 | URL 留空时使用默认接口；渠道支持 `wechat`、`extension`、`app`。 |
| Server 酱 | SendKey，可选 URL | URL 留空时使用默认接口。 |
| 自定义模板 | URL 和请求体模板 | 模板可使用 `{sender}`、`{message}`、`{timestamp}`。 |
| 飞书机器人 | Webhook，可选 Secret | 支持签名。 |
| Gotify | 服务器 URL 和应用 Token | 向 Gotify 应用推送消息。 |
| Telegram Bot | Chat ID 和 Bot Token，可选 API URL | URL 留空时使用 Telegram 官方 API。 |

工具箱中的“推送测试与调试”可以打开推送日志、发送测试推送、查看 HTTP 请求体和响应内容。

## 邮箱通知

邮箱通知需要填写：

- SMTP 服务器，例如 `smtp.qq.com`。
- SMTP 端口，常见 SSL 端口为 `465`。
- 发件邮箱账号。
- 邮箱密码或授权码。
- 收件邮箱地址。

设备启动且通知配置有效时，会发送一封启动通知邮件。收到短信时，也会发送邮件通知。

## 管理员短信命令

在系统配置中填写管理员手机号后，该号码发来的特定短信会被识别为远程命令。

支持命令：

```text
SMS:目标号码:短信内容
```

向指定号码发送短信。

```text
RESET
```

重启 4G 模组和 ESP32。

管理员号码比较时会兼容 `+86` 前缀，例如 `+8613800138000` 和 `13800138000` 会视为同一个号码。

## 号码黑名单

系统配置页面可以填写号码黑名单，每行一个号码。来自黑名单号码的短信会被忽略，不会触发邮件或推送。

同样兼容 `+86` 前缀匹配。

## 掉线检测

系统配置页面提供“掉线检测”功能，开启后设备会通过 WiFi 按固定间隔向指定 URL 发起 HTTP 请求，可配合外部监控服务（如 UptimeKuma 的 Push 监控、自建心跳接口等）判断设备是否在线。

可配置项：

| 配置 | 说明 |
| --- | --- |
| 功能开关 | 默认关闭。 |
| 请求 URL | 要请求的地址。 |
| 请求方式 | `GET` 或 `POST`。 |
| 请求内容 | `POST` 时作为请求体发送，`Content-Type` 为 `application/json`。 |
| 间隔时间 | 单位秒，默认 5 秒，最小 1 秒。 |

开启并连接 WiFi 后，设备会先立即发起一次请求，之后按设定间隔重复执行。

## LED 状态指示

板载 LED 会根据设备运行阶段显示不同状态，方便在没有串口的情况下判断设备处于哪个环节。ESP32-S3 SuperMini 板载的是 GP48 上的 WS2812 RGB 灯，用颜色区分状态；ESP32-C3 SuperMini 板载的是 GP8 单色 LED，用闪烁节奏区分。

| 阶段 | 含义 | S3 颜色 | 闪烁节奏 |
| --- | --- | --- | --- |
| 上电初始化 | 系统启动中 | 白 | 常亮 |
| 模组初始化 | 配置 4G 模组 | 紫 | 慢闪 |
| 等待网络注册 | 等待蜂窝网络 | 蓝 | 慢闪 |
| WiFi 连接中 | 正在连接 WiFi | 青 | 快闪 |
| 配网门户 | 已开启配网热点 | 黄 | 双闪 |
| 配置无效 | 没有可用的通知通道 | 红 | 快闪 |
| 正常待机 | 运行中 | 绿 | 心跳（每 3 秒短闪一次） |
| 转发短信中 | 正在推送/发邮件 | 亮绿 | 常亮 |

> 提示：板上的 BATTERY LED 由充电管理芯片直连，仅反映电池充电状态，无法由程序控制。纯 USB / 外部供电且未接电池时该灯可能持续闪烁，属于正常现象。

## 常见问题

### 烧录失败

- 确认 Arduino IDE 选择的是 `ESP32S3 Dev Module`。
- 确认 USB 线支持数据传输。
- 降低 Upload Speed 到 `460800` 或 `115200`。
- 临时断开模组 TX/RX，烧录完成后再接回。
- 必要时按住开发板 BOOT 键再开始烧录。

### 串口一直显示 AT 未响应

- 检查模组供电是否稳定。
- 检查 GND 是否共地。
- 检查 TX/RX 是否交叉连接。
- 检查 EN 是否接到 ESP32-S3 的 GPIO5。
- 确认模组 AT 串口波特率为 `115200`。

### 等待网络注册很久

- 确认 SIM 卡未欠费，且能正常收发短信。
- 确认天线已接好。
- 换到信号更好的位置。
- 使用工具箱查询信号质量和网络状态。
- 确认模组支持当前运营商频段。

### Web 页面打不开

- 从串口监视器确认设备 IP。
- 确认电脑或手机与设备在同一个 WiFi 网络。
- 如果刚清空 WiFi 或连接失败，连接 `SMS-Setup-xxxxxx` 热点并访问 `http://192.168.4.1/wifi`。

### 收到短信但没有推送

- 确认设备 WiFi 已连接。
- 确认至少配置了一个完整的邮箱或推送通道。
- 在 `/tools` 中开启推送日志，并点击“测试推送”查看响应码和响应内容。
- 如果使用钉钉、飞书等加签服务，确认 Secret 填写正确。

## 修改引脚或适配其它开发板

如果你的 ESP32-S3 板子 UART 引脚不同，修改 [code/pins.h](./code/pins.h)：

```cpp
#define TXD 43
#define RXD 44
#define MODEM_EN_PIN 5
```

修改后重新编译烧录，并按新的引脚接线。

如果更换为 ESP32-C3 或其它 ESP32 板卡，还需要在 Arduino IDE 中选择对应开发板，并确认可用串口引脚、启动脚限制和板载 LED 引脚。

## 项目结构

```text
code/
  code.ino                  主程序入口
  pins.h                    ESP32-S3 与模组的引脚定义
  config_types.h            配置结构和推送通道类型
  config_core.h             配置保存、加载和校验
  wifi_provision.h          WiFi 配网门户
  push_channels.h           多通道推送实现
  email_notify.h            邮件通知
  sms_receive.h             短信接收和 PDU 解析
  sms_concat.h              长短信合并
  modem_sms_control.h       短信发送、模组重启、管理员命令
  keep_alive.h              掉线检测（定时 HTTP 请求）
  led_indicator.h           跨板 LED 状态指示
  web_pages.h               Web 页面模板
  web_*_handler.h           Web 请求处理
assets/                     README 使用的图片资源
archive/                    旧文档归档
```

## 安全提醒

- Web 管理默认密码必须修改。
- 不要把公网端口直接暴露到设备 Web 管理页面。
- 自定义推送 URL、邮箱授权码、Bot Token 都属于敏感信息。
- AT 指令调试入口能力很强，只应在可信局域网内使用。
- Ping 和部分网络测试会消耗少量 SIM 卡流量。
