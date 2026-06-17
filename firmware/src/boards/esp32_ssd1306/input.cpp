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

static volatile int32_t  enc_accum   = 0;  // raw sub-steps since last delta()
static volatile uint8_t  enc_prev_ab = 0;  // last (A<<1|B)

// Click is latched in an ISR via a debounced press/release state machine, not
// polled. The old polled debouncer needed the loop to sample the switch ≥25 ms
// into a still-held press; the playful tide's repaints open polling gaps that
// swallowed quick taps (a long press still worked). An ISR captures the press
// instantly regardless of render load — but it must reject contact bounce on
// BOTH edges and require a clean release before the next press, or release
// bounce double-fires (menu reopening, phantom item activations).
static volatile bool     enc_click_pending = false;  // one-shot, consumed by clicked()
static volatile bool     enc_sw_down = false;        // debounced logical state
static volatile uint32_t enc_sw_last_ms = 0;         // time of last accepted edge
#define ENC_SW_DEBOUNCE_MS 20

static void IRAM_ATTR enc_isr(void) {
    uint8_t a = (uint8_t)digitalRead(ENC_PIN_A);
    uint8_t b = (uint8_t)digitalRead(ENC_PIN_B);
    uint8_t ab = (uint8_t)((a << 1) | b);
    uint8_t idx = (uint8_t)((enc_prev_ab << 2) | ab);
    enc_accum += ENC_TABLE[idx & 0x0F];
    enc_prev_ab = ab;
}

static void IRAM_ATTR enc_sw_isr(void) {
    uint32_t now = millis();
    if (now - enc_sw_last_ms < ENC_SW_DEBOUNCE_MS) return;  // within a settling window → bounce
    bool low = (digitalRead(ENC_PIN_SW) == LOW);            // active-low: LOW = pressed
    if (low == enc_sw_down) return;                         // no real state change
    enc_sw_last_ms = now;
    enc_sw_down = low;
    if (low) enc_click_pending = true;                      // latch one click per press edge
}

void input_hal_init(void) {
    pinMode(ENC_PIN_A, INPUT_PULLUP);
    pinMode(ENC_PIN_B, INPUT_PULLUP);
    pinMode(ENC_PIN_SW, INPUT_PULLUP);

    enc_prev_ab = (uint8_t)((digitalRead(ENC_PIN_A) << 1) | digitalRead(ENC_PIN_B));
    enc_accum = 0;
    enc_sw_down = (digitalRead(ENC_PIN_SW) == LOW);  // seed the debounced state
    attachInterrupt(digitalPinToInterrupt(ENC_PIN_A), enc_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_PIN_B), enc_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_PIN_SW), enc_sw_isr, CHANGE);  // both edges
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
    // Consume the press latched by enc_sw_isr(). Independent of loop timing, so
    // a quick tap is never missed even while the render loop is busy.
    noInterrupts();
    bool clicked = enc_click_pending;
    enc_click_pending = false;
    interrupts();
    return clicked;
}

void input_hal_encoder_debug(void) {
    Serial.printf("ENC raw: A(gpio%d)=%d  B(gpio%d)=%d  SW(gpio%d)=%d\n",
                  ENC_PIN_A, digitalRead(ENC_PIN_A),
                  ENC_PIN_B, digitalRead(ENC_PIN_B),
                  ENC_PIN_SW, digitalRead(ENC_PIN_SW));
}
