# 低成本短信转发器

> 当前分支为新方案，老方案请前往[luatos分支](https://github.com/chenxuuu/sms_forwarding/tree/old-luatos)。  
本项目仅用于接收短信与进行保号相关功能。  
多卡控制、通话、拨号、开放接口、自动化等功能永远不会支持，请勿提出相关需求。

[后台页面演示](https://sms.j2.cx/)

本项目旨在使用低成本的硬件设备，实现短信的自动转发功能，支持多种推送方式同时启用。

> 视频教程：[B站视频](https://www.bilibili.com/video/BV1cSmABYEiX)

<img src="assets/photo.png" width="200" />

## 功能

- 支持使用通用AT指令与模块进行通信
- 开启后支持通过WEB界面配置短信转发参数、查询当前状态
- **支持多达5个推送通道同时启用**，每个通道可独立配置
- 支持将收到的短信转发到指定的邮箱
- 支持通过WEB界面主动发送短信，以便消耗余额
- 支持通过WEB界面进行Ping测试，以极低的成本消耗余额
- 支持长短信自动合并（30秒超时）
- 支持管理员短信远程发送短信和重启设备

## 推送通道支持

支持以下7种推送方式，可同时启用多个通道：

| 推送方式 | 说明 | 需要配置 |
|---------|------|---------|
| **POST JSON** | 通用HTTP POST | URL |
| **Bark** | iOS推送服务 | Bark服务器URL |
| **GET请求** | URL参数方式 | URL |
| **钉钉机器人** | 企业群通知 | Webhook URL，可选Secret加签 |
| **PushPlus** | 微信公众号推送 | Token |
| **Server酱** | 微信推送服务 | SendKey |
| **自定义模板** | 灵活的JSON模板 | URL + 请求体模板 |
| **飞书机器人** | 自定义通知 | Webhook URL |

### 推送格式说明

- **POST JSON**: `{"sender":"发送者号码","message":"短信内容","timestamp":"时间戳"}`
- **Bark**: `{"title":"发送者号码","body":"短信内容"}`
- **GET请求**: `URL?sender=xxx&message=xxx&timestamp=xxx`（自动URL编码）
- **钉钉机器人**: 文本消息格式，支持加签验证
- **PushPlus**: 使用Token推送，支持HTML格式
- **Server酱**: 使用SendKey推送，支持Markdown格式
- **自定义模板**: 使用`{sender}`、`{message}`、`{timestamp}`占位符
- **飞书机器人**: 文本消息格式，支持加签验证

|状态信息|主动ping|
|-|-|
|![](assets/status.png)|![](assets/ping.png)|

## 硬件搭配

如果希望自行焊接硬件，参考下面的硬件搭配，总成本约¥27.8，仅支持移动/联通卡。

- ESP32C3开发板，当前选用[ESP32C3 Super Mini](https://item.taobao.com/item.htm?id=852057780489&skuId=5813710390565)，¥9.5包邮
- ML307R-DC开发板，当前选用[小蓝鲸ML307R-DC核心板](https://item.taobao.com/item.htm?id=797466121802&skuId=5722077108045)，¥16.3包邮
- [4G FPC天线](https://item.taobao.com/item.htm?id=797466121802&skuId=5722077108045)，¥2，与核心板同购

若希望直接使用成品，可选直接购以下套件，支持移动/联通/电信卡：

- [小蓝鲸WIFI短信宝](https://item.taobao.com/item.htm?id=1003711355912)
- [4G FPC天线](https://item.taobao.com/item.htm?id=1003711355912&skuId=6162872574943)，与开发板同购

## 硬件连接

ESP32C3 与 ML307R-DC 通过串口（UART）连接，接线如下：

```
┌───────────────────────────────────────────────┐
|                                               |
|   ESP32C3 Super Mini      ML307R-DC核心板     |
| ┌───────────────────┐    ┌─────────────────┐ |
└─┼─ GPIO5 (MODEM_EN) │    │                 │ |
  │       GPIO3 (TX) ─┼───►│ RX              │ |
  │                   │    │             EN ─┼─┘
  │       GPIO4 (RX) ◄┼────┤ TX              │ 
  │                   │    │                 │ 
  │              GND ─┼────┤ GND             │ 
  │                   │    │                 │ 
  │               5V ─┼────┤ VCC (5V)        |
  │                   │    │                 │
  └───────────────────┘    └─────────────────┘
                           │                 │
                           │  SIM卡槽        │
                           │  (插入Nano SIM) │
                           │                 │
                           │  天线接口       │
                           │  (连接4G天线)   │
                           └─────────────────┘
```
改变接线方式，核心板不再和en短接而是和esp32c3的GPIO5连接，使模块能够被控制上下电(代码也同步改动)。
可通过USB连接ESP32C3进行编程和供电，正常工作时，ESP32C3的虚拟串口数据将直接被转发到ML307R-DC，方便调试。

## 软件组成

- ESP32C3运行自己的`Arduino`固件，负责连接WiFi和接收ML307R-DC发送过来的短信数据，然后转发到指定HTTP接口或邮箱
- ML307R-DC运行默认的AT固件，不用动

需要在`Arduino IDE`中单独安装这些库：

- **ReadyMail** by Mobizt
- **pdulib** by David Henry

需要在`Arduino IDE`中安装ESP32开发板支持，参考[官方文档](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)，版型选`MakerGO ESP32 C3 SuperMini`。

## ESP32-C3 原项目引脚说明

原项目面向 ESP32C3 Super Mini 开发板，代码中的默认串口和模块控制引脚如下：

```cpp
#define TXD 3
#define RXD 4
#define MODEM_EN_PIN 5
```

对应接线关系：

| ESP32C3 Super Mini | ML307R-DC 模块 | 说明 |
| --- | --- | --- |
| GPIO3 / TX | RX | ESP32C3 向模块发送 AT 数据 |
| GPIO4 / RX | TX | ESP32C3 接收模块返回数据 |
| GPIO5 | EN | 控制模块断电/上电重启 |
| GND | GND | 共地 |
| 5V | VCC / 5V | 模块供电 |

注意：串口 TX/RX 需要交叉连接，即开发板 TX 接模块 RX，开发板 RX 接模块 TX。

## 当前 ESP32-S3 SuperMini 使用说明

当前分支已改为适配手头的 ESP32-S3 SuperMini 开发板。代码中的引脚配置如下：

```cpp
#define TXD 43
#define RXD 44
#define MODEM_EN_PIN 5

#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif
```

对应接线关系：

| ESP32-S3 SuperMini | ML307R-DC 模块 | 说明 |
| --- | --- | --- |
| GPIO43 / TX | RX | ESP32-S3 向模块发送 AT 数据 |
| GPIO44 / RX | TX | ESP32-S3 接收模块返回数据 |
| GPIO5 | EN | 控制模块断电/上电重启 |
| GND | GND | 共地 |
| 5V | VCC / 5V | 模块供电 |

如果烧录时失败，可以先断开 ML307R-DC 的 TX/RX，烧录完成后再接回模块并复位开发板。

## Arduino IDE 配置说明

当前开发板建议在 Arduino IDE 中使用以下配置：

| 配置项 | 推荐值 |
| --- | --- |
| 开发板 | `ESP32S3 Dev Module` |
| Flash Size | `4MB (32Mb)` |
| Partition Scheme | `Huge APP (3MB No OTA/1MB SPIFFS)` 或同类 `Huge APP / No OTA` 选项 |
| USB CDC On Boot | `Enabled` |
| PSRAM | `Disabled`，除非确认开发板带 PSRAM |
| CPU Frequency | `240MHz` |
| Upload Speed | `921600`，不稳定时改为 `115200` 或 `460800` |
| 串口监视器波特率 | `115200` |

选择 `Huge APP` 的原因是当前固件功能较多，默认 OTA 分区下单个 app 分区约 1.25MB，剩余空间较紧张。`Huge APP` 会取消 OTA 双分区，把更多 Flash 空间分配给主程序，适合通过 USB 线烧录的使用方式。

切换分区方案后，建议第一次上传时在 Arduino IDE 中启用：

```text
Tools -> Erase All Flash Before Sketch Upload -> Enabled
```

这会清空 ESP32 Flash 中的运行时配置，例如 Web 管理密码、推送通道、邮箱配置等，上传完成后需要重新进入网页配置。
