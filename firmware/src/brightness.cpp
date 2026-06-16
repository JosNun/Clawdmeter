#include "brightness.h"
#include "idle.h"
#include <Preferences.h>
#include <Arduino.h>

// User-controlled display brightness, persisted to NVS as a single continuous
// level (0..255). idle owns the actual panel brightness, so we route the chosen
// level through idle_set_awake_brightness().
//
// Two ways to change it:
//   • brightness_cycle()  — the PWR button on the AMOLED ports steps through a
//                           few presets (LEVELS) and lands on the next one up.
//   • brightness_adjust() — a rotary encoder nudges the level in fine steps.
// Both write the same NVS key, so the two input styles stay in sync.

static const uint8_t LEVELS[] = {64, 128, 200, 255};
#define LEVELS_COUNT (sizeof(LEVELS) / sizeof(LEVELS[0]))
#define DEFAULT_LEVEL 200   // matches the prior hard-coded DISPLAY_DEFAULT_BRIGHTNESS

#define ADJUST_STEP  16     // per-detent change for brightness_adjust()
#define MIN_LEVEL    16     // floor for the encoder — 0 (off) is the menu's Sleep item, not a turn

static uint8_t cur_level = DEFAULT_LEVEL;

static void save_level(void) {
    Preferences prefs;
    prefs.begin("clawdmeter", false);
    prefs.putUChar("brt_lvl", cur_level);
    prefs.end();
}

void brightness_init(void) {
    Preferences prefs;
    prefs.begin("clawdmeter", true);
    uint8_t saved = prefs.getUChar("brt_lvl", 0);  // 0 ⇒ unset
    prefs.end();

    if (saved != 0) cur_level = saved;
    idle_set_awake_brightness(cur_level);
    Serial.printf("Brightness init: level=%u\n", cur_level);
}

void brightness_cycle(void) {
    // Advance to the first preset brighter than the current level; wrap to the
    // dimmest. Works regardless of whether the encoder left us between presets.
    uint8_t next = LEVELS[0];
    for (unsigned i = 0; i < LEVELS_COUNT; i++) {
        if (LEVELS[i] > cur_level) { next = LEVELS[i]; break; }
    }
    cur_level = next;
    save_level();
    idle_set_awake_brightness(cur_level);
    Serial.printf("Brightness cycled: level=%u\n", cur_level);
}

void brightness_adjust(int steps) {
    int v = (int)cur_level + steps * ADJUST_STEP;
    if (v < MIN_LEVEL) v = MIN_LEVEL;
    if (v > 255)       v = 255;
    if ((uint8_t)v == cur_level) return;
    cur_level = (uint8_t)v;
    save_level();
    idle_set_awake_brightness(cur_level);
    Serial.printf("Brightness adjust: level=%u\n", cur_level);
}

uint8_t brightness_get(void) {
    return cur_level;
}
