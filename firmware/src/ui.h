#pragma once
#include "data.h"
#include "ble.h"

enum screen_t {
    SCREEN_SPLASH,
    SCREEN_USAGE,
    SCREEN_COUNT,
};

void ui_init(void);
void ui_update(const UsageData* data);
void ui_tick_anim(void);
void ui_show_screen(screen_t screen);
void ui_toggle_splash(void);
screen_t ui_get_current_screen(void);
void ui_update_ble_status(ble_state_t state, const char* name, const char* mac);
void ui_update_battery(int percent, bool charging);

// ---- Encoder-driven settings menu (tiny mono layout) ----
// The action a menu item maps to; the controller (main.cpp) performs the side
// effect so ble/idle/brightness deps stay out of the UI layer.
typedef enum {
    MENU_ACT_NONE = 0,
    MENU_ACT_REFRESH,     // ask the daemon to poll now
    MENU_ACT_BRIGHTNESS,  // enter the brightness-adjust sub-mode (rotate to set)
    MENU_ACT_REPAIR,      // clear BLE bonds and re-advertise
    MENU_ACT_SLEEP,       // blank the display until the next interaction
    MENU_ACT_BACK,        // close the menu, no-op
} menu_action_t;

void          ui_menu_open(void);        // show the overlay (no-op if unsupported)
void          ui_menu_close(void);       // hide it, back to the usage view
bool          ui_menu_is_open(void);
void          ui_menu_move(int delta);   // move selection by N detents (wraps)
menu_action_t ui_menu_activate(void);    // action of the highlighted item

// Brightness HUD: show the current level (0..255) over the usage view while the
// menu's brightness-adjust sub-mode is active. It's sticky — call
// ui_brightness_hud_hide() to dismiss it (on click/timeout). No-op on layouts
// without the HUD.
void          ui_brightness_hud_show(uint8_t level);
void          ui_brightness_hud_hide(void);

// Branded boot greeting: a creature + wordmark shown briefly at startup, then
// it wipes away to reveal the usage view. Call once from setup(). No-op on
// layouts without it (e.g. AMOLED boards, which have their own splash).
void          ui_boot_greeting_show(void);

// ---- Display mode (tiny mono layout) ----
// The usage view has two faces: MODE_INFO is the straightforward 2-row bars;
// MODE_PLAYFUL is "Claude's day" — quota as a rising tide with a claudepix
// creature floating on the surface (calm low, frantic near the cap). Switched
// via the menu's "View" item. No-op on layouts without the playful face.
typedef enum {
    MODE_INFO = 0,
    MODE_PLAYFUL,
    MODE_COUNT,
} display_mode_t;

void           ui_mode_switch(int delta);  // switch scene; sign sets the wipe direction
display_mode_t ui_mode_get(void);

// Notify the UI that a refresh was requested (e.g. the menu's "Refresh now"),
// so the usage-view stroller can hop in and blink while it's in flight. The
// matching completion is driven by ui_update() when the data lands. No-op where
// the stroller isn't built.
void          ui_refresh_requested(void);
