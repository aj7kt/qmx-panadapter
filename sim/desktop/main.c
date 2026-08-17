// SDL port entry point - a stand-in for main/main.c's app_main().
// Calls the REAL ui_init() (main/ui/ui.c, unmodified) against a display
// created by LVGL's own SDL2 backend, with everything ui_init() depends on
// either really implemented (the main/**/*_sdl.c port siblings - see
// docs/porting.md) or honestly stubbed (see stubs/*.c and README.md's tiers).
#include "display.h"
#include "ui.h"
#include "settings.h"
#include "mem_channels.h"
#include "audio.h"
#include "cat.h"
#include "dsp.h"
#include "ft8_screen.h"
#include "ft8_status.h"
#include "ft8_tx.h"
#include "ft8_qso.h"
#include "ft8_pileup.h"
#include "render.h"
#include "adif/adif_log.h"
#include "sim_devices.h"
#include "lvgl.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *wav_path = NULL;
    const char *cat_port = NULL;
    bool start_ft8 = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cat") == 0 && i + 1 < argc) cat_port = argv[++i];
        else if (strcmp(argv[i], "--ft8") == 0) start_ft8 = true;
        else if (!wav_path) wav_path = argv[i];
    }

    settings_init();
    mem_channels_init();
    adif_log_init();

    lv_init();

    lv_display_t *disp = NULL;
    if (display_init(&disp) != ESP_OK) {
        fprintf(stderr, "display_init failed\n");
        return 1;
    }

    native_menu_install(); // macOS menu bar (File/View/Settings); no-op elsewhere

    audio_init();
    cat_init();
    dsp_init();

    ui_init(disp);

    // The REAL render task (main/render/render.c): pulls dsp_get_spectrum()
    // at its own cadence, EMA-smooths, and paints the spectrum + waterfall
    // canvases via ui_push_spectrum(). Same order as firmware app_main —
    // after ui_init (the canvases must exist) and dsp_init.
    if (render_init() != ESP_OK) {
        fprintf(stderr, "render_init failed\n");
        return 1;
    }

    ft8_screen_init();
    ft8_status_init();
    ft8_tx_init();
    ft8_qso_init();
    ft8_pileup_init();
    ft8_rx_sdl_start(); // real ft8_lib decode, slot-aligned (RX only)
    ui_apply_saved_mode();
    if (start_ft8) ui_request_base_mode(true); // --ft8: start on the FT8 screen

    if (wav_path) {
        if (!sim_audio_select_wav(wav_path)) {
            fprintf(stderr, "warning: could not load %s\n", wav_path);
        }
    } else {
        fprintf(stderr, "No audio source given - pick one in Settings (Cmd-,) "
                        "or File > Open WAV, or pass a WAV path as the first "
                        "argument.\n");
    }
    if (cat_port && !sim_cat_select_port(cat_port)) {
        fprintf(stderr, "warning: could not open %s for CAT\n", cat_port);
    }

    display_fade_in_backlight(100);

    Uint32 last = SDL_GetTicks();
    for (;;) {
        Uint32 now = SDL_GetTicks();
        lv_tick_inc(now - last ? now - last : 1);
        last = now;

        display_lock(0);
        uint32_t next = lv_timer_handler();
        display_unlock();

        Uint32 delay = next;
        if (delay < 5) delay = 5;
        if (delay > 33) delay = 33;
        SDL_Delay(delay);
    }
    return 0;
}
