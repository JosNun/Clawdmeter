#include "../../hal/input_hal.h"
#include "board.h"
#include <Arduino.h>

// Display-only port: no HID buttons are wired (the dev board's BOOT button on
// GPIO0 is unused), so input_hal_is_held() always reports nothing held. The
// real input on this board is the rotary encoder + push switch — see below.

// ======== Rotary encoder ========
// EC11-style encoder: two quadrature phases (A/B) and a push switch, all with
// the common legs tied to GND and read through the ESP32's internal pullups.
// We decode A/B with a Gray-code state table in a pin-change ISR (reliable for
// hand speeds without missing steps), accumulating sub-steps; the public
// delta() collapses ENC_STEPS_PER_DETENT sub-steps into one reported detent.

static const int8_t ENC_TABLE[16] = {
    //  prev(2) | cur(2)  ->  -1 / 0 / +1
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0,
};

static volatile int32_t enc_accum   = 0;  // raw sub-steps since last delta()
static volatile uint8_t enc_prev_ab = 0;  // last (A<<1|B)

static void IRAM_ATTR enc_isr(void) {
    uint8_t a = (uint8_t)digitalRead(ENC_PIN_A);
    uint8_t b = (uint8_t)digitalRead(ENC_PIN_B);
    uint8_t ab = (uint8_t)((a << 1) | b);
    uint8_t idx = (uint8_t)((enc_prev_ab << 2) | ab);
    enc_accum += ENC_TABLE[idx & 0x0F];
    enc_prev_ab = ab;
}

void input_hal_init(void) {
    pinMode(ENC_PIN_A, INPUT_PULLUP);
    pinMode(ENC_PIN_B, INPUT_PULLUP);
    pinMode(ENC_PIN_SW, INPUT_PULLUP);

    enc_prev_ab = (uint8_t)((digitalRead(ENC_PIN_A) << 1) | digitalRead(ENC_PIN_B));
    enc_accum = 0;
    attachInterrupt(digitalPinToInterrupt(ENC_PIN_A), enc_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_PIN_B), enc_isr, CHANGE);
}

bool input_hal_is_held(InputButton btn) {
    (void)btn;
    return false;   // no HID buttons on this board
}

int input_hal_encoder_delta(void) {
    // Snapshot + clear the accumulator atomically w.r.t. the ISR.
    noInterrupts();
    int32_t raw = enc_accum;
    int32_t detents = raw / ENC_STEPS_PER_DETENT;
    enc_accum = raw - detents * ENC_STEPS_PER_DETENT;  // keep the remainder
    interrupts();

#if ENC_REVERSED
    detents = -detents;
#endif
    return (int)detents;
}

bool input_hal_encoder_clicked(void) {
    // Debounced press-edge detector. Polled every loop (~5 ms); a 25 ms stable
    // window rejects contact bounce. Active-low (switch leg to GND).
    static bool     stable_pressed = false;
    static bool     last_raw       = false;
    static uint32_t last_change_ms = 0;

    bool raw = (digitalRead(ENC_PIN_SW) == LOW);
    uint32_t now = millis();
    if (raw != last_raw) {
        last_raw = raw;
        last_change_ms = now;
    }
    if ((now - last_change_ms) >= 25 && raw != stable_pressed) {
        stable_pressed = raw;
        if (stable_pressed) return true;   // fire once on the press edge
    }
    return false;
}

void input_hal_encoder_debug(void) {
    Serial.printf("ENC raw: A(gpio%d)=%d  B(gpio%d)=%d  SW(gpio%d)=%d\n",
                  ENC_PIN_A, digitalRead(ENC_PIN_A),
                  ENC_PIN_B, digitalRead(ENC_PIN_B),
                  ENC_PIN_SW, digitalRead(ENC_PIN_SW));
}
