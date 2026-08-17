// SDL-port sibling of settings.c — same settings.h contract, desktop backend.
// Compiled ONLY by the SDL port (sim/); the ESP-IDF build's explicit
// SRCS list in main/CMakeLists.txt never includes *_sdl.c. See docs/porting.md.
//
// The real settings.c is NVS-backed with a debounced flush task; this is a
// single in-memory qmx_settings_t PERSISTED to
// ~/.config/panadapter_sdl/settings.bin. Persistence is a versioned whole-
// struct dump: a header records sizeof(qmx_settings_t), and a file written
// by a build with a different struct layout is discarded (defaults win) —
// the NVS-blob trade-off, chosen so 93 setters need zero per-field
// serialization code that would drift. Saving is debounced: a background
// thread snapshots the struct every 2 s and writes only on change, so the
// setters stay one-liners (the same shape as the firmware's flush task).
#include "settings.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static qmx_settings_t s_s;
static wifi_known_t s_wifi_known[WIFI_KNOWN_MAX];
static int s_wifi_known_n = 0;
static bool s_inited = false;

static void set_defaults(void)
{
    memset(&s_s, 0, sizeof(s_s));
    s_s.db_min = -130.0f;
    s_s.db_max = -30.0f;
    s_s.ema_alpha = 0.3f;
    s_s.iq_enabled = true;
    s_s.ft8_freq_hz = 14074000;
    s_s.cw_pitch_hz = 700;
    snprintf(s_s.my_callsign, sizeof(s_s.my_callsign), "N0CALL");
    snprintf(s_s.my_grid, sizeof(s_s.my_grid), "AA00aa");
    s_s.cw_cal_hz = -60;
    s_s.zoom_factor = 1.0f;
    s_s.brightness_pct = 100;
    snprintf(s_s.cq_msg[0], sizeof(s_s.cq_msg[0]), "CQ %s %s", s_s.my_callsign, s_s.my_grid);
    s_s.wifi_enabled = true;
    s_s.cw_audio_vol = 60;
    s_s.wf_black_db = 9.0f;
    s_s.wf_contrast_db = 45.0f;
    s_s.wf_floor_blend = 100;
    s_s.snap_to_peak = true;
    s_s.rit_pill_show = true;
    s_s.ft8_early_decode = true;
    s_s.spots_en = true;
    s_s.pskreporter_en = true;
    s_s.tx_tone_hz = 1500;
    s_s.charge_limit_pct = 80;
}

// --- persistence ---

typedef struct {
    uint32_t magic, ver, s_size, w_size;
    int32_t wifi_n;
} settings_file_hdr_t;
#define SETTINGS_MAGIC 0x4C445351u // "QSDL"

static char s_path[512];

static void build_path(void)
{
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        snprintf(s_path, sizeof(s_path), "panadapter_sdl_settings.bin");
        return;
    }
    char dir[480];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.config/panadapter_sdl", home);
    mkdir(dir, 0755);
    snprintf(s_path, sizeof(s_path), "%s/settings.bin", dir);
}

static void save_now(void)
{
    FILE *f = fopen(s_path, "wb");
    if (!f) return;
    settings_file_hdr_t h = { SETTINGS_MAGIC, 1, sizeof(qmx_settings_t),
                              sizeof(wifi_known_t), s_wifi_known_n };
    fwrite(&h, sizeof(h), 1, f);
    fwrite(&s_s, sizeof(s_s), 1, f);
    fwrite(s_wifi_known, sizeof(wifi_known_t), WIFI_KNOWN_MAX, f);
    fclose(f);
}

static bool load_file(void)
{
    FILE *f = fopen(s_path, "rb");
    if (!f) return false;
    settings_file_hdr_t h;
    bool ok = fread(&h, sizeof(h), 1, f) == 1 &&
              h.magic == SETTINGS_MAGIC && h.ver == 1 &&
              h.s_size == sizeof(qmx_settings_t) &&
              h.w_size == sizeof(wifi_known_t) &&
              h.wifi_n >= 0 && h.wifi_n <= WIFI_KNOWN_MAX;
    if (ok) ok = fread(&s_s, sizeof(s_s), 1, f) == 1 &&
                 fread(s_wifi_known, sizeof(wifi_known_t), WIFI_KNOWN_MAX, f) == WIFI_KNOWN_MAX;
    fclose(f);
    if (ok) {
        s_wifi_known_n = h.wifi_n;
        fprintf(stderr, "settings: loaded %s\n", s_path);
    } else {
        fprintf(stderr, "settings: %s missing fields or from a different build - "
                        "starting from defaults\n", s_path);
        set_defaults(); // a partial read must not leave a half-loaded struct
    }
    return ok;
}

// Debounced autosave: snapshot-compare every 2 s, write only on change. The
// comparison races the LVGL thread's setters benignly - a torn field is
// re-noticed and re-written on the next pass.
static qmx_settings_t s_saved_s;
static wifi_known_t s_saved_w[WIFI_KNOWN_MAX];
static int s_saved_n;

static int autosave_thread(void *arg)
{
    (void)arg;
    for (;;) {
        SDL_Delay(2000);
        if (memcmp(&s_saved_s, &s_s, sizeof(s_s)) != 0 ||
            memcmp(s_saved_w, s_wifi_known, sizeof(s_saved_w)) != 0 ||
            s_saved_n != s_wifi_known_n) {
            s_saved_s = s_s;
            memcpy(s_saved_w, s_wifi_known, sizeof(s_saved_w));
            s_saved_n = s_wifi_known_n;
            save_now();
        }
    }
    return 0;
}

void settings_init(void)
{
    if (s_inited) return;
    set_defaults();
    build_path();
    load_file();
    s_saved_s = s_s;
    memcpy(s_saved_w, s_wifi_known, sizeof(s_saved_w));
    s_saved_n = s_wifi_known_n;
    SDL_CreateThread(autosave_thread, "settings_autosave", NULL);
    s_inited = true;
}

void settings_load_all(qmx_settings_t *out)
{
    if (!s_inited) settings_init();
    *out = s_s;
}

void settings_set_db_min(float v) { s_s.db_min = v; }
void settings_set_db_max(float v) { s_s.db_max = v; }
void settings_set_ema_alpha(float v) { s_s.ema_alpha = v; }
void settings_set_iq_enabled(bool v) { s_s.iq_enabled = v; }
void settings_set_flat_mode(bool v) { s_s.flat_mode = v; }
void settings_set_wifi_ssid(const char *ssid) { snprintf(s_s.wifi_ssid, sizeof(s_s.wifi_ssid), "%s", ssid ? ssid : ""); }
void settings_set_wifi_pass(const char *pass) { snprintf(s_s.wifi_pass, sizeof(s_s.wifi_pass), "%s", pass ? pass : ""); }
void settings_set_last_vfo(uint32_t hz) { s_s.last_vfo_hz = hz; }
void settings_set_ft8_freq_hz(uint32_t hz) { s_s.ft8_freq_hz = hz; }
void settings_set_cw_pitch_hz(uint16_t hz) { s_s.cw_pitch_hz = hz; }
void settings_set_colormap_idx(uint8_t idx) { s_s.colormap_idx = idx; }
void settings_set_my_callsign(const char *call) { snprintf(s_s.my_callsign, sizeof(s_s.my_callsign), "%s", call ? call : ""); }
void settings_set_my_grid(const char *grid) { snprintf(s_s.my_grid, sizeof(s_s.my_grid), "%s", grid ? grid : ""); }

void settings_set_cq_msg(uint8_t idx, const char *text)
{
    if (idx >= 3) return;
    snprintf(s_s.cq_msg[idx], sizeof(s_s.cq_msg[idx]), "%s", text ? text : "");
}
void settings_set_cq_sel(uint8_t idx) { s_s.cq_sel = idx; }
void settings_set_cq_max_calls(uint8_t n) { s_s.cq_max_calls = n; }
void settings_set_hound_mode(uint8_t m) { s_s.hound_mode = m; }
void settings_set_cq_listen_every(uint8_t n) { s_s.cq_listen_every = n; }
void settings_set_onboarded(bool v) { s_s.onboarded = v; }
void settings_set_ft8_filters(const ft8_filters_t *f) { if (f) s_s.ft8_filters = *f; }
void settings_set_cw_audio_en(bool v) { s_s.cw_audio_en = v; }
void settings_set_cw_audio_vol(uint8_t v) { s_s.cw_audio_vol = v; }
void settings_set_wf_black_db(float db) { s_s.wf_black_db = db; }
void settings_set_wf_contrast_db(float db) { s_s.wf_contrast_db = db; }
void settings_set_wf_floor_blend(uint8_t pct) { s_s.wf_floor_blend = pct; }
void settings_set_wf_window(uint8_t idx) { s_s.wf_window = idx; }
void settings_set_display_flip(bool v) { s_s.display_flip = v; }
void settings_set_qmx_vol_db(uint8_t db) { s_s.qmx_vol_db = db; }
void settings_set_cw_tx_offset_hz(int16_t hz) { s_s.cw_tx_offset_hz = hz; }
int16_t settings_get_cw_tx_offset_hz(void) { return s_s.cw_tx_offset_hz; }
void settings_set_psk_rx_en(bool v) { s_s.psk_rx_en = v; }
void settings_set_bt_mouse_en(bool v) { s_s.bt_mouse_en = v; }
void settings_set_cluster_en(bool v) { s_s.cluster_en = v; }
void settings_set_spots_mode_filter(bool v) { s_s.spots_mode_filter = v; }
void settings_set_swr_limit_x10(uint8_t v) { s_s.swr_limit_x10 = v; }
uint8_t settings_get_swr_limit_x10(void) { return s_s.swr_limit_x10; }
void settings_set_distance_in_miles(bool v) { s_s.distance_in_miles = v; }
void settings_set_rit_pill_show(bool v) { s_s.rit_pill_show = v; }
void settings_set_spur_mode(uint8_t v) { s_s.spur_mode = v; }
void settings_set_ft8_early_decode(bool v) { s_s.ft8_early_decode = v; }
void settings_set_greylist_en(bool v) { s_s.greylist_en = v; }
void settings_set_pskreporter_en(bool v) { s_s.pskreporter_en = v; }
void settings_set_spots_en(bool v) { s_s.spots_en = v; }
void settings_set_rbn_en(bool v) { s_s.rbn_en = v; }
void settings_set_sota_en(bool v) { s_s.sota_en = v; }

int settings_wifi_known_count(void) { return s_wifi_known_n; }

int settings_wifi_known_get(wifi_known_t *out, int max)
{
    int n = s_wifi_known_n < max ? s_wifi_known_n : max;
    for (int i = 0; i < n; i++) out[i] = s_wifi_known[i];
    return n;
}

void settings_wifi_known_remember(const char *ssid, const char *pass)
{
    if (!ssid || !ssid[0]) return;
    for (int i = 0; i < s_wifi_known_n; i++) {
        if (strcmp(s_wifi_known[i].ssid, ssid) == 0) {
            wifi_known_t tmp = s_wifi_known[i];
            snprintf(tmp.pass, sizeof(tmp.pass), "%s", pass ? pass : "");
            memmove(&s_wifi_known[1], &s_wifi_known[0], (size_t)i * sizeof(wifi_known_t));
            s_wifi_known[0] = tmp;
            return;
        }
    }
    int n = s_wifi_known_n < WIFI_KNOWN_MAX ? s_wifi_known_n + 1 : WIFI_KNOWN_MAX;
    memmove(&s_wifi_known[1], &s_wifi_known[0], (size_t)(n - 1) * sizeof(wifi_known_t));
    snprintf(s_wifi_known[0].ssid, sizeof(s_wifi_known[0].ssid), "%s", ssid);
    snprintf(s_wifi_known[0].pass, sizeof(s_wifi_known[0].pass), "%s", pass ? pass : "");
    s_wifi_known_n = n;
}

void settings_wifi_known_set_all(const wifi_known_t *list, int n)
{
    if (n > WIFI_KNOWN_MAX) n = WIFI_KNOWN_MAX;
    for (int i = 0; i < n; i++) s_wifi_known[i] = list[i];
    s_wifi_known_n = n;
}

void settings_wifi_known_forget(const char *ssid)
{
    if (!ssid) return;
    for (int i = 0; i < s_wifi_known_n; i++) {
        if (strcmp(s_wifi_known[i].ssid, ssid) == 0) {
            memmove(&s_wifi_known[i], &s_wifi_known[i + 1], (size_t)(s_wifi_known_n - i - 1) * sizeof(wifi_known_t));
            s_wifi_known_n--;
            return;
        }
    }
}

void settings_wifi_known_clear(void) { s_wifi_known_n = 0; }

void settings_set_tx_tone_hz(uint16_t v) { s_s.tx_tone_hz = v; }
void settings_set_tx_tone_hold(bool v) { s_s.tx_tone_hold = v; }
void settings_set_bandplan_region(uint8_t v) { s_s.bandplan_region = v; }
void settings_set_field_day_en(bool v) { s_s.field_day_en = v; }

void settings_set_activation(uint8_t type, const char *ref)
{
    if (!ref || !ref[0]) type = 0;
    s_s.act_type = type;
    snprintf(s_s.act_ref, sizeof(s_s.act_ref), "%s", ref ? ref : "");
}
uint8_t settings_get_activation_type(void) { return s_s.act_type; }
bool settings_get_activation_ref(char *out, size_t out_sz)
{
    if (s_s.act_type == 0) { if (out && out_sz) out[0] = '\0'; return false; }
    snprintf(out, out_sz, "%s", s_s.act_ref);
    return true;
}
const char *settings_activation_sig_name(void)
{
    if (s_s.act_type == 1) return "POTA";
    if (s_s.act_type == 2) return "SOTA";
    return NULL;
}

void settings_set_fd_class(const char *cls) { snprintf(s_s.fd_class, sizeof(s_s.fd_class), "%s", cls ? cls : ""); }
void settings_set_fd_section(const char *section) { snprintf(s_s.fd_section, sizeof(s_s.fd_section), "%s", section ? section : ""); }
void settings_set_sim_mode_en(bool v) { s_s.sim_mode_en = v; }
void settings_set_ft8_op_mode(uint8_t v) { s_s.ft8_op_mode = v; }
void settings_set_passband_width_hz(uint32_t hz) { s_s.passband_width_hz = hz; }
void settings_set_wifi_enabled(bool v) { s_s.wifi_enabled = v; }
void settings_set_qmx_gps(bool v) { s_s.qmx_gps = v; }
void settings_set_freq_kp_calc(bool v) { s_s.freq_kp_calc = v; }
void settings_set_freq_kp_pos(int16_t dx, int16_t dy) { s_s.freq_kp_dx = dx; s_s.freq_kp_dy = dy; }
void settings_set_freq_kp_small(bool v) { s_s.freq_kp_small = v; }
void settings_set_qrz_api_key(const char *key) { snprintf(s_s.qrz_api_key, sizeof(s_s.qrz_api_key), "%s", key ? key : ""); }
void settings_set_qrz_uploaded_n(uint32_t n) { s_s.qrz_uploaded_n = n; }
void settings_set_eqsl_user(const char *user) { snprintf(s_s.eqsl_user, sizeof(s_s.eqsl_user), "%s", user ? user : ""); }
void settings_set_eqsl_pswd(const char *pswd) { snprintf(s_s.eqsl_pswd, sizeof(s_s.eqsl_pswd), "%s", pswd ? pswd : ""); }
void settings_set_eqsl_uploaded_n(uint32_t n) { s_s.eqsl_uploaded_n = n; }
void settings_set_lotw_dxcc(const char *dxcc) { snprintf(s_s.lotw_dxcc, sizeof(s_s.lotw_dxcc), "%s", dxcc ? dxcc : ""); }
void settings_set_lotw_cqz(const char *cqz) { snprintf(s_s.lotw_cqz, sizeof(s_s.lotw_cqz), "%s", cqz ? cqz : ""); }
void settings_set_lotw_ituz(const char *ituz) { snprintf(s_s.lotw_ituz, sizeof(s_s.lotw_ituz), "%s", ituz ? ituz : ""); }
void settings_set_lotw_state(const char *state) { snprintf(s_s.lotw_state, sizeof(s_s.lotw_state), "%s", state ? state : ""); }
void settings_set_lotw_county(const char *county) { snprintf(s_s.lotw_county, sizeof(s_s.lotw_county), "%s", county ? county : ""); }
void settings_set_lotw_uploaded_n(uint32_t n) { s_s.lotw_uploaded_n = n; }
void settings_set_cw_cal_hz(int16_t hz) { s_s.cw_cal_hz = hz; }
void settings_set_zoom_factor(float v) { s_s.zoom_factor = v; }
void settings_set_brightness_pct(uint8_t pct) { s_s.brightness_pct = pct; }
void settings_set_last_ui_mode(uint8_t mode) { s_s.last_ui_mode = mode; }
void settings_set_last_unix_time(uint32_t unix_sec) { s_s.last_unix_time = unix_sec; }
void settings_set_charge_limit_en(bool v) { s_s.charge_limit_en = v; }
void settings_set_charge_limit_pct(uint8_t pct) { s_s.charge_limit_pct = pct; }
void settings_set_display_sleep_min(uint8_t minutes) { s_s.display_sleep_min = minutes; }
void settings_set_resmon_en(bool v) { s_s.resmon_en = v; }
void settings_set_resmon_pos(int16_t dx, int16_t dy) { s_s.resmon_dx = dx; s_s.resmon_dy = dy; }

void settings_flush(void) { save_now(); } // callers wanting it NOW get it now; the autosave covers the rest
