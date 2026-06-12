# Board Pin Reference

This note records the pinout information extracted from the user-provided ESP32-S3 SuperMini and ESP32-C3 SuperMini images. Use it when planning alternate UARTs, a second modem, or cross-board pin mappings.

## ESP32-S3 SuperMini

Current project defaults:

| Purpose | GPIO |
| --- | --- |
| Modem UART TX | GPIO43 |
| Modem UART RX | GPIO44 |
| Modem EN | GPIO5 |
| Board RGB LED / WS2812 DIN | GPIO48 |

### Conveniently Exposed Pins

Front/top-side convenient pins shown in the image:

| Pin | Notes shown in image |
| --- | --- |
| TX | UART0 TX label |
| RX | UART0 RX label |
| GPIO1 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO2 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO3 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO4 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO5 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO6 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO7 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO8 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO9 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO10 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO11 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO12 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO13 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO48 | WS2812 RGB LED DIN; also shown with SPI/I2C/UART capability on underside view |

Back/underside or edge pins shown in the image:

| Pin | Notes shown in image |
| --- | --- |
| GPIO14 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO15 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO16 | UART, PWM, I2S, ADC, I2C, SPI |
| GPIO17 | GPIO label shown |
| GPIO18 | GPIO label shown |
| GPIO21 | GPIO label shown |
| GPIO33 | SPI, I2C |
| GPIO34 | SPI, I2C |
| GPIO35 | SPI, I2C, UART |
| GPIO36 | SPI, I2C, UART |
| GPIO37 | SPI, I2C |
| GPIO38 | SPI, I2C |
| GPIO39 | SPI, I2C, I2S, PWM, UART |
| GPIO40 | SPI, I2C, I2S, PWM, UART |
| GPIO41 | SPI, I2C, I2S, PWM, UART |
| GPIO42 | SPI, I2C, I2S, PWM, UART |
| GPIO45 | SPI, I2C, I2S, PWM, UART |
| GPIO46 | SPI, I2C, I2S, PWM, UART |
| GPIO47 | SPI, I2C, I2S, PWM, UART |
| GPIO48 | SPI, I2C, I2S, PWM, UART; also RGB LED DIN |

### Power / Control Pads

| Label | Notes |
| --- | --- |
| 5V | Power input/output pad shown |
| GND | Ground |
| 3V3 / 3V3(OUT) | 3.3 V output |
| BAT / B+ / B- | Battery pads shown |
| BOOT | Boot button/pad |
| RESET | Reset button/pad |
| BOOST | Charging current selection jumper; image warns this is for large batteries only |

## ESP32-C3 SuperMini

Current project defaults:

| Purpose | GPIO |
| --- | --- |
| Modem UART TX | GPIO3 |
| Modem UART RX | GPIO4 |
| Modem EN | GPIO5 |
| Board LED | GPIO8 |

### Conveniently Exposed Pins

| Pin | Aliases / functions shown in images |
| --- | --- |
| GPIO0 | A0 |
| GPIO1 | A1 |
| GPIO2 | A2 |
| GPIO3 | A3 |
| GPIO4 | A4, SCK |
| GPIO5 | A5, MISO |
| GPIO6 | MOSI |
| GPIO7 | SS |
| GPIO8 | SDA |
| GPIO9 | SCL |
| GPIO10 | GPIO only shown |
| GPIO20 | RX |
| GPIO21 | TX |

Arduino-style constants shown in the image:

```cpp
static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS   = 7;
static const uint8_t MOSI = 6;
static const uint8_t MISO = 5;
static const uint8_t SCK  = 4;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;
```

### Power Pins

| Label | Notes |
| --- | --- |
| 5V | Power |
| GND | Ground |
| 3V3 | 3.3 V |

## Planning Notes

- For a second modem on ESP32-S3, prefer using exposed GPIOs that are not already assigned to the first modem, EN control, or LED.
- Avoid using GPIO48 for modem control on S3 because it is the onboard WS2812 RGB LED data pin in the provided diagram.
- The current ESP32-S3 modem uses GPIO43/GPIO44 in code, but these are not among the easiest front-side GPIO1-GPIO13 pads shown in the first image. If physical wiring convenience matters, a future board profile can remap the primary or secondary modem UART to easier pads.
- ESP32-C3 has far fewer convenient pins. A dual-modem design should probably be S3-first, with C3 either single-modem only or requiring a reduced feature profile.
