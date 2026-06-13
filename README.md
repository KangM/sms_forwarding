# 低成本短信转发器复刻指南

这是一个基于 ESP32 SuperMini 和 4G 短信模组的短信转发器。设备通过 UART 控制蜂窝模组接收短信，再通过 WiFi 把短信转发到邮箱、HTTP 接口或常见机器人推送服务，同时提供一个 Web 管理页面用于配置、调试和排障。

当前仓库已经支持两套主控板：

- `ESP32-S3 SuperMini`
- `ESP32-C3 SuperMini`

默认编译目标是 `ESP32-S3 SuperMini`。如需切换到 C3，需要在构建参数中定义 `SMS_BOARD_C3`，或直接使用仓库里的脚本参数。

## 主要功能

- 接收短信并自动转发
- 支持 PDU 短信解析，支持中文短信
- 支持长短信自动拼接
- 支持短信列表查看与逐条读取 PDU
- 支持最多 5 个推送通道同时启用
- 支持 SMTP 邮件通知
- 支持全局推送过滤规则和页面内规则测试
- 支持管理员短信命令远程发短信、重启设备
- 支持号码黑名单
- 支持掉线检测 KeepAlive
- 支持 WiFi 可达性诊断、自动重连、完整 WiFi reset
- 支持串口命令台排障
- 支持运行时日志与 Flash 错误上下文日志
- 支持板载 LED 状态提示

## 需要购买的硬件

最低复刻建议准备：

| 硬件 | 建议 | 说明 |
| --- | --- | --- |
| 主控开发板 | ESP32-S3 SuperMini 或 ESP32-C3 SuperMini | 当前仓库两者都支持，默认以 S3 为主 |
| 蜂窝模组 | ML307R-DC 或其它支持 AT 指令的 4G 短信模组 | 需要支持 UART、PDU 模式、短信到达上报 |
| 4G 天线 | 与模组接口匹配 | 没有天线会影响驻网和短信收发 |
| Nano SIM 卡 | 可正常收发短信的卡 | 先用手机验证短信功能正常 |
| 杜邦线 | 若干 | 用于 UART、EN、供电接线 |
| USB 数据线 | 支持数据传输 | 用于烧录和串口监视 |
| 稳定 5V 电源 | 推荐 | 模组发射瞬间电流较高，弱 USB 口容易出问题 |

## 接线方法

当前引脚定义见 [code/pins.h](D:/文档/CodeWork/sms_forwarding/code/pins.h)。

### 方案一：ESP32-S3 SuperMini

默认宏：

```cpp
#define SMS_BOARD_S3
```

默认引脚：

```cpp
#define TXD 43
#define RXD 44
#define MODEM_EN_PIN 5
```

接线表：

| ESP32-S3 SuperMini | 4G 模组 | 说明 |
| --- | --- | --- |
| GPIO43 / TX | RX | 向模组发送 AT 指令 |
| GPIO44 / RX | TX | 接收模组响应和短信上报 |
| GPIO5 | EN | 控制模组断电/上电 |
| 5V | 5V / VCC | 模组供电 |
| GND | GND | 必须共地 |

补充说明：

- S3 板载 RGB 灯使用 `GPIO48`，不要拿它去接模组控制脚
- 当前项目默认就是按这套 S3 接线编译

### 方案二：ESP32-C3 SuperMini

默认宏：

```cpp
#define SMS_BOARD_C3
```

默认引脚：

```cpp
#define TXD 3
#define RXD 4
#define MODEM_EN_PIN 5
```

接线表：

| ESP32-C3 SuperMini | 4G 模组 | 说明 |
| --- | --- | --- |
| GPIO3 / TXD | RX | 向模组发送 AT 指令 |
| GPIO4 / RXD | TX | 接收模组响应和短信上报 |
| GPIO5 | EN | 控制模组断电/上电 |
| 5V | 5V / VCC | 模组供电 |
| GND | GND | 必须共地 |

补充说明：

- C3 板载单色 LED 在 `GPIO8`
- 当前代码里的 C3 就是这套接法，不需要你再自己猜引脚

### 通用接线注意事项

- UART 必须交叉连接：`TX -> RX`，`RX -> TX`
- 主控和模组必须共地
- 模组 EN 建议接到 ESP32 控制脚，不要直接常高
- 烧录有冲突时，可以临时断开模组 TX/RX，烧完再接回

## Arduino IDE 设置

### 1. 安装 ESP32 平台

建议使用：

```text
esp32 by Espressif Systems 3.3.10
```

### 2. 安装依赖库

在 Library Manager 中安装：

- `pdulib`
- `ReadyMail`

### 3. 打开工程

用 Arduino IDE 打开：

```text
code/code.ino
```

### 4. 开发板设置

#### ESP32-S3 SuperMini

| 选项 | 推荐值 |
| --- | --- |
| Board | `ESP32S3 Dev Module` |
| USB CDC On Boot | `Enabled` |
| Flash Size | `4MB (32Mb)` |
| Partition Scheme | `Huge APP` |
| PSRAM | `Disabled` |
| Upload Speed | `921600`，不稳就降 |

#### ESP32-C3 SuperMini

| 选项 | 推荐值 |
| --- | --- |
| Board | `ESP32C3 Dev Module` |
| USB CDC On Boot | `Enabled` |
| Flash Size | `4MB (32Mb)` |
| Partition Scheme | `Huge APP` |
| Upload Speed | `921600`，不稳就降 |

### 5. 编译宏

如果你直接在 Arduino IDE 里编译：

- S3：默认即可
- C3：需要额外定义 `SMS_BOARD_C3`

如果你的 IDE 不方便加宏，建议直接使用下面的 PowerShell 脚本。

## PowerShell 一键脚本

仓库根目录提供：

- [build-flash-monitor-s3.ps1](D:/文档/CodeWork/sms_forwarding/build-flash-monitor-s3.ps1)

虽然文件名还带 `s3`，但现在已经同时支持 S3 和 C3。

### 默认行为

```powershell
.\build-flash-monitor-s3.ps1
```

含义：

- 默认编译 `ESP32-S3 SuperMini`
- 默认串口 `COM6`
- 执行 `编译 -> 烧录 -> 打开带时间戳的串口监视器`

### 常用示例

```powershell
.\build-flash-monitor-s3.ps1 -Port COM6
.\build-flash-monitor-s3.ps1 -Board C3 -Port COM7
.\build-flash-monitor-s3.ps1 -Board C3 -Port COM7 -Clean
.\build-flash-monitor-s3.ps1 -Monitor -Port COM6
.\build-flash-monitor-s3.ps1 -Help
```

## 首次启动流程

设备上电后大致会做这些事：

- 初始化串口日志
- 初始化 `Serial1`
- 拉低/拉高 EN，对模组做一次干净重启
- 等待 `AT` 响应
- 执行 `AT+CGACT=0,1`，先禁用数据连接
- 设置短信存储到模块内部 `ME`
- 设置 `AT+CNMI=2,1,0,0,0`
- 设置 `AT+CMGF=0` 进入 PDU 模式
- 等待蜂窝网络注册
- 连接 WiFi，或进入 WiFi 配网模式
- 启动 Web 服务

## WiFi 配网

如果没有可用 WiFi，设备会启动开放热点：

```text
SMS-Setup-xxxxxx
```

连接后访问：

```text
http://192.168.4.1/wifi
```

保存 WiFi 后设备会自动重启并尝试连接。

## Web 管理页面

默认地址：

```text
http://设备IP/
```

默认账号密码：

```text
账号：admin
密码：admin123
```

建议首次进入后立刻修改。

主要页面：

| 页面 | 地址 | 用途 |
| --- | --- | --- |
| 系统配置 | `/` | 配置 Web、邮箱、推送、黑名单、管理员号码等 |
| 工具箱 | `/tools` | 短信发送、短信列表、AT 调试、模组查询、Ping 等 |
| WiFi 配网 | `/wifi` | 修改 WiFi |

## 推送与过滤

支持的推送类型包括：

- POST JSON
- Bark
- GET 请求
- 钉钉机器人
- PushPlus
- Server 酱
- 自定义模板
- 飞书机器人
- Gotify
- Telegram Bot

### 推送过滤

当前支持一套全局过滤规则：

- 匹配对象：`发信号码` / `短信内容`
- 匹配方式：`包含` / `不包含` / `开头` / `结尾`
- 表达式支持：
  - `A&&B`
  - `A||B`

限制：

- 不支持正则
- 不支持混用 `&&` 和 `||`
- 默认大小写敏感

页面里还提供了“测试规则”功能，可以直接输入测试号码和短信内容，验证当前规则是否会放行，不会真的发推送。

## 短信列表

短信列表现在不是一次把大段 `CMGL` 内容全搬进内存，而是：

- 先扫描索引
- 页面只显示最近若干条
- 逐条用 `CMGR` 读取
- 前端负责 PDU 解码显示

这样更适合长短信和短信较多的场景，也更不容易把 ESP32 栈和堆打爆。

## 串口命令台

串口监视器除了看日志，还可以直接输入命令排障。

常用命令：

```text
HELP
STATUS
PINGGW
DNS <host>
TCP <host> [port]
HTTP <url>
KADEBUG
KAAUTO [ON|OFF]
LOG
LOG 100
LOG FLASH
LOG FLASH PREV
LOG FLASH CLEAR
LOGTIME [ON|OFF]
RECONNECT
WIFIRESET
MODEMRESET
PERF
PERFRESET [seconds]
AT+...
```

说明：

- `STATUS`：看运行状态、WiFi 信息、网关 Ping、片内温度
- `WIFIRESET`：完整重建 WiFi
- `KADEBUG`：对当前 KeepAlive 目标做深度诊断
- `AT+...`：整行转发到模组
- `LOGTIME OFF`：串口输出日志不带设备 uptime 前缀，避免和监视器自己的时间戳混在一起

## 日志系统

目前日志分两层：

### Runtime Log

- 存在 RAM 环形缓冲区
- 默认容量 `200` 条
- 记录的是最近运行日志
- 时间格式是设备运行时间，例如：

```text
[00:05:23] [INFO] [KEEPALIVE] request ok code=200 cost=1379ms
```

### Flash Log

- 默认不全量落盘
- 只有遇到 `ERROR` 才触发错误上下文保存
- 保存内容包括：
  - 错误前最近若干条
  - 错误本身
  - 错误后的后续日志

当前 Flash Log 规则：

- 单个活动日志文件最大 `65536` 字节
- 文件路径：
  - `/syslog.log`
  - `/syslog.prev.log`
- 新错误上下文开始前会预留一块空间，尽量让一次错误上下文写在同一个文件里
- Flash Log 优先写真实时间；如果 NTP 还没同步成功，则回退成 `uptime HH:MM:SS`

## KeepAlive 与网络恢复

KeepAlive 现在不只是定时访问 URL，还带这些保护：

- 请求前先检查网关可达性
- WiFi 假在线时能触发重连
- 支持故障期加速复查
- 支持自动深度诊断
- 支持串口手动触发排障

这套机制的目标是尽量把“WiFi 还显示连接，但实际上已经不通”的情况尽早识别出来。

## 管理员短信命令

支持：

```text
SMS:目标号码:短信内容
RESET
```

含义：

- `SMS:...`：用模组代发短信
- `RESET`：重启模组并重启 ESP32

管理员号码比对兼容 `+86` 前缀。

## 常见问题

### 1. 烧录失败

- 降低 Upload Speed
- 断开模组 TX/RX 再烧
- 确认数据线支持传输
- 必要时按住 BOOT 再烧录

### 2. 串口一直显示 AT 未响应

- 检查供电
- 检查共地
- 检查 UART 交叉连接
- 检查 EN 是否接对
- 确认模组 AT 波特率是 `115200`

### 3. 等待网络注册很久

- 检查 SIM 卡
- 检查天线
- 换信号更好的位置
- 在工具页查询信号质量和网络状态

### 4. 收到短信但没转发

- 检查 WiFi 是否已连接
- 检查通知通道是否配置完整
- 检查是否被黑名单或推送过滤规则拦截
- 用 `/tools` 里的推送调试与规则测试定位

## 项目结构

```text
code/
  code.ino
  pins.h
  logger.h
  keep_alive.h
  wifi_provision.h
  modem_sms_control.h
  sms_receive.h
  sms_concat.h
  push_channels.h
  serial_console.h
  diagnostics_utils.h
  config_core.h
  web_*_handler.h
docs/
  board-pin-reference.md
archive/
  README.legacy-20260612.md
build-flash-monitor-s3.ps1
```

## 安全提醒

- 默认 Web 密码一定要改
- 不要把管理页面直接暴露到公网
- Bot Token、SMTP 授权码、推送 URL 都是敏感信息
- AT 调试能力很强，只建议在可信局域网内使用
