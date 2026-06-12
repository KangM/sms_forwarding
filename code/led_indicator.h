#pragma once

// 跨板 LED 状态指示封装。
// S3：GP48 上的 WS2812 可寻址 RGB 灯，用 rgbLedWrite 驱动，用颜色区分状态。
// C3：GP8 普通单色 LED，低电平点亮，用闪烁节奏区分状态。
// 上层只调用 ledSetState() 设置语义状态，由 ledTick() 在 loop 中非阻塞驱动。

// 设备运行阶段对应的 LED 语义状态
enum LedState {
  LED_BOOTING,          // 上电/系统初始化
  LED_MODEM_INIT,       // 模组初始化中
  LED_WAIT_CELLULAR,    // 等待蜂窝网络注册
  LED_WIFI_CONNECTING,  // WiFi 连接中
  LED_WIFI_PORTAL,      // 配网门户已开启
  LED_ERROR,            // 配置无效（无可用通知通道）
  LED_RUNNING_IDLE,     // 正常运行待机（心跳）
  LED_BUSY_PUSHING      // 正在转发/推送短信
};

// 闪烁节奏类型
enum LedPattern {
  LED_PATTERN_SOLID,        // 常亮
  LED_PATTERN_BLINK_SLOW,   // 慢闪（1s 周期）
  LED_PATTERN_BLINK_FAST,   // 快闪（200ms 周期）
  LED_PATTERN_HEARTBEAT,    // 心跳：每 3s 短闪一次
  LED_PATTERN_DOUBLE_BLINK  // 双闪：两次快闪后停顿
};

struct LedStyle {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  LedPattern pattern;
};

// 各状态的颜色（仅 RGB 板使用）和节奏。亮度取较低值避免刺眼并省电。
static LedStyle ledStyleFor(LedState state) {
  switch (state) {
    case LED_BOOTING:         return { 40, 40, 40, LED_PATTERN_SOLID };
    case LED_MODEM_INIT:      return { 40,  0, 40, LED_PATTERN_BLINK_SLOW };
    case LED_WAIT_CELLULAR:   return {  0,  0, 60, LED_PATTERN_BLINK_SLOW };
    case LED_WIFI_CONNECTING: return {  0, 40, 40, LED_PATTERN_BLINK_FAST };
    case LED_WIFI_PORTAL:     return { 50, 35,  0, LED_PATTERN_DOUBLE_BLINK };
    case LED_ERROR:           return { 60,  0,  0, LED_PATTERN_BLINK_FAST };
    case LED_RUNNING_IDLE:    return {  0, 40,  0, LED_PATTERN_HEARTBEAT };
    case LED_BUSY_PUSHING:    return {  0, 60,  0, LED_PATTERN_SOLID };
    default:                  return {  0,  0,  0, LED_PATTERN_SOLID };
  }
}

static LedState ledCurrentState = LED_BOOTING;
// 进入正常运行后的"常态"，供推送结束后恢复（运行/配置无效/配网三选一）
static LedState ledNormalState = LED_RUNNING_IDLE;
static bool ledPhysicalOn = false;

// 物理点亮/熄灭，屏蔽板间差异
static void ledRenderOn(const LedStyle& style) {
#if defined(LED_IS_RGB)
  rgbLedWrite(LED_BUILTIN, style.r, style.g, style.b);
#else
  digitalWrite(LED_BUILTIN, LOW);  // C3 低电平点亮
#endif
  ledPhysicalOn = true;
}

static void ledRenderOff() {
#if defined(LED_IS_RGB)
  rgbLedWrite(LED_BUILTIN, 0, 0, 0);
#else
  digitalWrite(LED_BUILTIN, HIGH);
#endif
  ledPhysicalOn = false;
}

void ledInit() {
#if !defined(LED_IS_RGB)
  pinMode(LED_BUILTIN, OUTPUT);
#endif
  ledRenderOff();
}

// 设置语义状态并立即渲染首帧（保证阻塞操作期间也能看到状态变化）
void ledSetState(LedState state) {
  ledCurrentState = state;
  LedStyle style = ledStyleFor(state);
  if (style.pattern == LED_PATTERN_SOLID) {
    ledRenderOn(style);
  } else {
    // 闪烁类状态先点亮首帧，后续由 ledTick 接管
    ledRenderOn(style);
  }
}

// 记录正常运行常态，并切换到该状态
void ledSetNormalState(LedState state) {
  ledNormalState = state;
  ledSetState(state);
}

// 推送等临时状态结束后恢复常态
void ledRestoreNormal() {
  ledSetState(ledNormalState);
}

// 在 loop 中频繁调用：根据当前状态的节奏非阻塞地刷新 LED
void ledTick() {
  LedStyle style = ledStyleFor(ledCurrentState);
  unsigned long now = millis();
  bool on = true;

  switch (style.pattern) {
    case LED_PATTERN_SOLID:
      on = true;
      break;
    case LED_PATTERN_BLINK_SLOW:
      on = (now % 1000) < 500;
      break;
    case LED_PATTERN_BLINK_FAST:
      on = (now % 200) < 100;
      break;
    case LED_PATTERN_HEARTBEAT: {
      // 每 3s 周期内前 60ms 点亮，其余熄灭
      on = (now % 3000) < 60;
      break;
    }
    case LED_PATTERN_DOUBLE_BLINK: {
      // 1.2s 周期内：0-100ms 亮, 100-250 灭, 250-350 亮, 其余灭
      unsigned long p = now % 1200;
      on = (p < 100) || (p >= 250 && p < 350);
      break;
    }
  }

  if (on && !ledPhysicalOn) {
    ledRenderOn(style);
  } else if (!on && ledPhysicalOn) {
    ledRenderOff();
  }
}

// 阻塞式延时，期间持续刷新 LED 节奏（替代旧的 blink_short，用于 setup 重试循环）
void ledDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    ledTick();
    delay(5);
  }
}
