// SDL-port sibling of display.c — same display.h contract, desktop backend.
// Compiled ONLY by the SDL port (sim/); the ESP-IDF build's explicit
// SRCS list in main/CMakeLists.txt never includes *_sdl.c. See docs/porting.md.
//
// The real display.c brings up the
// MIPI-DSI panel + PI4IO expander + ST7121/ST7123 touch through the M5Stack
// BSP; this uses LVGL's own official SDL2 driver (lv_sdl_window_create()),
// which is part of LVGL 9.2.2 itself - the exact version this project pins -
// so no custom LVGL/SDL glue was needed here at all.
#include "display.h"
#include "esp_lcd_touch.h"
#include "lv_sdl_window.h"
#include "lv_sdl_mouse.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static SDL_mutex *s_lock = NULL;
static lv_display_t *s_disp = NULL;

esp_err_t display_init(lv_display_t **out_disp)
{
    s_lock = SDL_CreateMutex();
    if (!s_lock) return ESP_FAIL;

    s_disp = lv_sdl_window_create(DISPLAY_H_RES, DISPLAY_V_RES);
    if (!s_disp) return ESP_FAIL;
    lv_sdl_window_set_title(s_disp, "QMX Panadapter - replica");
    lv_sdl_mouse_create(); // one finger's worth of "touch" via the mouse

    *out_disp = s_disp;
    return ESP_OK;
}

// Real callers (e.g. cat.c's poll task on the real firmware) may call into
// LVGL from a thread other than the one running lv_timer_handler() - this
// mutex is what makes that safe, same role esp_lvgl_port's lock plays.
bool display_lock(uint32_t timeout_ms)
{
    (void)timeout_ms; // SDL_mutex has no timed lock; real firmware's timeout is a safety net we don't need here
    if (!s_lock) return true;
    return SDL_LockMutex(s_lock) == 0;
}

void display_unlock(void)
{
    if (s_lock) SDL_UnlockMutex(s_lock);
}

void display_set_brightness(int percent)
{
    (void)percent; // no backlight to dim on desktop
}

void display_fade_in_backlight(int target_percent)
{
    (void)target_percent;
}

static bool s_flipped = false;
void display_set_flipped(bool flipped) { s_flipped = flipped; }
bool display_is_flipped(void) { return s_flipped; }

// ui.c declares this extern itself rather than pulling it from a header;
// see esp_shim/esp_lcd_touch.h's comment for why NULL is the right answer.
esp_lcd_touch_handle_t bsp_display_get_touch_handle(void) { return NULL; }
