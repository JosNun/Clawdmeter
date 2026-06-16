#include "board.h"
#include <Arduino.h>
#include <Wire.h>

// Brings up the shared I2C bus before display_hal_init() probes the SSD1306.
// No IO expander on this board, so nothing else to release.
extern "C" void board_init(void) {
    Wire.begin(IIC_SDA, IIC_SCL);
    Wire.setClock(400000);   // SSD1306 fast-mode I2C — keeps the flush snappy
}
