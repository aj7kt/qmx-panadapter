#pragma once
// Tier-1 shim for the M5Stack Tab5 BSP header. Real desktop equivalents of
// display bring-up live in main/display/display_sdl.c; this header only
// needs to satisfy the couple of symbols main/ui/ui.c references directly.
static inline void bsp_generate_poweroff_signal(void) { /* no physical power rail to signal on desktop */ }

#include <stdbool.h>
static inline void bsp_set_charge_en(bool en) { (void)en; /* no INA226/charger on desktop */ }
