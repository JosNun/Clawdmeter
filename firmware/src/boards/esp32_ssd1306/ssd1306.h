#pragma once
#include <stdint.h>

// Minimal vendored SSD1306 128x32 I2C driver. No external library — same
// philosophy as the AMOLED-1.8 touch reader (avoid pulling a whole GFX/OLED
// stack for ~100 lines of register pokes). Holds a 1bpp framebuffer
// (128*32/8 = 512 B) in internal SRAM; the display HAL writes pixels into it
// and calls flush() to push over I2C.

#define SSD1306_W 128
#define SSD1306_H 32

class SSD1306 {
public:
    // Probe 0x3C then 0x3D; init the panel if found. Returns true on success.
    // Assumes Wire.begin() has already run (board_init does this).
    bool    begin(void);
    bool    found(void) const { return addr_ != 0; }
    uint8_t address(void) const { return addr_; }

    void clear(void);                       // zero framebuffer (does not push)
    void set_pixel(int x, int y, bool on);  // bounds-checked
    void fill(bool on);                     // set whole framebuffer (does not push)
    void flush(void);                       // push framebuffer over I2C

    void set_contrast(uint8_t c);           // 0x81 contrast register
    void display_on(bool on);               // 0xAF / 0xAE

private:
    void cmd(uint8_t c);
    uint8_t addr_ = 0;
    uint8_t fb_[SSD1306_W * SSD1306_H / 8] = {0};
};
