#include "../../hal/input_hal.h"
#include "board.h"
#include <Arduino.h>

void input_hal_init(void) {
    pinMode(BTN_BACK_GPIO, INPUT_PULLUP);
    pinMode(BTN_FWD_GPIO,  INPUT_PULLUP);
}

bool input_hal_is_held(InputButton btn) {
    switch (btn) {
    case INPUT_BTN_PRIMARY:
        return digitalRead(BTN_BACK_GPIO) == LOW;
    case INPUT_BTN_SECONDARY:
        return digitalRead(BTN_FWD_GPIO) == LOW;
    }
    return false;
}

// No rotary encoder on this board.
int  input_hal_encoder_delta(void)   { return 0; }
bool input_hal_encoder_clicked(void) { return false; }
void input_hal_encoder_debug(void)   {}
