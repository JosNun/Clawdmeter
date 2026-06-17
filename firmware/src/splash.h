#pragma once
#include <stdint.h>
#include <lvgl.h>

// Initialize splash module. Creates the canvas widget inside `parent` and
// allocates the 480x480 pixel buffer (PSRAM).
void splash_init(lv_obj_t *parent);

// Advance animation frame if hold time elapsed. Call from main loop.
void splash_tick(void);

// Cycle to the next animation in the catalog.
void splash_next(void);

// Show/hide the splash container.
void splash_show(void);
void splash_hide(void);

// Pick the next animation matching the current usage-rate group.
// Called automatically by splash_show(); also exposed so other modules can
// trigger a re-pick when the rate group changes mid-display.
void splash_pick_for_current_rate(void);

// True when splash is currently rendering (used to gate re-picks).
bool splash_is_active(void);

// Root container (so ui.cpp can attach a click event).
lv_obj_t* splash_get_root(void);

// Mini animated creatures for embedding elsewhere (idle screen, pairing hint…).
// Renders the named claudepix animation (e.g. "idle blink") at ~px×px inside
// `parent`; returns the canvas object (position it with lv_obj_align) or NULL if
// the animation isn't found / allocation fails / the registry is full
// (SPLASH_MINI_MAX). Several may coexist — one splash_mini_tick() call advances
// them all, and each self-skips (resetting to frame 0) while its screen hides.
lv_obj_t* splash_mini_create(lv_obj_t *parent, const char *anim_name, int px);
void splash_mini_tick(void);

// Swap an existing mini creature (by its canvas) to a different animation,
// restarting from frame 0. No-op if the canvas isn't a known mini or it's
// already on that animation. Lets a creature change pose mid-sequence (e.g. the
// usage-view stroller switching to a blink while it pauses).
void splash_mini_set_anim(lv_obj_t *canvas, const char *anim_name);
