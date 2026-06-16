#pragma once

// ESP32-WROOM-32 dev board + SSD1306 128x32 monochrome OLED over I2C.
//
// This is a deliberately minimal "display-only" port: no touch, no PMU /
// battery, no IMU, no physical buttons wired (the dev board's BOOT button
// exists but is unused for now). The point is to prove the firmware runs on
// a plain ESP32 (no PSRAM, UART-bridge USB, BLE 4.2) driving a tiny 1-bit
// panel, showing just the quota usage.
//
// The SSD1306 is a 1bpp I2C panel — nothing like the QSPI AMOLEDs the other
// ports use. display.cpp keeps a 512-byte framebuffer and a small vendored
// I2C driver (ssd1306.{h,cpp}); the HAL flush thresholds LVGL's RGB565
// strips down to on/off pixels, so the shared code stays RGB565 throughout.

#define BOARD_NAME           "ESP32-WROOM-32 + SSD1306 128x32"

// ---- Display geometry ----
#define LCD_WIDTH            128
#define LCD_HEIGHT           32

// ---- I2C bus (SSD1306 lives here; shared with nothing else on this board) ----
// ESP32-WROOM-32 Arduino default I2C pins.
#define IIC_SDA              21
#define IIC_SCL              22

// SSD1306 I2C address is auto-probed (0x3C then 0x3D) in ssd1306.cpp, so no
// fixed address macro here.

// ---- Buttons ----
// BOOT (GPIO0) is present on the dev board but unused in this display-only
// port. input.cpp reports nothing held, so no HID key events are generated.
#define BTN_BACK_GPIO        0

// ---- Rotary encoder (EC11-style, common pins to GND, internal pullups) ----
// Five wires: two GNDs (encoder common + one switch leg) and three signals on
// GPIO 23/32/33. Two signals are the A/B quadrature phases, one is the push
// switch. The exact mapping below is a first guess — use the `encdbg` serial
// command to confirm which GPIO is which (rotate → the two A/B pins toggle;
// press → the SW pin reads LOW), then fix these defines. If clockwise turns
// register as counter-clockwise, set ENC_REVERSED to 1 (or swap A/B).
#define BOARD_HAS_ENCODER     1
#define ENC_PIN_A             32
#define ENC_PIN_B             33
#define ENC_PIN_SW            23
#define ENC_REVERSED          0
#define ENC_STEPS_PER_DETENT  4   // EC11 quadrature emits 4 transitions per click

// ---- Capability flags ----
// Everything optional is off — the UI hides the battery indicator, skips the
// secondary-button HID mapping, and the per-board stubs dead-strip cleanly.
#define BOARD_HAS_SECONDARY_BUTTON 0
#define BOARD_HAS_ROTATION         0
#define BOARD_HAS_IMU              0
#define BOARD_HAS_BATTERY          0
#define BOARD_HAS_IO_EXPANDER      0
