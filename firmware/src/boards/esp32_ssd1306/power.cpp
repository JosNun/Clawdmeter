#include "../../hal/power_hal.h"

// No PMU, no battery, no PWR button on this board. All stubs — the UI honors
// caps.has_battery (false) and hides the battery indicator; the hold-to-pair
// gesture in main.cpp simply never arms.
void power_hal_init(void) {}
void power_hal_tick(void) {}

int  power_hal_battery_pct(void)      { return -1; }
bool power_hal_is_charging(void)      { return false; }
bool power_hal_is_vbus_in(void)       { return false; }
bool power_hal_pwr_pressed(void)      { return false; }
bool power_hal_pwr_long_pressed(void) { return false; }
bool power_hal_pwr_released(void)     { return false; }
