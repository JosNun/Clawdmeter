#include "../../hal/touch_hal.h"

// No touch panel on this board. Always report "not pressed" — the shared
// touch indev callback then never generates pointer events.
void touch_hal_init(void) {}

void touch_hal_read(uint16_t* x, uint16_t* y, bool* pressed) {
    *x = 0;
    *y = 0;
    *pressed = false;
}
