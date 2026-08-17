// Tier-3 stubs: the FT8 protocol engine (ft8_qso.c, ft8_tx.c, ft8_test.c,
// ft8_status.c, ft8_robot.c, ft8_hound.c, ft8_greylist.c, ft8_pileup.c,
// ft8_msg_guard.c) is not reimplemented for this pass - the FT8 screen
// renders (it's real UI code), but nothing decodes or transmits yet. See
// sim/README.md's roadmap. Every function here answers "idle / not
// running / nothing to report", which is what the real engine would also
// say before its first RX slot completes - an honest starting state, not a
// placeholder pretending to be something else.
#include "ft8_qso.h"
#include "ft8_tx.h"
#include "ft8_test.h"
#include "ft8_status.h"
#include "ft8_robot.h"
#include "ft8_hound.h"
#include "ft8_greylist.h"
#include "ft8_pileup.h"
#include "ft8_msg_guard.h"
#include <string.h>
#include <stdio.h>

// ---- ft8_status.h ----
void ft8_status_init(void) { }
void ft8_status_set(const char *fmt, ...) { (void)fmt; }
void ft8_status_get(char *buf, size_t len) { if (buf && len) buf[0] = '\0'; }

// ---- ft8_qso.h ----
void ft8_qso_init(void) { }
bool ft8_qso_start_cq(const ft8_tx_request_t *cq_req, char *err, size_t err_len)
{ (void)cq_req; snprintf(err, err_len, "FT8 engine not implemented in this sim pass"); return false; }
bool ft8_qso_start(const ft8_tx_request_t *tx1_req, char *err, size_t err_len)
{ (void)tx1_req; snprintf(err, err_len, "FT8 engine not implemented in this sim pass"); return false; }
void ft8_qso_advance(int64_t slot_sec) { (void)slot_sec; }
void ft8_qso_on_tx_complete(void) { }
void ft8_qso_abort(void) { }
void ft8_qso_mark_robot_started(void) { }
bool ft8_qso_override_next(ft8_tx_kind_t kind, char *err, size_t err_len)
{ (void)kind; snprintf(err, err_len, "no active exchange"); return false; }
ft8_qso_state_t ft8_qso_get_state(void) { return FT8_QSO_IDLE; }
bool ft8_qso_is_hound_active(void) { return false; }
void ft8_qso_get_target(char *buf, size_t len) { if (buf && len) buf[0] = '\0'; }
void ft8_qso_get_pinned_call(char *buf, size_t len) { if (buf && len) buf[0] = '\0'; }
int ft8_qso_get_cq_calls_sent(void) { return -1; }
bool ft8_qso_is_busy(char *target_buf, size_t len) { if (target_buf && len) target_buf[0] = '\0'; return false; }
void ft8_qso_get_cur_extra(char *buf, size_t len) { if (buf && len) buf[0] = '\0'; }
bool ft8_qso_cq_filter_active(void) { return false; }
bool ft8_qso_get_priority_freq(int *freq_hz_out) { (void)freq_hz_out; return false; }
int ft8_qso_get_tx_tone_hz(void) { return 0; }
bool ft8_qso_set_tx_tone_hz(int hz, char *err, size_t err_len)
{ (void)hz; snprintf(err, err_len, "nothing running"); return false; }
void ft8_qso_fmt_report(int snr_db, char *out, size_t len)
{ if (snr_db > 15) snr_db = 15; if (snr_db < -24) snr_db = -24; snprintf(out, len, "%+03d", snr_db); }
bool ft8_qso_build_manual_reply(const ft8_call_t *heard, int reply_freq_hz,
                                ft8_tx_request_t *out, bool *is_fresh_grid,
                                char *err, size_t err_len)
{ (void)heard; (void)reply_freq_hz; (void)out; if (is_fresh_grid) *is_fresh_grid = false;
  snprintf(err, err_len, "FT8 engine not implemented in this sim pass"); return false; }
void ft8_qso_notify_manual_final(const char *target_call) { (void)target_call; }
void ft8_qso_note_manual_target(const char *target_call) { (void)target_call; }
bool ft8_qso_get_working_target(char *buf, size_t len) { if (buf && len) buf[0] = '\0'; return false; }
// TRUE is the inert answer here, not false: rebuild_list() drops any row this
// rejects, so returning false blanked the ENTIRE decode list (real decodes
// reached the data layer and never the screen). With no filters configured
// the real implementation matches everything - this is that empty-filter case.
bool ft8_filter_match(const char *text, const ft8_filters_t *f) { (void)text; (void)f; return true; }

// ---- ft8_tx.h ----
void ft8_tx_init(void) { }
bool ft8_tx_build_request(ft8_tx_kind_t kind, const char *target_call, int target_audio_freq_hz,
                          int64_t target_last_utc, const char *extra, ft8_tx_request_t *out_req,
                          char *out_err, size_t out_err_len)
{ (void)kind; (void)target_call; (void)target_audio_freq_hz; (void)target_last_utc; (void)extra; (void)out_req;
  snprintf(out_err, out_err_len, "FT8 TX not implemented in this sim pass"); return false; }
bool ft8_tx_build_request_text(const char *message_text, int audio_freq_hz, ft8_tx_request_t *out_req,
                               char *out_err, size_t out_err_len)
{ (void)message_text; (void)audio_freq_hz; (void)out_req;
  snprintf(out_err, out_err_len, "FT8 TX not implemented in this sim pass"); return false; }
bool ft8_tx_build_request_fd(ft8_tx_kind_t kind, const char *target_call, int target_audio_freq_hz,
                             int64_t target_last_utc, const char *class_section, ft8_tx_request_t *out_req,
                             char *out_err, size_t out_err_len)
{ (void)kind; (void)target_call; (void)target_audio_freq_hz; (void)target_last_utc; (void)class_section; (void)out_req;
  snprintf(out_err, out_err_len, "FT8 TX not implemented in this sim pass"); return false; }
bool ft8_tx_arm(const ft8_tx_request_t *req, char *out_err, size_t out_err_len)
{ (void)req; snprintf(out_err, out_err_len, "FT8 TX not implemented in this sim pass"); return false; }
void ft8_tx_disarm(void) { }
void ft8_tx_request_abort(void) { }
int ft8_tx_last_abort_ms(void) { return -1; }
ft8_tx_state_t ft8_tx_get_status(char *text, size_t text_len, int *secs_until)
{ if (text && text_len) text[0] = '\0'; if (secs_until) *secs_until = 0; return FT8_TX_IDLE; }
bool ft8_tx_get_parity_lock(bool *want_even) { (void)want_even; return false; }
int ft8_tx_get_tone_hz(void) { return 0; }
int ft8_tx_seconds_until_slot(bool match_parity, bool want_even, ftx_protocol_t proto)
{ (void)match_parity; (void)want_even; (void)proto; return 0; }
float ft8_tx_get_last_power_swr(float *power_w, float *swr) { *power_w = -1.0f; *swr = -1.0f; return -1.0f; }
bool ft8_tx_swr_tripped(float *swr_out) { if (swr_out) *swr_out = 0.0f; return false; }
void ft8_tx_clear_swr_trip(void) { }
int ft8_find_clear_tone_hz(void) { return FT8_TX_CQ_DEFAULT_FREQ_HZ; }
uint64_t ft8_tx_get_tone_occupancy(int *n_slots_out, int *n_stations_out)
{ if (n_slots_out) *n_slots_out = 52; if (n_stations_out) *n_stations_out = 0; return 0; }
void ft8_tx_get_tone_occupancy_split(uint64_t *even_out, uint64_t *odd_out, int *n_slots_out, int *n_stations_out)
{ if (even_out) *even_out = 0; if (odd_out) *odd_out = 0; if (n_slots_out) *n_slots_out = 52; if (n_stations_out) *n_stations_out = 0; }
static int s_tone_pref_hz = FT8_TX_CQ_DEFAULT_FREQ_HZ;
static bool s_tone_hold = false;
int ft8_tx_get_tone_pref_hz(void) { return s_tone_pref_hz; }
void ft8_tx_set_tone_pref_hz(int hz) { s_tone_pref_hz = hz; }
bool ft8_tx_get_tone_hold(void) { return s_tone_hold; }
void ft8_tx_set_tone_hold(bool on) { s_tone_hold = on; }
int ft8_tx_pick_tone_hz(void) { return s_tone_pref_hz; }
int ft8_find_clear_tone_hz_near(int center_hz) { return center_hz; }
bool ft8_tx_is_clashing(void) { return false; }
bool ft8_tx_should_run_this_slot(int64_t slot_start_ms, ft8_tx_request_t *out) { (void)slot_start_ms; (void)out; return false; }
bool ft8_tx_slot_would_run(int64_t slot_start_ms) { (void)slot_start_ms; return false; }
static int s_follow_offset_ms = 0;
void ft8_tx_set_follow_offset_ms(int ms) { s_follow_offset_ms = ms; }
int ft8_tx_get_follow_offset_ms(void) { return s_follow_offset_ms; }
void ft8_tx_run(const ft8_tx_request_t *req) { (void)req; }

// ---- ft8_test.h ----
void ft8_self_test(void) { }
bool ft8_task_is_alive(void) { return false; }
void ft8_arrl_fd_selftest(void) { }
void ft8_arrl_fd_e2e_selftest(void) { }
bool ft8_synth_and_decode(const ftx_message_t *msg, float tone_hz, char *out_text, size_t out_len,
                          int *out_snr_db, int *out_score)
{ (void)msg; (void)tone_hz; if (out_text && out_len) out_text[0] = '\0';
  if (out_snr_db) *out_snr_db = 0; if (out_score) *out_score = 0; return false; }
void ft8_sim_synth_selftest(void) { }
bool ft8_get_last_timing_ms(int *out_ms) { (void)out_ms; return false; }
int64_t ft8_last_rx_utc_for_parity(bool even) { (void)even; return 0; }
bool ft8_get_last_applied_ms(int *out_ms) { (void)out_ms; return false; }
uint32_t ft8_get_timing_seq(void) { return 0; }
static ft8_op_mode_t s_op_mode = FT8_OP_MODE_FT8;
void ft8_op_mode_set(ft8_op_mode_t m) { s_op_mode = m; }
ft8_op_mode_t ft8_op_mode_get(void) { return s_op_mode; }
int ft8_op_mode_slot_ms(void) { return s_op_mode == FT8_OP_MODE_FT4 ? 7500 : 15000; }

// ---- ft8_robot.h ----
void ft8_robot_tick(int64_t slot_sec) { (void)slot_sec; }
void ft8_robot_stand_down(const char *reason) { (void)reason; }
bool ft8_robot_occupancy_ready(void) { return false; }

// ---- ft8_hound.h ----
ft8_hound_mode_t ft8_hound_mode(void) { return FT8_HOUND_OFF; }
void ft8_hound_observe(const ft8_call_t *list, int n, int64_t now) { (void)list; (void)n; (void)now; }
bool ft8_hound_looks_like_fox(const ft8_call_t *c) { (void)c; return false; }
const ft8_call_t *ft8_hound_find_fox(const ft8_call_t *list, int n, int64_t slot_sec)
{ (void)list; (void)n; (void)slot_sec; return NULL; }
void ft8_hound_tick(int64_t slot_sec) { (void)slot_sec; }
int ft8_hound_qsy_tone_for(const char *call) { (void)call; return 0; }
int ft8_hound_pick_tx_tone(void) { return FT8_HOUND_TX_MIN_HZ; }

// ---- ft8_greylist.h ----
void ft8_greylist_note_timeout(const char *call) { (void)call; }
bool ft8_greylist_contains(const char *call) { (void)call; return false; }
void ft8_greylist_clear(const char *call) { (void)call; }
void ft8_greylist_clear_all(void) { }
int ft8_greylist_get_all(char out[][12], int max) { (void)out; (void)max; return 0; }

// ---- ft8_pileup.h ----
void ft8_pileup_init(void) { }
void ft8_pileup_note_caller(const char *call, int16_t snr_db, int16_t freq_hz, int64_t last_seen_utc)
{ (void)call; (void)snr_db; (void)freq_hz; (void)last_seen_utc; }
void ft8_pileup_remove(const char *call) { (void)call; }
int ft8_pileup_get_all(ft8_pileup_entry_t *out, int max) { (void)out; (void)max; return 0; }
int ft8_pileup_count(void) { return 0; }
void ft8_pileup_clear(void) { }

// ---- ft8_msg_guard.h ---- (portable in the real firmware too; trivial to make real)
void ft8_msg_normalize(char *s)
{
    if (!s) return;
    char *w = s, *r = s;
    bool last_space = true;
    while (*r) {
        if (*r == ' ' || *r == '\t') {
            if (!last_space) { *w++ = ' '; last_space = true; }
        } else {
            *w++ = *r; last_space = false;
        }
        r++;
    }
    if (w > s && w[-1] == ' ') w--;
    *w = '\0';
}
bool ft8_msg_roundtrip_ok(const char *typed, const char *decoded, const char *my_call)
{ (void)typed; (void)decoded; (void)my_call; return true; }
