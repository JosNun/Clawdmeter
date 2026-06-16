#pragma once
#include <stdint.h>

// Physical button abstraction. Boards report up to two screen-independent
// buttons:
//   PRIMARY   — left button on this project's boards (BOOT / GPIO 0).
//               Drives the Claude Code voice-mode PTT (HID Space).
//   SECONDARY — right button on boards that have one (e.g. GPIO 18 on
//               AMOLED-2.16). Drives mode-toggle (HID Shift+Tab). Boards
//               without it report held=false forever and shared code
//               handles that gracefully via BoardCaps.button_count.
//
// The PWR button is owned by power_hal (it's tied to the PMU on some boards
// and to an IO expander on others — see power_hal_pwr_pressed()).

enum InputButton {
    INPUT_BTN_PRIMARY = 0,
    INPUT_BTN_SECONDARY = 1,
};

void input_hal_init(void);

// True while the button is physically held (active-low GPIOs are
// de-bounced at the caller's expense — the existing code polls every
// loop iteration). Boards lacking a button always return false.
bool input_hal_is_held(InputButton btn);

// ---- Optional rotary encoder ----
// Gated at runtime by BoardCaps.has_encoder, so shared code stays #ifdef-free.
// Boards without an encoder link the trivial stubs (return 0 / false / no-op).

// Net detents turned since the last call: positive = clockwise, negative =
// counter-clockwise, 0 if it hasn't moved. Consuming reads (resets the count).
int input_hal_encoder_delta(void);

// True once per debounced press of the encoder's push switch (press edge).
bool input_hal_encoder_clicked(void);

// Print the raw level of each encoder pin to Serial — an identification aid
// for wiring up a new encoder (which GPIO is A/B vs the switch). No-op on
// boards without an encoder.
void input_hal_encoder_debug(void);
