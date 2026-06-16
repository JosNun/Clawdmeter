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
    MENU_ACT_REFRESH,    // ask the daemon to poll now
    MENU_ACT_REPAIR,     // clear BLE bonds and re-advertise
    MENU_ACT_SLEEP,      // blank the display until the next interaction
    MENU_ACT_BACK,       // close the menu, no-op
} menu_action_t;

void          ui_menu_open(void);        // show the overlay (no-op if unsupported)
void          ui_menu_close(void);       // hide it, back to the usage view
bool          ui_menu_is_open(void);
void          ui_menu_move(int delta);   // move selection by N detents (wraps)
menu_action_t ui_menu_activate(void);    // action of the highlighted item
