#include "ui.h"
#include "splash.h"
#include <lvgl.h>
#include <time.h>
#include "logo.h"
#include "icons.h"
#include "hal/board_caps.h"

// Custom fonts (scaled for 314 PPI, ~1.9x from original 165 PPI)
LV_FONT_DECLARE(font_tiempos_56);
LV_FONT_DECLARE(font_tiempos_34);
LV_FONT_DECLARE(font_styrene_48);
LV_FONT_DECLARE(font_styrene_28);
LV_FONT_DECLARE(font_styrene_24);
LV_FONT_DECLARE(font_styrene_20);
LV_FONT_DECLARE(font_styrene_16);
LV_FONT_DECLARE(font_styrene_14);
LV_FONT_DECLARE(font_styrene_12);
LV_FONT_DECLARE(font_mono_32);
LV_FONT_DECLARE(font_mono_18);

// Layout values computed from the active board's geometry. Populated once
// in ui_init() and treated as const for the rest of the program. Adding a
// new display size means extending compute_layout() with another
// breakpoint — never editing the screen-builder functions below.
struct Layout {
    bool    tiny;            // very small monochrome panels (e.g. 128x32) use a
                             // dedicated 2-row mini layout, not the card UI below
    const lv_font_t* tiny_font;

    int16_t scr_w, scr_h;
    int16_t margin;
    int16_t title_y;
    int16_t content_y;
    int16_t content_w;

    // Usage screen
    int16_t usage_panel_h;
    int16_t usage_panel_gap;
    int16_t usage_bar_y;
    int16_t usage_reset_y;
    int16_t bar_h;
    int16_t panel_pad_x, panel_pad_y;
    int16_t pill_pad_x, pill_pad_y;
    const lv_font_t* title_font;     // screen title / clock
    const lv_font_t* pct_font;       // big percentage number
    const lv_font_t* ent_pct_font;   // enterprise spending number
    const lv_font_t* pill_font;      // "Current" / "Weekly" pill
    const lv_font_t* reset_font;     // "Resets in ..." line
    const lv_font_t* pace_font;      // enterprise "Under/On/Over pace" line
    const lv_font_t* anim_font;      // animated status line
    int16_t anim_y;                  // status line offset from bottom
    bool    small_icons;             // 40px logo + 24px battery (vs 80/48) on small screens
    int16_t title_nudge;             // title x-shift balancing the corner logo
    int16_t logo_y;                  // logo top edge
    int16_t batt_y;                  // battery icon top edge
    int16_t batt_w;                  // battery icon width, for position math

    // Pairing hint / idle screen
    int16_t pair_y1, pair_y2, pair_y3;
    int16_t idle_px;                 // sleeping-creature size on the idle screen

    // Bluetooth screen
    int16_t bt_info_panel_h;
    int16_t bt_reset_zone_h;
    const lv_font_t* bt_title_font;
    const lv_font_t* bt_status_font;
    const lv_font_t* bt_device_font;
    const lv_font_t* bt_credit_1_font;
    const lv_font_t* bt_credit_2_font;
};
static Layout L = {};

// Pick layout values from the active board's pixel dimensions. The two
// existing boards happen to land on the two breakpoints below; new ports
// inherit the closer one — visually OK, may need a polish pass for
// pixel-perfect alignment but never blocks the port from booting.
static void compute_layout(const BoardCaps& c) {
    L.scr_w = c.width;
    L.scr_h = c.height;
    L.margin = 20;
    L.title_y = 30;

    // Very small monochrome panels (128x32-class) can't hold the card UI at
    // all — the screen builder switches to a 2-row mini layout. Only the few
    // fields below matter on this path; the rest are left at their defaults.
    L.tiny = (c.height < 64);
    if (L.tiny) {
        L.margin = 0;
        L.title_y = 0;
        L.content_y = 0;
        L.content_w = c.width;
        L.tiny_font = &font_styrene_12;
        return;
    }

    // Values shared by the two original breakpoints; the small branch below
    // overrides them wholesale.
    L.bar_h = 24;
    L.panel_pad_x = 16;
    L.panel_pad_y = 12;
    L.pill_pad_x = 18;
    L.pill_pad_y = 6;
    L.title_font   = &font_tiempos_56;
    L.pct_font     = &font_styrene_48;
    L.ent_pct_font = &font_tiempos_56;
    L.pill_font    = &font_styrene_28;
    L.reset_font   = &font_styrene_28;
    L.pace_font    = &font_styrene_16;
    L.anim_font    = &font_mono_32;
    L.anim_y = -15;
    L.small_icons = false;
    L.title_nudge = 16;
    L.logo_y = L.title_y - 10;
    L.batt_y = L.title_y;
    L.batt_w = ICON_BATTERY_W;
    L.pair_y1 = 40;
    L.pair_y2 = 120;
    L.pair_y3 = 160;
    L.idle_px = 160;

    if (c.height >= 460) {
        // Large layout — tuned for 480x480 (AMOLED-2.16).
        L.content_y = 100;
        L.usage_panel_h = 150;
        L.usage_panel_gap = 16;
        L.usage_bar_y = 56;
        L.usage_reset_y = 94;
        L.bt_info_panel_h = 160;
        L.bt_reset_zone_h = 110;
        L.bt_title_font    = &font_tiempos_56;
        L.bt_status_font   = &font_styrene_48;
        L.bt_device_font   = &font_styrene_28;
        L.bt_credit_1_font = &font_styrene_24;
        L.bt_credit_2_font = &font_styrene_20;
    } else if (c.height >= 300) {
        // Compact layout — tuned for 368x448 (AMOLED-1.8).
        L.content_y = 85;
        L.usage_panel_h = 130;
        L.usage_panel_gap = 12;
        L.usage_bar_y = 48;
        L.usage_reset_y = 78;
        L.bt_info_panel_h = 140;
        L.bt_reset_zone_h = 90;
        L.bt_title_font    = &font_tiempos_34;
        L.bt_status_font   = &font_styrene_28;
        L.bt_device_font   = &font_styrene_20;
        L.bt_credit_1_font = &font_styrene_16;
        L.bt_credit_2_font = &font_styrene_14;
    } else {
        // Small layout — tuned for 240x240 (LCD-1.54 and similar square TFTs).
        // Everything shrinks: fonts two steps down, panels ~half height, and
        // the corner logo/battery switch to the 40px/24px small assets.
        L.margin = 8;
        L.title_y = 4;
        L.content_y = 44;
        L.usage_panel_h = 74;
        L.usage_panel_gap = 6;
        L.usage_bar_y = 30;
        L.usage_reset_y = 46;
        L.bar_h = 12;
        L.panel_pad_x = 10;
        L.panel_pad_y = 6;
        L.pill_pad_x = 8;
        L.pill_pad_y = 2;
        L.title_font   = &font_tiempos_34;
        L.pct_font     = &font_styrene_24;
        L.ent_pct_font = &font_tiempos_34;
        L.pill_font    = &font_styrene_14;
        L.reset_font   = &font_styrene_14;
        L.pace_font    = &font_styrene_12;
        L.anim_font    = &font_mono_18;
        // Center the status line in the strip below the weekly panel; flush
        // against the bottom edge it reads as unevenly spaced.
        L.anim_y = -10;
        L.small_icons = true;
        L.title_nudge = 8;
        L.logo_y = 2;
        L.batt_y = 10;
        L.batt_w = ICON_BATTERY_SMALL_W;
        L.pair_y1 = 12;
        L.pair_y2 = 56;
        L.pair_y3 = 80;
        L.idle_px = 96;
        L.bt_info_panel_h = 90;
        L.bt_reset_zone_h = 60;
        L.bt_title_font    = &font_tiempos_34;
        L.bt_status_font   = &font_styrene_20;
        L.bt_device_font   = &font_styrene_14;
        L.bt_credit_1_font = &font_styrene_12;
        L.bt_credit_2_font = &font_styrene_12;
    }

    L.content_w = L.scr_w - 2 * L.margin;
}

// Anthropic brand palette — design tokens live in theme.h
#include "theme.h"
#define COL_BG        THEME_BG
#define COL_PANEL     THEME_PANEL
#define COL_TEXT      THEME_TEXT
#define COL_DIM       THEME_DIM
#define COL_ACCENT    THEME_ACCENT
#define COL_GREEN     THEME_GREEN
#define COL_AMBER     THEME_AMBER
#define COL_RED       THEME_RED
#define COL_BAR_BG    THEME_BAR_BG

// ---- Usage screen widgets (single non-splash view) ----
static lv_obj_t* usage_container;
static lv_obj_t* lbl_title;
// Clock fed by the daemon: base epoch (local wall-clock seconds) + the lv_tick at
// which it landed, so the title ticks forward locally between 60s payloads.
static long     clock_base_epoch = 0;
static uint32_t clock_base_ms = 0;
static int      clock_fmt = 24;   // 12 or 24, set from the daemon payload
static int      clock_last_min = -1;   // last rendered minute; avoids redrawing the title every tick
static lv_obj_t* usage_group;   // the two usage panels — shown when connected
static lv_obj_t* pair_group;    // pairing hint — shown when disconnected
static lv_obj_t* bar_session;
static lv_obj_t* lbl_session_pct;
static lv_obj_t* lbl_session_label;
static lv_obj_t* lbl_session_reset;
static lv_obj_t* bar_weekly;
static lv_obj_t* lbl_weekly_pct;
static lv_obj_t* lbl_weekly_label;
static lv_obj_t* lbl_weekly_reset;
static lv_obj_t* panel_session = nullptr;
static lv_obj_t* panel_weekly = nullptr;
// Enterprise-only widgets inside panel_session
static lv_obj_t* lbl_session_pct_sym = nullptr;  // "%" in smaller font
static lv_obj_t* lbl_spending_desc = nullptr;     // "of your monthly budget"
static lv_obj_t* lbl_spending_status = nullptr;   // "Under pace" / "On pace" / "Over pace"
static lv_obj_t* lbl_anim;      // status line: connection state + whimsical idle

// ---- Battery indicator (shared, on top) ----
static lv_obj_t* battery_img;
static lv_obj_t* logo_img;
static lv_image_dsc_t battery_dscs[5];  // empty, low, medium, full, charging

// ---- Live-data freshness → which usage sub-view to show ----
// usage panels when data is flowing, an idle "Zzz" screen when the host is
// connected but no usage update landed within DATA_FRESH_MS, the pairing hint
// when BLE is down. Re-evaluated every loop in ui_tick_anim().
static lv_obj_t* idle_group;            // the "Zzz" idle screen
static uint32_t  last_data_ms = 0;      // lv_tick when the last valid usage update landed
static bool      data_received = false; // any valid update since boot
static int       view_state = -1;       // -1 unknown / 0 pair / 1 idle / 2 usage
static const uint32_t DATA_FRESH_MS = 90000;  // usage counts as "live" within this window (daemon sends ~60s)

// ---- Shared ----
static lv_image_dsc_t logo_dsc;
static screen_t current_screen = SCREEN_USAGE;
static bool     s_ble_connected = false;   // cached BLE connection state
static uint32_t connected_at_ms = 0;       // when we last entered CONNECTED ("Connected" dwell)

// Animation state
static uint32_t anim_last_ms = 0;
static uint8_t anim_spinner_idx = 0;
static uint8_t anim_phase = 0;
static uint8_t anim_msg_idx = 0;
static uint32_t anim_msg_start = 0;
#define ANIM_MSG_MS     4000

static const char* const spinner_frames[] = {
    "\xC2\xB7", "\xE2\x9C\xBB", "\xE2\x9C\xBD",
    "\xE2\x9C\xB6", "\xE2\x9C\xB3", "\xE2\x9C\xA2",
};
#define SPINNER_COUNT 6
#define SPINNER_PHASES (2 * (SPINNER_COUNT - 1))  // 10: ping-pong 0..5..0

static const uint16_t spinner_ms[SPINNER_COUNT] = {
    260, 130, 130, 130, 130, 260,
};

static const char* const anim_messages[] = {
    "Accomplishing", "Elucidating", "Perusing",
    "Actioning", "Enchanting", "Philosophising",
    "Actualizing", "Envisioning", "Pondering",
    "Baking", "Finagling", "Pontificating",
    "Booping", "Flibbertigibbeting", "Processing",
    "Brewing", "Forging", "Puttering",
    "Calculating", "Forming", "Puzzling",
    "Cerebrating", "Frolicking", "Reticulating",
    "Channelling", "Generating", "Ruminating",
    "Churning", "Germinating", "Scheming",
    "Clauding", "Hatching", "Schlepping",
    "Coalescing", "Herding", "Shimmying",
    "Cogitating", "Honking", "Shucking",
    "Combobulating", "Hustling", "Simmering",
    "Computing", "Ideating", "Smooshing",
    "Concocting", "Imagining", "Spelunking",
    "Conjuring", "Incubating", "Spinning",
    "Considering", "Inferring", "Stewing",
    "Contemplating", "Jiving", "Sussing",
    "Cooking", "Manifesting", "Synthesizing",
    "Crafting", "Marinating", "Thinking",
    "Creating", "Meandering", "Tinkering",
    "Crunching", "Moseying", "Transmuting",
    "Deciphering", "Mulling", "Unfurling",
    "Deliberating", "Mustering", "Unravelling",
    "Determining", "Musing", "Vibing",
    "Discombobulating", "Noodling", "Wandering",
    "Divining", "Percolating", "Whirring",
    "Doing", "Wibbling",
    "Effecting", "Wizarding",
    "Working", "Wrangling",
};
#define ANIM_MSG_COUNT (sizeof(anim_messages) / sizeof(anim_messages[0]))

static lv_color_t pct_color(float pct) {
    if (pct >= 80.0f) return COL_RED;
    if (pct >= 50.0f) return COL_AMBER;
    return COL_GREEN;
}

static void format_reset_time(int mins, char* buf, size_t len) {
    if (mins < 0) {
        snprintf(buf, len, "---");
    } else if (mins < 60) {
        snprintf(buf, len, "Resets in %dm", mins);
    } else if (mins < 1440) {
        snprintf(buf, len, "Resets in %dh %dm", mins / 60, mins % 60);
    } else {
        snprintf(buf, len, "Resets in %dd %dh", mins / 1440, (mins % 1440) / 60);
    }
}

// Compact reset string for the tiny mono layout, where only ~3 chars fit
// ("2h", "3d", "45m", "--"). The full "Resets in 2h 10m" form is used on the
// larger panels (format_reset_time above).
static void format_reset_compact(int mins, char* buf, size_t len) {
    if (mins < 0)          snprintf(buf, len, "--");
    else if (mins < 60)    snprintf(buf, len, "%dm", mins);
    else if (mins < 1440)  snprintf(buf, len, "%dh", mins / 60);
    else                   snprintf(buf, len, "%dd", mins / 1440);
}

// Forward decls — callbacks defined near ui_show_screen below
static void global_click_cb(lv_event_t* e);

static lv_obj_t* make_panel(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_left(panel, L.panel_pad_x, 0);
    lv_obj_set_style_pad_right(panel, L.panel_pad_x, 0);
    lv_obj_set_style_pad_top(panel, L.panel_pad_y, 0);
    lv_obj_set_style_pad_bottom(panel, L.panel_pad_y, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_EVENT_BUBBLE);
    return panel;
}

static lv_obj_t* make_bar(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);
    return bar;
}

static void init_icon_dsc_rgb565a8(lv_image_dsc_t* dsc, int w, int h, const uint8_t* data) {
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc->header.stride = w * 2;
    dsc->data = data;
    dsc->data_size = w * h * 3;
}

static lv_obj_t* make_pill(lv_obj_t* parent, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, L.pill_font, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_set_style_bg_color(lbl, COL_BAR_BG, 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lbl, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_left(lbl, L.pill_pad_x, 0);
    lv_obj_set_style_pad_right(lbl, L.pill_pad_x, 0);
    lv_obj_set_style_pad_top(lbl, L.pill_pad_y, 0);
    lv_obj_set_style_pad_bottom(lbl, L.pill_pad_y, 0);
    return lbl;
}

static void init_battery_icons(void) {
    if (L.small_icons) {
        init_icon_dsc_rgb565a8(&battery_dscs[0], ICON_BATTERY_SMALL_W, ICON_BATTERY_SMALL_H, icon_battery_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[1], ICON_BATTERY_LOW_SMALL_W, ICON_BATTERY_LOW_SMALL_H, icon_battery_low_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[2], ICON_BATTERY_MEDIUM_SMALL_W, ICON_BATTERY_MEDIUM_SMALL_H, icon_battery_medium_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[3], ICON_BATTERY_FULL_SMALL_W, ICON_BATTERY_FULL_SMALL_H, icon_battery_full_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[4], ICON_BATTERY_CHARGING_SMALL_W, ICON_BATTERY_CHARGING_SMALL_H, icon_battery_charging_small_data);
        return;
    }
    init_icon_dsc_rgb565a8(&battery_dscs[0], ICON_BATTERY_W, ICON_BATTERY_H, icon_battery_data);
    init_icon_dsc_rgb565a8(&battery_dscs[1], ICON_BATTERY_LOW_W, ICON_BATTERY_LOW_H, icon_battery_low_data);
    init_icon_dsc_rgb565a8(&battery_dscs[2], ICON_BATTERY_MEDIUM_W, ICON_BATTERY_MEDIUM_H, icon_battery_medium_data);
    init_icon_dsc_rgb565a8(&battery_dscs[3], ICON_BATTERY_FULL_W, ICON_BATTERY_FULL_H, icon_battery_full_data);
    init_icon_dsc_rgb565a8(&battery_dscs[4], ICON_BATTERY_CHARGING_W, ICON_BATTERY_CHARGING_H, icon_battery_charging_data);
}

// ======== Usage Screen ========

static lv_obj_t* make_usage_panel(lv_obj_t* parent, int y, const char* pill_text,
                                  lv_obj_t** out_pct, lv_obj_t** out_pill,
                                  lv_obj_t** out_bar, lv_obj_t** out_reset) {
    lv_obj_t* panel = make_panel(parent, L.margin, y, L.content_w, L.usage_panel_h);

    *out_pct = lv_label_create(panel);
    lv_label_set_text(*out_pct, "---%");
    lv_obj_set_style_text_font(*out_pct, L.pct_font, 0);
    lv_obj_set_style_text_color(*out_pct, COL_TEXT, 0);
    lv_obj_set_pos(*out_pct, 0, 0);

    *out_pill = make_pill(panel, pill_text);
    lv_obj_align(*out_pill, LV_ALIGN_TOP_RIGHT, 0, 1);

    *out_bar = make_bar(panel, 0, L.usage_bar_y,
                        L.content_w - 2 * L.panel_pad_x, L.bar_h);

    *out_reset = lv_label_create(panel);
    lv_label_set_text(*out_reset, "---");
    lv_obj_set_style_text_font(*out_reset, L.reset_font, 0);
    lv_obj_set_style_text_color(*out_reset, COL_DIM, 0);
    lv_obj_set_pos(*out_reset, 0, L.usage_reset_y);

    return panel;
}

// Pairing hint — shown when disconnected so the screen isn't empty and the
// user knows how to (re)pair. Wording matches the 3-second release gesture.
static void build_pair_group(lv_obj_t* parent) {
    pair_group = lv_obj_create(parent);
    lv_obj_set_size(pair_group, L.scr_w, L.scr_h - L.content_y);
    lv_obj_set_pos(pair_group, 0, L.content_y);
    lv_obj_set_style_bg_opa(pair_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pair_group, 0, 0);
    lv_obj_set_style_pad_all(pair_group, 0, 0);
    lv_obj_clear_flag(pair_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pair_group, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t* l1 = lv_label_create(pair_group);
    lv_label_set_text(l1, "To pair");
    lv_obj_set_style_text_font(l1, L.bt_status_font, 0);
    lv_obj_set_style_text_color(l1, COL_TEXT, 0);
    lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, L.pair_y1);

    lv_obj_t* l2 = lv_label_create(pair_group);
    lv_label_set_text(l2, "hold the power button");
    lv_obj_set_style_text_font(l2, L.bt_device_font, 0);
    lv_obj_set_style_text_color(l2, COL_DIM, 0);
    lv_obj_align(l2, LV_ALIGN_TOP_MID, 0, L.pair_y2);

    lv_obj_t* l3 = lv_label_create(pair_group);
    lv_label_set_text(l3, "for 3 seconds, then release");
    lv_obj_set_style_text_font(l3, L.bt_device_font, 0);
    lv_obj_set_style_text_color(l3, COL_DIM, 0);
    lv_obj_align(l3, LV_ALIGN_TOP_MID, 0, L.pair_y3);

    lv_obj_add_flag(pair_group, LV_OBJ_FLAG_HIDDEN);  // ui_update_ble_status decides
}

// Idle "Zzz" screen — shown when the host is connected but no usage update has
// landed recently (token expired, daemon down, host asleep…). Full-screen, like
// the pairing hint, so we never render hours-old numbers as if they were live.
static void build_idle_group(lv_obj_t* parent) {
    idle_group = lv_obj_create(parent);
    lv_obj_set_size(idle_group, L.scr_w, L.scr_h - L.content_y);
    lv_obj_set_pos(idle_group, 0, L.content_y);
    lv_obj_set_style_bg_opa(idle_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(idle_group, 0, 0);
    lv_obj_set_style_pad_all(idle_group, 0, 0);
    lv_obj_clear_flag(idle_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(idle_group, LV_OBJ_FLAG_EVENT_BUBBLE);

    // A shrunk-down sleeping creature (reused claudepix "expression sleep" art)
    // sits between the header and the status line; the animated "Listening…"
    // status line carries the words, so no extra text is needed here.
    lv_obj_t* creature = splash_mini_create(idle_group, "expression sleep", L.idle_px);
    if (creature) lv_obj_align(creature, LV_ALIGN_CENTER, 0, -20);

    lv_obj_add_flag(idle_group, LV_OBJ_FLAG_HIDDEN);  // update_view_state decides
}

// ======== Tiny mono layout (128x32-class panels) ========
// Reuses the same widget globals as the full UI (bar_session, lbl_*_pct,
// lbl_*_reset, usage_group/pair_group/idle_group, lbl_anim) so ui_update(),
// ui_tick_anim(), and update_view_state() all work unchanged — only the
// geometry and fonts differ. No logo, battery, status line, or splash here.

static lv_obj_t* tiny_label(lv_obj_t* parent, int x, int y, const char* txt, lv_color_t col) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, L.tiny_font, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

// One usage window per row: "S 45% [====-- ] 2h". The empty bar track is dark
// (reads as off on a 1-bit panel) so it gets a thin outline to stay visible;
// the indicator is solid (ui_update recolors it, all colors read as on).
static void make_tiny_row(lv_obj_t* parent, int y, const char* tag,
                          lv_obj_t** out_pct, lv_obj_t** out_bar, lv_obj_t** out_reset) {
    // styrene_12 reserves 3px of descender space at the bottom of its line
    // box; since our text has no descenders, the ink rides high. Nudge text +
    // bar down a couple px so each row reads as vertically centered.
    const int ty = y + 3;   // text baseline-ish offset within the 16px row
    const int by = y + 6;   // bar offset (kept aligned with the text)
    tiny_label(parent, 1, ty, tag, COL_TEXT);
    *out_pct = tiny_label(parent, 14, ty, "--%", COL_TEXT);

    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, 48, by);
    lv_obj_set_size(bar, 52, 6);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, COL_DIM, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_TEXT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 1, LV_PART_INDICATOR);
    *out_bar = bar;

    *out_reset = tiny_label(parent, 104, ty, "--", COL_DIM);
}

static lv_obj_t* make_tiny_overlay(lv_obj_t* parent, const char* text) {
    lv_obj_t* g = lv_obj_create(parent);
    lv_obj_set_size(g, L.scr_w, L.scr_h);
    lv_obj_set_pos(g, 0, 0);
    lv_obj_set_style_bg_color(g, COL_BG, 0);
    lv_obj_set_style_bg_opa(g, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g, 0, 0);
    lv_obj_set_style_pad_all(g, 0, 0);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* l = tiny_label(g, 0, 0, text, COL_TEXT);
    lv_obj_center(l);
    lv_obj_add_flag(g, LV_OBJ_FLAG_HIDDEN);  // update_view_state decides
    return g;
}

// Usage-view "stroller": a claudepix creature that occasionally hops across the
// usage rows and off the other side. Its canvas is an opaque 20×20 block (white
// creature on true-black), so it cleanly occludes the stats as it passes and
// they restore perfectly once it leaves — no permanent space given up. Parented
// in usage_group (so it only shows on the usage view and rides the view wipe);
// motion + retrigger live near the view-animation helpers below.
static lv_obj_t* walker = nullptr;
static bool      walker_active = false;
static uint32_t  walker_last_ms = 0;
#define WALKER_INTERVAL_MS 180000   // ~3 min of usage view between strolls

// The idle ("connected, data went stale") overlay: a small blinking claudepix
// creature on the left with the status caption beside it. Opaque so it covers
// the usage rows cleanly during the view wipe. The creature animates via
// splash_mini_tick(), already driven from ui_tick_anim() when view_state==idle.
// A 20px (1-cell) creature is the only size that fits the 32px-tall panel.
//
// "idle blink" (not "expression sleep") because the sleeping creature is a
// solid silhouette — none of its frames use the dark eye color — whereas the
// blinking one has black eyes that survive the 1-bit threshold.
static lv_obj_t* build_idle_creature_tiny(lv_obj_t* parent) {
    lv_obj_t* g = lv_obj_create(parent);
    lv_obj_set_size(g, L.scr_w, L.scr_h);
    lv_obj_set_pos(g, 0, 0);
    lv_obj_set_style_bg_color(g, COL_BG, 0);
    lv_obj_set_style_bg_opa(g, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g, 0, 0);
    lv_obj_set_style_pad_all(g, 0, 0);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* creature = splash_mini_create(g, "idle blink", 20);
    if (creature) lv_obj_align(creature, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* cap = tiny_label(g, 0, 0, "No data", COL_TEXT);  // no "…": styrene_12 lacks the glyph
    lv_obj_align(cap, LV_ALIGN_CENTER, 12, 0);  // centered in the space right of the creature

    lv_obj_add_flag(g, LV_OBJ_FLAG_HIDDEN);  // update_view_state decides
    return g;
}

// ---- Encoder settings menu (tiny mono layout) ----
// A full-screen opaque overlay over the usage view. Only two 16px rows fit on a
// 32px panel, so we show a 2-item window: the selected item highlighted by a
// fixed inverted bar in the TOP slot, the next item dim below. Rotating scrolls
// the list through the bar (wrapping), clicking activates the item in the bar.
//
// The highlight bar is pinned at screen y=0; rotating slides the row list so the
// picked row glides into the bar. On a 1-bit panel a row must flip from
// light-on-black (below the bar) to dark-on-white (inside it) as it crosses the
// bar's bottom edge. We get that by keeping TWO synchronized copies of the row
// list: a dark-text copy clipped to the bar region (top 16px), and a dim-text
// copy clipped to everything below. Both ride identical 4-row tracks slid in
// lockstep, so wherever a row sits decides which copy is visible — it inverts
// exactly at the bar edge with no per-pixel color morph.
//
// The window content is [sel-1 .. sel+2] (wrapping). At rest the tracks sit so
// row index 1 (the selection) fills the bar and row 2 shows dim below. A move
// repaints the window for the new selection, offsets the tracks one row in the
// scroll direction, and slides them back to rest. Built only on the tiny
// layout; menu_overlay stays null elsewhere and the public ui_menu_* calls
// early-out, so they're safe no-ops on other boards.
#define MENU_ROWS   4
#define MENU_REST_Y (-16)   // dark-track y at rest: row index 1 lands in the bar
static lv_obj_t* menu_overlay     = nullptr;
static lv_obj_t* menu_track_dark  = nullptr;  // clipped to the bar (top 16px)
static lv_obj_t* menu_track_light = nullptr;  // clipped to the region below
static lv_obj_t* menu_row_dark[MENU_ROWS]  = {nullptr};
static lv_obj_t* menu_row_light[MENU_ROWS] = {nullptr};
static int       menu_sel = 0;
static bool      menu_closing = false;  // true while the close slide-out plays

static const struct {
    const char*   label;
    menu_action_t action;
} MENU_ITEMS[] = {
    {"Refresh now",   MENU_ACT_REFRESH},
    {"Re-pair",       MENU_ACT_REPAIR},
    {"Sleep display", MENU_ACT_SLEEP},
    {"Back",          MENU_ACT_BACK},
};
#define MENU_COUNT ((int)(sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0])))

// Paint the 4-row window [menu_sel-1 .. menu_sel+2] (wrapping) into both copies.
// Colors are fixed at build time (dark copy = bar text, light copy = dim text);
// here we only set the strings.
static void render_menu(void) {
    for (int i = 0; i < MENU_ROWS; i++) {
        int idx = (((menu_sel - 1 + i) % MENU_COUNT) + MENU_COUNT) % MENU_COUNT;
        lv_label_set_text(menu_row_dark[i],  MENU_ITEMS[idx].label);
        lv_label_set_text(menu_row_light[i], MENU_ITEMS[idx].label);
    }
}

// Slide both tracks together. The dark track rides the bar's coordinate space
// (origin y=0); the light track's clip starts 16px lower, so subtracting 16
// keeps a given row pixel-aligned across both copies as it crosses the edge.
static void menu_track_y_cb(void* obj, int32_t v) {
    (void)obj;
    lv_obj_set_y(menu_track_dark,  v);
    lv_obj_set_y(menu_track_light, v - 16);
}

// One 4-row track (128×64) inside a 16px-tall clip window, with rows in `color`.
static lv_obj_t* build_menu_track(lv_obj_t* parent, int clip_y, lv_color_t color,
                                  lv_obj_t** rows_out) {
    lv_obj_t* clip = lv_obj_create(parent);
    lv_obj_set_size(clip, L.scr_w, 16);
    lv_obj_set_pos(clip, 0, clip_y);
    lv_obj_set_style_bg_opa(clip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clip, 0, 0);
    lv_obj_set_style_radius(clip, 0, 0);  // square clip — no rounded-corner cropping
    lv_obj_set_style_pad_all(clip, 0, 0);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* track = lv_obj_create(clip);
    lv_obj_set_size(track, L.scr_w, MENU_ROWS * 16);
    lv_obj_set_pos(track, 0, MENU_REST_Y);
    lv_obj_set_style_bg_opa(track, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < MENU_ROWS; i++) {
        lv_obj_t* l = lv_label_create(track);
        lv_obj_set_size(l, L.scr_w, 16);
        lv_obj_set_pos(l, 0, i * 16);
        lv_obj_set_style_text_font(l, L.tiny_font, 0);
        lv_obj_set_style_text_color(l, color, 0);
        lv_obj_set_style_pad_left(l, 3, 0);
        lv_obj_set_style_pad_top(l, 2, 0);
        rows_out[i] = l;
    }
    return track;
}

static void build_menu_overlay(lv_obj_t* parent) {
    menu_overlay = lv_obj_create(parent);
    lv_obj_set_size(menu_overlay, L.scr_w, L.scr_h);
    lv_obj_set_pos(menu_overlay, 0, 0);
    lv_obj_set_style_bg_color(menu_overlay, COL_BG, 0);
    lv_obj_set_style_bg_opa(menu_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(menu_overlay, 0, 0);
    lv_obj_set_style_pad_all(menu_overlay, 0, 0);
    lv_obj_clear_flag(menu_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Fixed inverted highlight bar in the top slot (drawn under the dark copy).
    lv_obj_t* bar = lv_obj_create(menu_overlay);
    lv_obj_set_size(bar, L.scr_w, 16);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, COL_TEXT, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);

    // Light copy (dim, below the bar) first, then dark copy (over the bar).
    menu_track_light = build_menu_track(menu_overlay, 16, COL_DIM, menu_row_light);
    menu_track_dark  = build_menu_track(menu_overlay, 0,  COL_BG,  menu_row_dark);

    lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);  // opened by ui_menu_open()
}

// ---- Brightness HUD (tiny mono layout) ----
// A transient full-screen overlay the encoder flashes while adjusting
// brightness: a "Brightness" caption over a level bar. Each turn re-shows it
// and re-arms a one-shot lv_timer that hides it after BRIGHT_HUD_MS of stillness.
#define BRIGHT_HUD_MS 1200
static lv_obj_t*  bright_hud   = nullptr;
static lv_obj_t*  bright_bar   = nullptr;
static lv_timer_t* bright_timer = nullptr;

static void bright_hud_hide_cb(lv_timer_t* t) {
    (void)t;
    if (bright_hud) lv_obj_add_flag(bright_hud, LV_OBJ_FLAG_HIDDEN);
    bright_timer = nullptr;  // one-shot — LVGL deletes it after this fires
}

static void build_brightness_hud(lv_obj_t* parent) {
    bright_hud = lv_obj_create(parent);
    lv_obj_set_size(bright_hud, L.scr_w, L.scr_h);
    lv_obj_set_pos(bright_hud, 0, 0);
    lv_obj_set_style_bg_color(bright_hud, COL_BG, 0);
    lv_obj_set_style_bg_opa(bright_hud, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bright_hud, 0, 0);
    lv_obj_set_style_pad_all(bright_hud, 0, 0);
    lv_obj_clear_flag(bright_hud, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* cap = tiny_label(bright_hud, 0, 2, "Brightness", COL_TEXT);
    lv_obj_set_width(cap, L.scr_w);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, 0);

    bright_bar = lv_bar_create(bright_hud);
    lv_obj_set_pos(bright_bar, 14, 20);
    lv_obj_set_size(bright_bar, L.scr_w - 28, 8);
    lv_bar_set_range(bright_bar, 0, 255);
    lv_bar_set_value(bright_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bright_bar, COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bright_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bright_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_width(bright_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(bright_bar, COL_DIM, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bright_bar, COL_TEXT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bright_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bright_bar, 1, LV_PART_INDICATOR);

    lv_obj_add_flag(bright_hud, LV_OBJ_FLAG_HIDDEN);  // shown by ui_brightness_hud_show()
}

// ---- Boot greeting (tiny mono layout) ----
// A branded startup splash: a winking creature beside the "Clawdmeter"
// wordmark. Shown opaque over the view at boot, then wiped away by
// ui_boot_greeting_show()'s timer to reveal whatever view-state resolved
// underneath. A second mini-creature instance alongside the idle one — only
// possible since the mini engine became multi-instance.
static lv_obj_t* boot_group = nullptr;
#define BOOT_GREET_MS 1800

static void build_boot_greeting(lv_obj_t* parent) {
    boot_group = lv_obj_create(parent);
    lv_obj_set_size(boot_group, L.scr_w, L.scr_h);
    lv_obj_set_pos(boot_group, 0, 0);
    lv_obj_set_style_bg_color(boot_group, COL_BG, 0);
    lv_obj_set_style_bg_opa(boot_group, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(boot_group, 0, 0);
    lv_obj_set_style_pad_all(boot_group, 0, 0);
    lv_obj_clear_flag(boot_group, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* creature = splash_mini_create(boot_group, "expression wink", 20);
    if (creature) lv_obj_align(creature, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* cap = tiny_label(boot_group, 0, 0, "Clawdmeter", COL_TEXT);
    lv_obj_align(cap, LV_ALIGN_CENTER, 12, 0);  // centered in the space right of the creature

    lv_obj_add_flag(boot_group, LV_OBJ_FLAG_HIDDEN);  // shown by ui_boot_greeting_show()
}

static void init_usage_screen_tiny(lv_obj_t* scr) {
    usage_container = lv_obj_create(scr);
    lv_obj_set_size(usage_container, L.scr_w, L.scr_h);
    lv_obj_set_pos(usage_container, 0, 0);
    lv_obj_set_style_bg_opa(usage_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(usage_container, 0, 0);
    lv_obj_set_style_pad_all(usage_container, 0, 0);
    lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_SCROLLABLE);

    usage_group = lv_obj_create(usage_container);
    lv_obj_set_size(usage_group, L.scr_w, L.scr_h);
    lv_obj_set_pos(usage_group, 0, 0);
    lv_obj_set_style_bg_opa(usage_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(usage_group, 0, 0);
    lv_obj_set_style_pad_all(usage_group, 0, 0);
    lv_obj_clear_flag(usage_group, LV_OBJ_FLAG_SCROLLABLE);

    make_tiny_row(usage_group, 0,  "S", &lbl_session_pct, &bar_session, &lbl_session_reset);
    make_tiny_row(usage_group, 16, "W", &lbl_weekly_pct,  &bar_weekly,  &lbl_weekly_reset);

    // Occasional hop-across creature, on top of the rows, parked off-screen.
    walker = splash_mini_create(usage_group, "dance bounce", 20);
    if (walker) {
        lv_obj_set_pos(walker, L.scr_w, 6);  // off the right edge; y centres 20px in 32
        lv_obj_add_flag(walker, LV_OBJ_FLAG_HIDDEN);
    }

    pair_group = make_tiny_overlay(usage_container, "Waiting for host\xE2\x80\xA6");
    idle_group = build_idle_creature_tiny(usage_container);

    // Settings menu overlay — created last so it sits above the usage rows and
    // the pair/idle overlays when opened.
    build_menu_overlay(usage_container);
    build_brightness_hud(usage_container);
    build_boot_greeting(usage_container);  // created last → on top at boot

    // lbl_anim must exist (ui_tick_anim writes to it) but there's no room for
    // a status line on a 2-row 32px panel — keep it hidden.
    lbl_anim = lv_label_create(usage_container);
    lv_label_set_text(lbl_anim, "");
    lv_obj_add_flag(lbl_anim, LV_OBJ_FLAG_HIDDEN);
}

static void init_usage_screen(lv_obj_t* scr) {
    if (L.tiny) { init_usage_screen_tiny(scr); return; }

    usage_container = lv_obj_create(scr);
    lv_obj_set_size(usage_container, L.scr_w, L.scr_h);
    lv_obj_set_pos(usage_container, 0, 0);
    lv_obj_set_style_bg_opa(usage_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(usage_container, 0, 0);
    lv_obj_set_style_pad_all(usage_container, 0, 0);
    lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(usage_container, global_click_cb, LV_EVENT_CLICKED, NULL);

    lbl_title = lv_label_create(usage_container);
    lv_label_set_text(lbl_title, "Usage");
    lv_obj_set_style_text_font(lbl_title, L.title_font, 0);
    lv_obj_set_style_text_color(lbl_title, COL_TEXT, 0);
    // The nudge balances the corner logo on the left; smaller on small
    // screens where the logo is 40px and the battery icon sits closer.
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, L.title_nudge, L.title_y);

    // Usage panels (shown when connected) live in a transparent full-size group
    // so they can be toggled against the pairing hint as one unit.
    usage_group = lv_obj_create(usage_container);
    lv_obj_set_size(usage_group, L.scr_w, L.scr_h);
    lv_obj_set_pos(usage_group, 0, 0);
    lv_obj_set_style_bg_opa(usage_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(usage_group, 0, 0);
    lv_obj_set_style_pad_all(usage_group, 0, 0);
    lv_obj_clear_flag(usage_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(usage_group, LV_OBJ_FLAG_EVENT_BUBBLE);

    panel_session = make_usage_panel(usage_group, L.content_y, "Current",
                     &lbl_session_pct, &lbl_session_label,
                     &bar_session, &lbl_session_reset);

    // Enterprise-only overlays inside panel_session — hidden until enterprise data arrives
    lbl_session_pct_sym = lv_label_create(panel_session);
    lv_label_set_text(lbl_session_pct_sym, "%");
    lv_obj_set_style_text_font(lbl_session_pct_sym, L.reset_font, 0);
    lv_obj_set_style_text_color(lbl_session_pct_sym, COL_TEXT, 0);
    lv_obj_add_flag(lbl_session_pct_sym, LV_OBJ_FLAG_HIDDEN);

    lbl_spending_desc = lv_label_create(panel_session);
    lv_label_set_text(lbl_spending_desc, "of your monthly budget");
    lv_obj_set_style_text_font(lbl_spending_desc, L.reset_font, 0);
    lv_obj_set_style_text_color(lbl_spending_desc, COL_DIM, 0);
    lv_obj_set_pos(lbl_spending_desc, 0, L.usage_reset_y);
    lv_obj_add_flag(lbl_spending_desc, LV_OBJ_FLAG_HIDDEN);

    lbl_spending_status = lv_label_create(panel_session);
    lv_label_set_text(lbl_spending_status, "");
    lv_obj_set_style_text_font(lbl_spending_status, L.pace_font, 0);
    lv_obj_set_pos(lbl_spending_status, 0, L.usage_reset_y + 20);
    lv_obj_add_flag(lbl_spending_status, LV_OBJ_FLAG_HIDDEN);

    panel_weekly = make_usage_panel(usage_group,
                     L.content_y + L.usage_panel_h + L.usage_panel_gap, "Weekly",
                     &lbl_weekly_pct, &lbl_weekly_label,
                     &bar_weekly, &lbl_weekly_reset);
    // Recolor enabled so enterprise period box can color pace and reset separately
    lv_label_set_recolor(lbl_weekly_reset, true);

    build_pair_group(usage_container);
    build_idle_group(usage_container);

    // Status line — always visible on the usage view. Driven by ui_tick_anim().
    lbl_anim = lv_label_create(usage_container);
    lv_label_set_text(lbl_anim, "");
    lv_obj_set_style_text_font(lbl_anim, L.anim_font, 0);
    lv_obj_set_style_text_color(lbl_anim, COL_ACCENT, 0);
    lv_obj_align(lbl_anim, LV_ALIGN_BOTTOM_MID, 0, L.anim_y);
}

// ======== Public API ========

void ui_init(void) {
    compute_layout(board_caps());

    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    init_usage_screen(scr);

    // The tiny mono layout has no splash, logo, or battery indicator. Skip all
    // of it — leaving logo_img/battery_img null, which the update paths guard.
    if (L.tiny) return;

    if (L.small_icons) init_icon_dsc_rgb565a8(&logo_dsc, LOGO_SMALL_WIDTH, LOGO_SMALL_HEIGHT, logo_small_data);
    else               init_icon_dsc_rgb565a8(&logo_dsc, LOGO_WIDTH, LOGO_HEIGHT, logo_data);
    init_battery_icons();

    splash_init(scr);

    if (splash_get_root()) {
        lv_obj_add_event_cb(splash_get_root(), global_click_cb, LV_EVENT_CLICKED, NULL);
    }

    logo_img = lv_image_create(scr);
    lv_image_set_src(logo_img, &logo_dsc);
    lv_obj_set_pos(logo_img, L.margin, L.logo_y);

    battery_img = lv_image_create(scr);
    lv_image_set_src(battery_img, &battery_dscs[0]);
    lv_obj_set_pos(battery_img, L.scr_w - L.batt_w - L.margin, L.batt_y);
    // Boards without battery telemetry never show the indicator (per the HAL
    // contract; previously every board drew the empty-battery glyph).
    if (!board_caps().has_battery) {
        lv_obj_del(battery_img);
        battery_img = nullptr;
    }
}

void ui_update(const UsageData* data) {
    if (!data->valid) return;
    last_data_ms = lv_tick_get();   // a valid usage update just landed → dot goes green
    data_received = true;

    if (data->clock_epoch > 0) {    // daemon supplied wall-clock time → drive the title clock
        clock_base_epoch = data->clock_epoch;
        clock_base_ms = last_data_ms;
        clock_fmt = data->clock_fmt;
    } else if (clock_base_epoch != 0) {   // clock turned off daemon-side → revert title to "Usage"
        clock_base_epoch = 0;
        clock_last_min = -1;
        lv_label_set_text(lbl_title, "Usage");
    }

    int s_pct = (int)(data->session_pct + 0.5f);

    if (data->enterprise) {
        // Spending box: big number-only label + small "%" symbol + desc + pace
        lv_obj_set_style_text_font(lbl_session_pct, L.ent_pct_font, 0);
        lv_label_set_text(lbl_session_label, "Spending");
        lv_obj_add_flag(lbl_session_reset, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_session_pct_sym, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_spending_desc,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_spending_status,   LV_OBJ_FLAG_HIDDEN);
        if (panel_weekly) lv_obj_clear_flag(panel_weekly, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_text_font(lbl_session_pct, L.pct_font, 0);
        lv_label_set_text(lbl_session_label, "Current");
        lv_obj_clear_flag(lbl_session_reset, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_session_pct_sym, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_spending_desc,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_spending_status, LV_OBJ_FLAG_HIDDEN);
        if (panel_weekly) lv_obj_clear_flag(panel_weekly, LV_OBJ_FLAG_HIDDEN);
    }

    char buf[48];

    // Pace vars used in both enterprise blocks below
    const char* pace_text = "Under pace";
    lv_color_t  pace_color = COL_GREEN;
    const char* pace_hex   = "788c5d";   // matches THEME_GREEN
    if (data->session_pct > (float)data->time_pct + 15.0f) {
        pace_text = "Over pace";  pace_color = COL_RED;   pace_hex = "c0392b";
    } else if (data->session_pct > (float)data->time_pct - 15.0f) {
        pace_text = "On pace";    pace_color = COL_AMBER; pace_hex = "d97757";
    }

    if (data->enterprise) {
        lv_label_set_text_fmt(lbl_session_pct, "%d", s_pct);
        lv_obj_align_to(lbl_session_pct_sym, lbl_session_pct,
                        LV_ALIGN_OUT_RIGHT_TOP, 4, 12);
    } else {
        lv_label_set_text_fmt(lbl_session_pct, "%d%%", s_pct);
        if (L.tiny) format_reset_compact(data->session_reset_mins, buf, sizeof(buf));
        else        format_reset_time(data->session_reset_mins, buf, sizeof(buf));
        lv_label_set_text(lbl_session_reset, buf);
    }

    lv_bar_set_value(bar_session, s_pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_session, pct_color(data->session_pct), LV_PART_INDICATOR);

    if (data->enterprise) {
        // Period box: time % + dynamic pace color + "Resets <date>" label
        lv_label_set_text(lbl_weekly_label, "Period");
        lv_label_set_text_fmt(lbl_weekly_pct, "%d%%", data->time_pct);
        lv_bar_set_value(bar_weekly, data->time_pct, LV_ANIM_ON);
        lv_color_t bar_pace = (data->session_pct <= (float)data->time_pct) ? COL_GREEN :
                              (data->session_pct <= (float)data->time_pct + 15.0f) ? COL_AMBER :
                              COL_RED;
        lv_obj_set_style_bg_color(bar_weekly, bar_pace, LV_PART_INDICATOR);
        snprintf(buf, sizeof(buf), "#%s %s# - #faf9f5 Resets %s#",
                 pace_hex, pace_text, data->reset_date);
        lv_label_set_text(lbl_weekly_reset, buf);
    } else {
        int w_pct = (int)(data->weekly_pct + 0.5f);
        lv_label_set_text_fmt(lbl_weekly_pct, "%d%%", w_pct);
        lv_bar_set_value(bar_weekly, w_pct, LV_ANIM_ON);
        lv_obj_set_style_bg_color(bar_weekly, pct_color(data->weekly_pct), LV_PART_INDICATOR);
        if (L.tiny) format_reset_compact(data->weekly_reset_mins, buf, sizeof(buf));
        else        format_reset_time(data->weekly_reset_mins, buf, sizeof(buf));
        lv_label_set_text(lbl_weekly_reset, buf);
    }
}

// Pick the usage-view sub-screen: pairing hint (BLE down), the idle "Zzz" screen
// (connected but data has gone stale), or the live usage panels. Only re-lays-out
// on an actual change. The animated status line stays visible everywhere — it
// reads "Listening…" on the idle screen, keeping it alive rather than frozen.
// The three view groups (pair / idle / usage) are full-width siblings stacked
// at x=0. update_view_state() swaps them with a horizontal push-wipe: the
// outgoing group slides off one edge while the incoming one slides in from the
// other. Direction follows the state rank (pair=0 → idle=1 → usage=2): a higher
// state enters from the right (the list "advances"), a lower one from the left.
#define VIEW_WIPE_MS 200

static lv_obj_t* view_group_ptr(int v) {
    return v == 0 ? pair_group : (v == 1 ? idle_group : usage_group);
}

static void view_anim_x_cb(void* obj, int32_t x) {
    lv_obj_set_x((lv_obj_t*)obj, (int)x);
}

static void view_slide_out_done(lv_anim_t* a) {
    lv_obj_t* g = (lv_obj_t*)a->var;
    lv_obj_add_flag(g, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(g, 0);  // park at rest for the next transition
}

static void view_slide(lv_obj_t* g, int from, int to, bool hide_when_done) {
    lv_obj_clear_flag(g, LV_OBJ_FLAG_HIDDEN);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, g);
    lv_anim_set_exec_cb(&a, view_anim_x_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, VIEW_WIPE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    if (hide_when_done) lv_anim_set_completed_cb(&a, view_slide_out_done);
    lv_anim_start(&a);  // replaces any in-flight slide on this group
}

// ---- Usage-view stroller motion ----
// Hop in from the right to centre, pause (bouncing in place), then hop off the
// left. Reuses view_anim_x_cb to drive the creature's x.
#define WALKER_HOP_MS    1300
#define WALKER_PAUSE_MS  1200

static void walker_done(lv_anim_t* a) {
    (void)a;
    if (walker) {
        lv_obj_add_flag(walker, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(walker, L.scr_w);  // re-park off the right
    }
    walker_active = false;
}

static void walker_exit(lv_anim_t* a) {
    (void)a;
    if (!walker) return;
    int center = (L.scr_w - 20) / 2;
    lv_anim_t e;
    lv_anim_init(&e);
    lv_anim_set_var(&e, walker);
    lv_anim_set_exec_cb(&e, view_anim_x_cb);
    lv_anim_set_values(&e, center, -20);
    lv_anim_set_duration(&e, WALKER_HOP_MS);
    lv_anim_set_delay(&e, WALKER_PAUSE_MS);   // dwell at centre before leaving
    lv_anim_set_path_cb(&e, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&e, walker_done);
    lv_anim_start(&e);
}

static void walker_start(void) {
    if (!walker || walker_active) return;
    walker_active = true;
    int center = (L.scr_w - 20) / 2;
    lv_obj_set_x(walker, L.scr_w);
    lv_obj_clear_flag(walker, LV_OBJ_FLAG_HIDDEN);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, walker);
    lv_anim_set_exec_cb(&a, view_anim_x_cb);
    lv_anim_set_values(&a, L.scr_w, center);
    lv_anim_set_duration(&a, WALKER_HOP_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, walker_exit);  // chain: enter → pause → exit
    lv_anim_start(&a);
}

static void update_view_state(void) {
    if (!usage_group || !pair_group || !idle_group) return;
    int v;
    if (!s_ble_connected) {
        v = 0;  // pairing hint
    } else if (data_received && (lv_tick_get() - last_data_ms) < DATA_FRESH_MS) {
        v = 2;  // live usage
    } else {
        v = 1;  // idle / Zzz
    }
    if (v == view_state) return;
    int old = view_state;
    view_state = v;

    lv_obj_t* groups[3] = {pair_group, idle_group, usage_group};
    // Cancel any in-flight wipe and park every group at rest first.
    for (int i = 0; i < 3; i++) {
        lv_anim_delete(groups[i], view_anim_x_cb);
        lv_obj_set_x(groups[i], 0);
    }

    lv_obj_t* nw = view_group_ptr(v);

    if (old < 0) {  // first resolve (boot) — snap, no wipe
        for (int i = 0; i < 3; i++)
            (groups[i] == nw) ? lv_obj_clear_flag(groups[i], LV_OBJ_FLAG_HIDDEN)
                              : lv_obj_add_flag(groups[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_t* od  = view_group_ptr(old);
    int       W   = L.scr_w;
    int       dir = (v > old) ? 1 : -1;  // advancing state → incoming from right

    // Hide groups not taking part in this transition.
    for (int i = 0; i < 3; i++)
        if (groups[i] != nw && groups[i] != od)
            lv_obj_add_flag(groups[i], LV_OBJ_FLAG_HIDDEN);

    view_slide(nw,  dir * W, 0,        false);  // incoming
    view_slide(od,  0,      -dir * W,  true);   // outgoing, hides itself at rest
}

void ui_tick_anim(void) {
    if (current_screen != SCREEN_USAGE) return;
    update_view_state();
    splash_mini_tick();   // advance every embedded creature; hidden ones self-skip

    // Occasionally send the stroller across the live usage view.
    if (walker && view_state == 2 && !walker_active &&
        lv_tick_get() - walker_last_ms > WALKER_INTERVAL_MS) {
        walker_last_ms = lv_tick_get();
        walker_start();
    }

    uint32_t now = lv_tick_get();

    // Title clock: once the daemon has sent wall-clock time, replace "Usage" with
    // the live time, advanced locally so it ticks every minute between payloads.
    if (clock_base_epoch > 0) {
        time_t cur = (time_t)(clock_base_epoch + (now - clock_base_ms) / 1000);
        struct tm tmv;
        gmtime_r(&cur, &tmv);   // epoch is already local wall-clock → gmtime keeps it as-is
        if (tmv.tm_min != clock_last_min) {   // only rewrite the title when the minute changes
            clock_last_min = tmv.tm_min;
            char tbuf[12];
            if (clock_fmt == 12) {
                int h12 = tmv.tm_hour % 12;
                if (h12 == 0) h12 = 12;
                snprintf(tbuf, sizeof(tbuf), "%d:%02d %s", h12, tmv.tm_min,
                         tmv.tm_hour < 12 ? "AM" : "PM");
            } else {
                snprintf(tbuf, sizeof(tbuf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
            }
            lv_label_set_text(lbl_title, tbuf);
        }
    }

    if (now - anim_msg_start >= ANIM_MSG_MS) {
        anim_msg_idx = (anim_msg_idx + 1) % ANIM_MSG_COUNT;
        anim_msg_start = now;
    }

    if (now - anim_last_ms < spinner_ms[anim_spinner_idx]) return;
    anim_last_ms = now;
    anim_phase = (anim_phase + 1) % SPINNER_PHASES;
    anim_spinner_idx = (anim_phase < SPINNER_COUNT) ? anim_phase
                                                    : (SPINNER_PHASES - anim_phase);

    // Status text by priority. Whimsical messages only when connected & settled.
    const char* text;
    if (!s_ble_connected) {
        text = "Waiting";              // advertising / waiting for a host connection
    } else if (view_state == 1) {      // idle — alternate so it reads as alive AND data-less
        text = (anim_msg_idx & 1) ? "No data" : "Listening";
    } else if (now - connected_at_ms < 5000) {
        text = "Connected";
    } else {
        text = anim_messages[anim_msg_idx];
    }

    // All states share the whimsical style: "<glyph> <Title-case word>…"
    static char buf[80];
    snprintf(buf, sizeof(buf), "%s %s\xE2\x80\xA6",
             spinner_frames[anim_spinner_idx], text);
    lv_label_set_text(lbl_anim, buf);
}

static screen_t prev_non_splash_screen = SCREEN_USAGE;
static void apply_battery_visibility(void) {
    if (!battery_img) return;
    if (current_screen == SCREEN_SPLASH) lv_obj_add_flag(battery_img, LV_OBJ_FLAG_HIDDEN);
    else                                  lv_obj_clear_flag(battery_img, LV_OBJ_FLAG_HIDDEN);
}

static void global_click_cb(lv_event_t* e) {
    (void)e;
    if (current_screen == SCREEN_SPLASH) ui_show_screen(prev_non_splash_screen);
    else                                  ui_show_screen(SCREEN_SPLASH);
}

void ui_show_screen(screen_t screen) {
    // Tiny mono panels have no splash — the usage view is the only screen, so
    // any request (including the boot SCREEN_SPLASH) resolves to it.
    if (L.tiny) {
        lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_HIDDEN);
        current_screen = SCREEN_USAGE;
        return;
    }

    lv_obj_add_flag(usage_container, LV_OBJ_FLAG_HIDDEN);
    splash_hide();

    switch (screen) {
    case SCREEN_SPLASH:  splash_show(); break;
    case SCREEN_USAGE:   lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_HIDDEN); break;
    default: break;
    }

    if (logo_img) {
        if (screen == SCREEN_SPLASH) lv_obj_add_flag(logo_img, LV_OBJ_FLAG_HIDDEN);
        else                          lv_obj_clear_flag(logo_img, LV_OBJ_FLAG_HIDDEN);
    }

    if (screen != SCREEN_SPLASH) prev_non_splash_screen = screen;
    current_screen = screen;
    apply_battery_visibility();
}

void ui_toggle_splash(void) {
    if (current_screen == SCREEN_SPLASH) ui_show_screen(prev_non_splash_screen);
    else                                  ui_show_screen(SCREEN_SPLASH);
}

screen_t ui_get_current_screen(void) {
    return current_screen;
}

void ui_update_ble_status(ble_state_t state, const char* name, const char* mac) {
    (void)name; (void)mac;
    bool was_connected = s_ble_connected;
    s_ble_connected = (state == BLE_STATE_CONNECTED);

    if (s_ble_connected && !was_connected) connected_at_ms = lv_tick_get();
    // pair / idle / usage — picked from connection + data freshness.
    update_view_state();
}

// ---- Encoder settings menu ----

static void boot_greet_done(lv_anim_t* a) {
    (void)a;
    lv_obj_add_flag(boot_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(boot_group, 0);
}

static void boot_greet_dismiss_cb(lv_timer_t* t) {
    (void)t;
    if (!boot_group) return;
    // Wipe out to the left, revealing whatever view-state resolved underneath.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, boot_group);
    lv_anim_set_exec_cb(&a, view_anim_x_cb);
    lv_anim_set_values(&a, 0, -L.scr_w);
    lv_anim_set_duration(&a, VIEW_WIPE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, boot_greet_done);
    lv_anim_start(&a);
}

void ui_boot_greeting_show(void) {
    if (!boot_group) return;
    lv_obj_set_x(boot_group, 0);
    lv_obj_clear_flag(boot_group, LV_OBJ_FLAG_HIDDEN);
    lv_timer_t* t = lv_timer_create(boot_greet_dismiss_cb, BOOT_GREET_MS, nullptr);
    lv_timer_set_repeat_count(t, 1);  // one-shot: dismiss after the greeting
}

void ui_brightness_hud_show(uint8_t level) {
    if (!bright_hud) return;
    lv_bar_set_value(bright_bar, level, LV_ANIM_OFF);
    lv_obj_clear_flag(bright_hud, LV_OBJ_FLAG_HIDDEN);
    if (bright_timer) {
        lv_timer_reset(bright_timer);  // keep it up while the knob keeps turning
    } else {
        bright_timer = lv_timer_create(bright_hud_hide_cb, BRIGHT_HUD_MS, nullptr);
        lv_timer_set_repeat_count(bright_timer, 1);  // one-shot
    }
}

// The menu is a modal panel: opening slides it in from the right over the
// (static) usage view; closing slides it back out to the right, revealing the
// view underneath. Reuses the view-wipe cb/duration so the motion matches.
static void menu_close_done(lv_anim_t* a) {
    (void)a;
    lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(menu_overlay, 0);  // park at rest for the next open
    menu_closing = false;
}

void ui_menu_open(void) {
    if (!menu_overlay) return;
    if (bright_hud) lv_obj_add_flag(bright_hud, LV_OBJ_FLAG_HIDDEN);  // HUD yields to menu
    menu_closing = false;
    menu_sel = 0;
    render_menu();
    lv_anim_delete(menu_track_dark, menu_track_y_cb);  // reset internal scroll
    menu_track_y_cb(nullptr, MENU_REST_Y);
    lv_anim_delete(menu_overlay, view_anim_x_cb);       // cancel any close slide
    lv_obj_clear_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
    view_slide(menu_overlay, L.scr_w, 0, false);        // slide in from the right
}

void ui_menu_close(void) {
    if (!menu_overlay || menu_closing) return;
    if (lv_obj_has_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN)) return;  // already closed
    menu_closing = true;
    lv_anim_delete(menu_track_dark, menu_track_y_cb);   // stop internal scroll
    lv_anim_delete(menu_overlay, view_anim_x_cb);       // cancel open slide if mid-flight
    // Slide out to the right from wherever it is, then hide via menu_close_done.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, menu_overlay);
    lv_anim_set_exec_cb(&a, view_anim_x_cb);
    lv_anim_set_values(&a, lv_obj_get_x(menu_overlay), L.scr_w);
    lv_anim_set_duration(&a, VIEW_WIPE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, menu_close_done);
    lv_anim_start(&a);
}

bool ui_menu_is_open(void) {
    // "Open" means it's the active modal — a menu mid-close-slide is not, so the
    // encoder loop stops steering it and brightness control resumes immediately.
    return menu_overlay && !menu_closing &&
           !lv_obj_has_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
}

void ui_menu_move(int delta) {
    if (!menu_overlay || delta == 0) return;
    menu_sel = (((menu_sel + delta) % MENU_COUNT) + MENU_COUNT) % MENU_COUNT;
    render_menu();
    // Offset the tracks one row in the scroll direction so the bar still shows
    // the OLD selection, then slide to rest: turning down (delta>0) scrolls the
    // list up so the next item rises into the bar; turning up scrolls it down.
    // A fast multi-detent spin still shows a single one-row slide for feedback.
    int start_y = MENU_REST_Y + (delta > 0 ? 16 : -16);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, menu_track_dark);  // anim identity; cb drives both tracks
    lv_anim_set_exec_cb(&a, menu_track_y_cb);
    lv_anim_set_values(&a, start_y, MENU_REST_Y);
    lv_anim_set_duration(&a, 110);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);  // replaces any in-flight slide on this track
}

menu_action_t ui_menu_activate(void) {
    if (!menu_overlay) return MENU_ACT_NONE;
    return MENU_ITEMS[menu_sel].action;
}

void ui_update_battery(int percent, bool charging) {
    if (!battery_img) return;   // no battery indicator (e.g. tiny mono layout)
    int idx;
    if (charging) {
        idx = 4;
    } else if (percent < 0) {
        idx = 0;
    } else if (percent <= 10) {
        idx = 0;
    } else if (percent <= 35) {
        idx = 1;
    } else if (percent <= 75) {
        idx = 2;
    } else {
        idx = 3;
    }
    lv_image_set_src(battery_img, &battery_dscs[idx]);
    apply_battery_visibility();
}
