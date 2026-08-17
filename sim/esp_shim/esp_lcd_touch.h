#pragma once
#include <stdint.h>
#include "esp_err.h"

// Tier-1 shim matching the real component's field layout closely enough for
// main/ui/ui.c to compile (managed_components/espressif__esp_lcd_touch).
// On desktop there is no raw touch controller - bsp_display_get_touch_handle()
// (provided by main/display/display_sdl.c) always returns NULL, so the
// multi-touch pinch-zoom code paths that read this never actually run;
// pointer input comes from LVGL's own SDL mouse indev instead (see
// docs/architecture.md's UI section on how the mouse-cursor indev pattern
// already generalizes to this).
#define SIM_TOUCH_MAX_POINTS 5

typedef struct {
    uint8_t track_id;
    uint16_t x;
    uint16_t y;
    uint16_t strength;
} esp_lcd_touch_point_data_t;

typedef struct {
    uint8_t points;
    esp_lcd_touch_point_data_t coords[SIM_TOUCH_MAX_POINTS];
    uint8_t buttons;
} esp_lcd_touch_data_t;

typedef struct esp_lcd_touch_s {
    esp_lcd_touch_data_t data;
} esp_lcd_touch_t;
typedef esp_lcd_touch_t *esp_lcd_touch_handle_t;

static inline esp_err_t esp_lcd_touch_read_data(esp_lcd_touch_handle_t tp)
{
    (void)tp;
    return ESP_OK;
}
