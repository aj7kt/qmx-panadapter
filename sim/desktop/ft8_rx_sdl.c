// FT8 RX for the SDL port: a wall-clock-aligned slot loop that captures via
// the real dsp.h contract (dsp_ft8_capture_begin/progress/finish - see
// dsp_sdl.c), decodes with the REAL ft8_lib (monitor + ftx_find_candidates +
// ftx_decode_candidate, the exact pipeline the firmware runs), and injects
// results through the real ft8_screen_record_decode() - so the FT8 screen,
// row sorting, and stale-row aging all behave exactly as on hardware.
//
// This is the port's stand-in for ft8_test.c's slot loop (RX only - TX and
// the QSO machine stay stubbed). The SNR estimate, audio-Hz conversion and
// slot-timing formulas are lifted from ft8_test.c so the numbers in the
// decode list match what the device would show; if those ever change there,
// change them here (they are small and commented at both ends).
#include "sim_devices.h"
#include "dsp.h"
#include "ui_mode.h"
#include "ft8_screen.h"
#include "ft8_hash.h"
#include "common/monitor.h"
#include "ft8/decode.h"
#include "ft8/message.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SR_HZ            12000
#define SLOT_MS          15000
#define NUM_SAMPLES      (SLOT_MS / 1000 * SR_HZ) // 180000
#define MAX_CANDIDATES   140
#define FIND_MIN_SCORE   5
#define LDPC_MAX_ITERS   30
// Same values as ft8_test.c - keeps displayed SNR comparable to the device
// (and to WSJT-X, which that offset was calibrated against side-by-side).
#define SNR_REF_BW_HZ    2500.0f
#define SNR_CAL_OFFSET_DB 15.0f

static SDL_Thread *s_thread;
static monitor_t s_mon;
static float *s_buf;

static int64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

// ft8_test.c's ft8_estimate_noise_db: mean power over the whole waterfall.
static float estimate_noise_db(const monitor_t *mon)
{
    const ftx_waterfall_t *wf = &mon->wf;
    int total = wf->num_blocks * wf->block_stride;
    if (total <= 0) return 0.0f;
    double sum = 0;
    for (int i = 0; i < total; ++i)
        sum += powf(10.0f, WF_ELEM_MAG(wf->mag[i]) / 10.0f);
    return 10.0f * log10f((float)(sum / total));
}

// ft8_test.c's ft8_estimate_snr_db: strongest of the 8 tone bins per symbol,
// averaged over the slot, minus the noise floor, scaled to the 2500 Hz ref.
static float estimate_snr_db(const monitor_t *mon, const ftx_candidate_t *cand, float noise_db)
{
    const ftx_waterfall_t *wf = &mon->wf;
    int total = wf->num_blocks * wf->block_stride;
    if (total <= 0) return 0.0f;
    int base = ((cand->time_sub * wf->freq_osr) + cand->freq_sub) * wf->num_bins + cand->freq_offset;
    double sig_sum = 0;
    int sig_n = 0;
    for (int block = 0; block < wf->num_blocks; ++block) {
        float max_mag = -120.0f;
        for (int tone = 0; tone < 8; ++tone) {
            int idx = base + tone * wf->num_bins + block * wf->block_stride;
            if (idx < 0 || idx >= total) continue;
            float m = WF_ELEM_MAG(wf->mag[idx]);
            if (m > max_mag) max_mag = m;
        }
        sig_sum += powf(10.0f, max_mag / 10.0f);
        sig_n++;
    }
    if (sig_n == 0) return 0.0f;
    float sig_db = 10.0f * log10f((float)(sig_sum / sig_n));
    float bw_correction_db = 10.0f * log10f(SNR_REF_BW_HZ / (6.25f / wf->freq_osr));
    return (sig_db - noise_db) - bw_correction_db + SNR_CAL_OFFSET_DB;
}

static void decode_slot(int64_t slot_sec)
{
    monitor_reset(&s_mon);
    int n = 0;
    while (s_mon.wf.num_blocks < s_mon.wf.max_blocks &&
           (n + 1) * s_mon.block_size <= NUM_SAMPLES) {
        monitor_process(&s_mon, &s_buf[n * s_mon.block_size]);
        n++;
    }

    ftx_candidate_t cands[MAX_CANDIDATES];
    int n_cand = ftx_find_candidates(&s_mon.wf, MAX_CANDIDATES, cands, FIND_MIN_SCORE);
    float noise_db = (n_cand > 0) ? estimate_noise_db(&s_mon) : 0.0f;

    // Dedupe within the slot: several candidates (time/freq oversampling)
    // routinely decode to the same message.
    char seen[60][FTX_MAX_MESSAGE_LENGTH];
    int n_seen = 0, n_decoded = 0;

    for (int i = 0; i < n_cand; i++) {
        ftx_message_t msg;
        ftx_decode_status_t st;
        if (!ftx_decode_candidate(&s_mon.wf, &cands[i], LDPC_MAX_ITERS, &msg, &st)) continue;
        char text[FTX_MAX_MESSAGE_LENGTH];
        ftx_message_offsets_t off;
        if (ftx_message_decode(&msg, ft8_hash_if(), text, &off) != FTX_MESSAGE_RC_OK) continue;

        bool dup = false;
        for (int s = 0; s < n_seen && !dup; s++)
            if (strcmp(seen[s], text) == 0) dup = true;
        if (dup) continue;
        if (n_seen < 60) snprintf(seen[n_seen++], FTX_MAX_MESSAGE_LENGTH, "%s", text);

        // Formulas from ft8_test.c's decode_candidate_range (see there for
        // the reasoning - freq is bins->Hz, dt is boundary-relative ms).
        float dt_ms = (cands[i].time_offset * s_mon.block_size +
                       cands[i].time_sub * s_mon.subblock_size) / 12.0f;
        int snr_db = (int)lroundf(estimate_snr_db(&s_mon, &cands[i], noise_db));
        int freq_hz = (int)lroundf((s_mon.min_bin + cands[i].freq_offset) / s_mon.symbol_period);

        fprintf(stderr, "ft8: decoded '%s' (score=%d freq=%dHz snr=%d dt=%dms)\n",
                text, cands[i].score, freq_hz, snr_db, (int)lroundf(dt_ms));
        ft8_screen_record_decode(text, cands[i].score, snr_db, freq_hz,
                                 slot_sec, (int)lroundf(dt_ms));
        n_decoded++;
    }
    fprintf(stderr, "ft8: slot %lld: %d candidates, %d decoded\n",
            (long long)slot_sec, n_cand, n_decoded);
}

static int rx_thread(void *arg)
{
    (void)arg;
    const monitor_config_t cfg = {
        .f_min = 200.0f,
        .f_max = 3000.0f,
        .sample_rate = SR_HZ,
        .time_osr = 2,
        .freq_osr = 2,
        .protocol = FTX_PROTOCOL_FT8,
    };
    monitor_init(&s_mon, &cfg);
    s_buf = malloc(NUM_SAMPLES * sizeof(float));
    if (!s_buf) { fprintf(stderr, "ft8: capture buffer alloc failed\n"); return 1; }
    ft8_hash_init();

    for (;;) {
        if (ui_mode_get() != UI_MODE_FT8) { SDL_Delay(250); continue; }

        // Wait for (just past) a 15 s boundary, then capture the whole slot.
        int64_t ms = now_ms();
        int64_t into = ms % SLOT_MS;
        if (into > 500) { // not at a boundary - sleep toward the next one
            int64_t left = SLOT_MS - into;
            SDL_Delay((Uint32)(left > 250 ? 250 : left));
            continue;
        }
        int64_t slot_start_ms = ms - into;
        int64_t slot_sec = slot_start_ms / 1000;
        dsp_ft8_capture_begin(s_buf, NUM_SAMPLES, (uint32_t)(into * SR_HZ / 1000));

        bool aborted = false;
        while (dsp_ft8_capture_progress() < NUM_SAMPLES) {
            if (now_ms() >= slot_start_ms + SLOT_MS) break;
            if (ui_mode_get() != UI_MODE_FT8) { aborted = true; break; }
            SDL_Delay(100);
        }
        dsp_ft8_capture_finish(0);
        if (aborted) continue;

        decode_slot(slot_sec);
    }
    return 0;
}

void ft8_rx_sdl_start(void)
{
    if (!s_thread)
        s_thread = SDL_CreateThread(rx_thread, "ft8_rx", NULL);
}
