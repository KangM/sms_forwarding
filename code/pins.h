#pragma once

// Build with -DSMS_BOARD_C3, -DSMS_BOARD_C3_LUAOS, or -DSMS_BOARD_S3.
// Arduino IDE users get the current default board without extra build flags.
#if !defined(SMS_BOARD_C3) && !defined(SMS_BOARD_C3_LUAOS) && !defined(SMS_BOARD_S3)
#define SMS_BOARD_S3
#endif

#if defined(SMS_BOARD_C3_LUAOS)
// 合宙 ESP32C3-CORE 开发板（经典款 CH343 USB转串口版本）
// 注意：GPIO20/21 在 PCB 上连接 CH343 芯片(UART0)，模组必须避开这两个引脚。
// 模组改用 GPIO0(TX)+GPIO1(RX)，对应 ESP32-C3 默认 UART1 引脚。
#define TXD 0
#define RXD 1
#define MODEM_EN_PIN 5

#ifndef LED_BUILTIN
#define LED_BUILTIN 12  // 合宙板载 D4 LED（高电平点亮）
#endif

#define LED_ACTIVE_HIGH  // 合宙板载 LED 高电平点亮（与 SuperMini 的 LOW 点亮不同）

#elif defined(SMS_BOARD_C3)
// ESP32-C3 SuperMini UART and modem control pins.
#define TXD 21
#define RXD 20
#define MODEM_EN_PIN 5

#ifndef LED_BUILTIN
#define LED_BUILTIN 8
#endif

#elif defined(SMS_BOARD_S3)
// ESP32-S3 SuperMini UART and modem control pins.
#define TXD 43
#define RXD 44
#define MODEM_EN_PIN 5

#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

// S3 板载是 GP48 上的 WS2812 可寻址 RGB 灯，需用 rgbLedWrite 驱动。
#define LED_IS_RGB

#endif
