#include "../../hal/display_hal.h"
#include "board.h"
#include "ssd1306.h"
#include <Arduino.h>

// Bridges the shared RGB565 LVGL pipeline to a 1-bit SSD1306. LVGL renders
// into a small internal-SRAM strip (main.cpp's BUF_LINES, no PSRAM), the
// flush hands us RGB565 pixels, and we threshold each to on/off. This keeps
// the shared code RGB565 end-to-end — no LV_COLOR_DEPTH change, no #ifdef.

static SSD1306 oled;
static bool    panel_on = true;

// Perceived-luminance threshold. The dark theme (true-black bg, light text)
// thresholds cleanly: text/dim/bar-fill land above, bg/bar-track below.
#define ON_THRESHOLD 96

static inline bool rgb565_is_on(uint16_t c) {
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >> 5) & 0x3F;
    uint8_t b5 = c & 0x1F;
    uint16_t r8 = (r5 * 255) / 31;
    uint16_t g8 = (g6 * 255) / 63;
    uint16_t b8 = (b5 * 255) / 31;
    uint16_t lum = (r8 * 77 + g8 * 150 + b8 * 29) >> 8;  // Rec.601-ish
    return lum >= ON_THRESHOLD;
}

void display_hal_init(void) {
    if (!oled.begin()) {
        Serial.println("SSD1306: not found on I2C (tried 0x3C, 0x3D)");
    } else {
        Serial.printf("SSD1306: found at 0x%02X (%dx%d)\n",
                      oled.address(), SSD1306_W, SSD1306_H);
    }
}

void display_hal_begin(void) {
    oled.clear();
    oled.flush();
    // Brightness/contrast is taken over by idle_init() right after this.
}

void display_hal_set_brightness(uint8_t level) {
    // Map LVGL's 0..255 brightness to SSD1306 contrast, and treat 0 as a
    // hardware blank (used by the idle fade-to-sleep). The fade ramps
    // contrast on the way down, then this kills the panel at the bottom.
    if (level == 0) {
        if (panel_on) { oled.display_on(false); panel_on = false; }
        return;
    }
    if (!panel_on) { oled.display_on(true); panel_on = true; }
    oled.set_contrast(level);
}

void display_hal_fill_screen(uint16_t color) {
    oled.fill(rgb565_is_on(color));
    oled.flush();
}

void display_hal_draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h,
                             const uint16_t* pixels) {
    for (int32_t j = 0; j < h; j++) {
        const uint16_t* row = &pixels[j * w];
        for (int32_t i = 0; i < w; i++) {
            oled.set_pixel((int)(x + i), (int)(y + j), rgb565_is_on(row[i]));
        }
    }
    // The panel is tiny (512 B); a full push per strip is a sub-frame-time
    // I2C burst, so there's no need for dirty-page tracking.
    oled.flush();
}

void display_hal_tick(void) {
    // No rotation on this board — nothing to do.
}

void display_hal_round_area(int32_t* x1, int32_t* y1, int32_t* x2, int32_t* y2) {
    // SSD1306 is fully pixel-addressable via our own framebuffer; no
    // even-alignment requirement, so leave LVGL's invalidate region as-is.
    (void)x1; (void)y1; (void)x2; (void)y2;
}
