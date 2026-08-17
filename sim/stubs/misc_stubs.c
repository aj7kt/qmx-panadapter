// Tier-3 stubs for hardware/network/storage subsystems not reimplemented in
// this pass. See sim/README.md.
#include "adif/adif_log.h"
#include "wifi/wifi.h"
#include "time_sync/time_sync.h"
#include "battery.h"
#include "bt_hid_mouse.h"
#include "cw_audio.h"
#include "diag_log.h"
#include "dsp/iq_balance.h"
#include "util/psram_task.h"
#include "storage/sd_archive.h"
#include "dsp/spur_map.h"
#include "keyboard/tab5_keyboard.h"
#include "usb_hid_mouse.h"
#include "util/usb_shutdown.h"
#include "net/reader_net.h"
#include "net/webserver_ws.h"
#include "net/spots.h"
#include "screenshot/screenshot.h"
#include "freertos/task.h"
#include <string.h>

// ---- adif/adif_log.h ----
void adif_log_init(void) { }
void adif_log_record(const adif_qso_t *qso) { (void)qso; }
int adif_log_count(void) { return 0; }
const char *adif_log_file_path(void) { return "/tmp/qso.adi"; }
bool adif_log_contains_call(const char *call) { (void)call; return false; }
bool adif_log_contains_call_on_band(const char *call, uint32_t freq_hz) { (void)call; (void)freq_hz; return false; }
void adif_log_clear(void) { }
bool adif_log_delete_record(int idx) { (void)idx; return false; }
int adif_log_count_activation(const char *sig_info) { (void)sig_info; return 0; }
bool adif_log_get_record(int idx, char *out, size_t out_sz) { (void)idx; if (out && out_sz) out[0] = '\0'; return false; }
bool adif_log_extract_field(const char *line, const char *field, char *out, size_t out_sz)
{ (void)line; (void)field; if (out && out_sz) out[0] = '\0'; return false; }
const char *adif_log_band_for_freq(uint32_t hz) { (void)hz; return ""; }

// ---- wifi/wifi.h ----
void panadapter_wifi_start(void) { }
void panadapter_wifi_reconnect(const char *ssid, const char *pass) { (void)ssid; (void)pass; }
void panadapter_wifi_update_credentials(const char *ssid, const char *pass) { (void)ssid; (void)pass; }
void panadapter_wifi_set_enabled(bool enabled) { (void)enabled; }
bool wifi_is_connected(void) { return false; }
bool panadapter_wifi_is_enabled(void) { return false; }
const char *wifi_get_ssid(void) { return ""; }
int wifi_get_rssi_dbm(void) { return 0; }
const char *wifi_get_ip(void) { return ""; }
bool wifi_time_is_valid(void) { return true; } // desktop system clock is already real
void panadapter_wifi_scan_start(void) { }
wifi_scan_state_t panadapter_wifi_scan_state(void) { return WIFI_SCAN_IDLE; }
int panadapter_wifi_scan_get(wifi_scan_ap_t *out, int max) { (void)out; (void)max; return 0; }

// ---- time_sync/time_sync.h ---- (desktop system clock stands in for every real source)
time_sync_source_t time_sync_get_source(void) { return TIME_SOURCE_NONE; }
time_sync_source_t time_sync_get_effective_source(void) { return TIME_SOURCE_NONE; }
bool time_sync_qmx_gps_confirmed(void) { return false; }
int64_t time_sync_get_ft8_offset_ms(void) { return 0; }
void time_sync_init(i2c_master_bus_handle_t bus) { (void)bus; }
void time_sync_notify_sntp(time_t utc) { (void)utc; }
bool time_sync_notify_qmx(int h, int m, int s) { (void)h; (void)m; (void)s; return false; }
void time_sync_set_manual(int year, int mon, int mday, int h, int m, int s)
{ (void)year; (void)mon; (void)mday; (void)h; (void)m; (void)s; }
void time_sync_apply_correction_ms(int delta_ms) { (void)delta_ms; }
int time_sync_apply_correction_ms_quiet(int delta_ms) { (void)delta_ms; return 0; }
void time_sync_mark_ft8(void) { }
void time_sync_mark_qmx(void) { }
void time_sync_push_to_qmx(void) { }

// ---- battery.h ----
esp_err_t battery_init(i2c_master_bus_handle_t bus) { (void)bus; return ESP_OK; }
int battery_get_level(void) { return 100; } // desktop is always "on AC power"
int battery_mv_to_level(int mv) { (void)mv; return 100; }
int battery_get_mv(void) { return 4200; }
bool battery_is_charging(void) { return false; }
bool battery_present(void) { return false; }

// ---- bt_hid_mouse.h ----
void bt_hid_mouse_init(void) { }
bool bt_hid_mouse_started(void) { return false; }

// ---- cw_audio.h ----
static bool s_cw_audio_en = false;
static uint8_t s_cw_audio_vol = 60;
void cw_audio_init(void) { }
void cw_audio_preopen(void) { }
void cw_audio_set_enabled(bool en) { s_cw_audio_en = en; }
bool cw_audio_is_enabled(void) { return s_cw_audio_en; }
void cw_audio_set_volume(uint8_t vol_0_100) { s_cw_audio_vol = vol_0_100; }
uint8_t cw_audio_get_volume(void) { return s_cw_audio_vol; }

// ---- diag_log.h ----
void diag_log_init(void) { }
void diag_log_write_session_header(void) { }
bool diag_log_enabled(void) { return false; }
size_t diag_log_size(void) { return 0; }
size_t diag_log_snapshot(char *dst, size_t cap) { (void)dst; (void)cap; return 0; }
void diag_log_clear(void) { }
uint64_t diag_log_total(void) { return 0; }
const char *diag_log_persist_path_rotated(void) { return ""; }
uint32_t diag_log_usb_enum_failures(void) { return 0; }
size_t diag_log_read_from(uint64_t from, char *dst, size_t cap, uint64_t *out_next)
{ (void)from; (void)dst; (void)cap; if (out_next) *out_next = 0; return 0; }
void diag_log_persist_start(void) { }
const char *diag_log_persist_path(void) { return ""; }

// ---- dsp/iq_balance.h ----
void iq_balance_reset(void) { }
void iq_balance_set_enabled(bool on) { (void)on; }
void iq_balance_init(bool enabled) { (void)enabled; }
bool iq_balance_is_enabled(void) { return false; }
void iq_balance_apply(int16_t *i_inout, int16_t *q_inout) { (void)i_inout; (void)q_inout; }
void iq_balance_get_debug(float *k_phi, float *k_amp, float *p_i, float *p_q, float *xy,
                          uint32_t *upd, uint32_t *frozen)
{ if (k_phi) *k_phi = 0; if (k_amp) *k_amp = 1; if (p_i) *p_i = 0; if (p_q) *p_q = 0;
  if (xy) *xy = 0; if (upd) *upd = 0; if (frozen) *frozen = 0; }

// ---- util/psram_task.h ---- (real: just delegates to the FreeRTOS shim - see esp_shim/freertos/task.h)
TaskHandle_t psram_task_create(TaskFunction_t func, const char *name, uint32_t stack_bytes,
                                void *arg, UBaseType_t priority, BaseType_t core_id)
{
    TaskHandle_t h = NULL;
    xTaskCreatePinnedToCore(func, name, stack_bytes, arg, priority, &h, (int)core_id);
    return h;
}

// ---- storage/sd_archive.h ----
void sd_archive_init(void) { }
bool sd_archive_is_mounted(void) { return false; }
bool sd_archive_wait_mounted(uint32_t timeout_ms) { (void)timeout_ms; return false; }
void sd_archive_mark_adif_dirty(void) { }
void sd_archive_mark_config_dirty(void) { }
void sd_archive_mark_lotw_dirty(void) { }
const char *sd_archive_log_path(void) { return ""; }
bool sd_archive_lock(uint32_t timeout_ms) { (void)timeout_ms; return true; }
void sd_archive_unlock(void) { }
bool sd_archive_get_free_bytes(uint64_t *out_free, uint64_t *out_total) { (void)out_free; (void)out_total; return false; }

// ---- dsp/spur_map.h ----
static spur_mode_t s_spur_mode = SPUR_MODE_OFF;
void spur_map_init(void) { }
void spur_map_apply(float *mag2, int n_bins) { (void)mag2; (void)n_bins; }
spur_mode_t spur_map_get_mode(void) { return s_spur_mode; }
void spur_map_set_mode(spur_mode_t mode) { s_spur_mode = mode; }
bool spur_map_is_enabled(void) { return s_spur_mode != SPUR_MODE_OFF; }
int spur_map_get_marks(uint16_t *bins, int max) { (void)bins; (void)max; return 0; }
bool spur_map_is_measuring(void) { return false; }
void spur_map_forget_all(void) { }

// ---- keyboard/tab5_keyboard.h ----
esp_err_t tab5_keyboard_init(void) { return ESP_ERR_NOT_FOUND; } // no snap-on keyboard on desktop; use SDL's own
void tab5_keyboard_set_text_cb(tab5_kbd_text_cb_t cb, void *arg) { (void)cb; (void)arg; }
bool tab5_keyboard_present(void) { return false; }

// ---- usb_hid_mouse.h ---- (LVGL's own SDL mouse indev is used instead - see desktop/display.c)
void usb_hid_mouse_init(void) { }
bool usb_hid_mouse_present(void) { return false; }
void usb_hid_mouse_get(int *x, int *y, uint8_t *buttons) { if (x) *x = 0; if (y) *y = 0; if (buttons) *buttons = 0; }

// ---- util/usb_shutdown.h ----
bool usb_shutdown_graceful(void) { return false; }
void usb_shutdown_install_handler(void) { }

// ---- net/reader_net.h ----
void reader_net_load_index(void) { }
void reader_net_fetch(const char *page_rel, bool with_toc) { (void)page_rel; (void)with_toc; }
void reader_net_erase_all(void) { }

// ---- net/webserver_ws.h ----
esp_err_t webserver_ws_start(httpd_handle_t server) { (void)server; return ESP_OK; }
void webserver_ws_stop(void) { }
void webserver_ws_set_paused(bool paused) { (void)paused; }

// ---- net/spots.h ----
void spots_init(void) { }
int spots_get(spot_t *out, int max) { (void)out; (void)max; return 0; }
int spots_get_in_range(spot_t *out, int max, uint32_t lo_hz, uint32_t hi_hz) { (void)out; (void)max; (void)lo_hz; (void)hi_hz; return 0; }
int spots_get_in_range_wait(spot_t *out, int max, uint32_t lo_hz, uint32_t hi_hz, int wait_ms)
{ (void)out; (void)max; (void)lo_hz; (void)hi_hz; (void)wait_ms; return 0; }
uint32_t spots_version(void) { return 0; }
void spots_publish(spot_source_t src, const spot_t *list, int n) { (void)src; (void)list; (void)n; }
int spots_age_s(void) { return -1; }
bool spots_activation_for_call(const char *call, uint32_t freq_hz, char *sig_out, size_t sig_sz, char *ref_out, size_t ref_sz)
{ (void)call; (void)freq_hz; if (sig_out && sig_sz) sig_out[0] = '\0'; if (ref_out && ref_sz) ref_out[0] = '\0'; return false; }
bool spots_any_source_enabled(void) { return false; }
void spots_request_refresh(void) { }

// ---- screenshot/screenshot.h ----
esp_err_t screenshot_capture_rgb565(uint8_t **out_buf, size_t *out_size, uint32_t *out_w, uint32_t *out_h)
{ (void)out_buf; (void)out_size; (void)out_w; (void)out_h; return ESP_FAIL; }
