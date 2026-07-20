#include "../../hal/sound_hal.h"

// ESP32-WROOM-32 + SSD1306: a display-only port with no buzzer or speaker wired
// up, so audio is a no-op. The shared session-reset chime engine lives in
// ../../chime.cpp and is only compiled in on boards that set BOARD_HAS_SOUND.

void sound_hal_init(void) {}
void sound_hal_tick(void) {}
void sound_hal_play_reset(void) {}
