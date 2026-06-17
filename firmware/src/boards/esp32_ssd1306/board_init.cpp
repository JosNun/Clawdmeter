#include "board.h"
#include <Arduino.h>
#include <Wire.h>

// Brings up the shared I2C bus before display_hal_init() probes the SSD1306.
// No IO expander on this board, so nothing else to release.
extern "C" void board_init(void) {
    Wire.begin(IIC_SDA, IIC_SCL);
    // 800 kHz: above the SSD1306's 400 kHz spec but almost universally fine on
    // short desk wiring, and it ~halves the per-frame flush time (the dominant
    // animation cost). Drop back to 400000 if the panel shows tearing/garbage.
    Wire.setClock(800000);
}
