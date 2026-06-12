#pragma once

// Build with -DSMS_BOARD_C3 or -DSMS_BOARD_S3.
// Arduino IDE users get the current default board without extra build flags.
#if !defined(SMS_BOARD_C3) && !defined(SMS_BOARD_S3)
#define SMS_BOARD_S3
#endif

#if defined(SMS_BOARD_C3)
// ESP32-C3 SuperMini UART and modem control pins.
#define TXD 3
#define RXD 4
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
