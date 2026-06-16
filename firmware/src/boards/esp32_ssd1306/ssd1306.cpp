#include "ssd1306.h"
#include <Arduino.h>
#include <Wire.h>
#include <string.h>

// SSD1306 control bytes: Co=0, D/C# selects command (0x00) vs data (0x40).
static const uint8_t CTRL_CMD  = 0x00;
static const uint8_t CTRL_DATA = 0x40;

void SSD1306::cmd(uint8_t c) {
    Wire.beginTransmission(addr_);
    Wire.write(CTRL_CMD);
    Wire.write(c);
    Wire.endTransmission();
}

bool SSD1306::begin(void) {
    // Probe the two strap-selectable addresses.
    const uint8_t candidates[2] = {0x3C, 0x3D};
    addr_ = 0;
    for (uint8_t a : candidates) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) { addr_ = a; break; }
    }
    if (!addr_) return false;

    // Standard 128x32 init sequence.
    static const uint8_t init_seq[] = {
        0xAE,               // display off
        0xD5, 0x80,         // clock divide / osc freq
        0xA8, 0x1F,         // multiplex ratio = height-1 (31)
        0xD3, 0x00,         // display offset 0
        0x40,               // start line 0
        0x8D, 0x14,         // charge pump on (internal Vcc)
        0x20, 0x00,         // memory addressing mode = horizontal
        0xA1,               // segment remap (col 127 -> SEG0)
        0xC8,               // COM output scan direction remapped
        0xDA, 0x02,         // COM pins config for 128x32
        0x81, 0x8F,         // contrast
        0xD9, 0xF1,         // pre-charge period
        0xDB, 0x40,         // VCOMH deselect
        0xA4,               // resume display from RAM
        0xA6,               // normal (non-inverted)
        0xAF,               // display on
    };
    for (uint8_t b : init_seq) cmd(b);

    clear();
    flush();
    return true;
}

void SSD1306::clear(void) {
    memset(fb_, 0, sizeof(fb_));
}

void SSD1306::fill(bool on) {
    memset(fb_, on ? 0xFF : 0x00, sizeof(fb_));
}

void SSD1306::set_pixel(int x, int y, bool on) {
    if ((unsigned)x >= SSD1306_W || (unsigned)y >= SSD1306_H) return;
    // 1bpp, page-major: 8 vertically-stacked pixels per byte.
    uint8_t* p = &fb_[(y >> 3) * SSD1306_W + x];
    uint8_t  mask = 1 << (y & 7);
    if (on) *p |= mask;
    else    *p &= ~mask;
}

void SSD1306::flush(void) {
    if (!addr_) return;
    // Address the full panel: columns 0..127, pages 0..3.
    cmd(0x21); cmd(0x00); cmd(SSD1306_W - 1);   // column range
    cmd(0x22); cmd(0x00); cmd((SSD1306_H / 8) - 1);  // page range

    // Stream the framebuffer in chunks that fit the Wire TX buffer
    // (control byte + 16 data bytes per transaction is safely small).
    const int total = sizeof(fb_);
    int i = 0;
    while (i < total) {
        Wire.beginTransmission(addr_);
        Wire.write(CTRL_DATA);
        int n = 0;
        while (n < 16 && i < total) { Wire.write(fb_[i++]); n++; }
        Wire.endTransmission();
    }
}

void SSD1306::set_contrast(uint8_t c) {
    if (!addr_) return;
    cmd(0x81);
    cmd(c);
}

void SSD1306::display_on(bool on) {
    if (!addr_) return;
    cmd(on ? 0xAF : 0xAE);
}
