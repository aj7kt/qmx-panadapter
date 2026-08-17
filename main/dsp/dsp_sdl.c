// SDL-port sibling of dsp.c — same dsp.h contract, portable FFT kernel.
// Compiled ONLY by the SDL port (sim/); the ESP-IDF build's explicit
// SRCS list in main/CMakeLists.txt never includes *_sdl.c. See docs/porting.md.
//
// Real FFT math (kiss_fft instead of
// esp-dsp's assembly kernels - the same "share the math, swap the kernel"
// idea docs/architecture.md's Testing section describes for the harnesses),
// computed synchronously in dsp_get_spectrum() rather than by a separate
// fft_task - render.c already calls it at its own cadence, so a background
// task would just be redundant plumbing here.
//
// Zoom-FFT, FT8 slot capture, and CW audio forwarding are NOT implemented:
// dsp_get_zoom_spectrum() returns NULL and dsp_get_zoom_decim() returns 1,
// which is dsp.h's own documented "zoom-FFT inactive" state - an honest
// answer, not a placeholder. See docs/architecture.md's Tier-3 rationale.
#include "dsp.h"
#include "audio.h"
#include "kiss_fft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Not part of audio.h - see audio_sdl.c's top-of-file comment.
int sim_audio_get_channels(void);

#define N DSP_FFT_SIZE

static kiss_fft_cfg s_cfg = NULL;
static float s_window[N];
static kiss_fft_cpx s_fin[N];
static kiss_fft_cpx s_fout[N];
static float s_last[N];
static bool s_have_last = false;
static int16_t s_samples[N * 2];

static void build_window(uint8_t idx)
{
    for (int n = 0; n < N; n++) {
        switch (idx) {
        case 1: // Hann
            s_window[n] = (float)(0.5 - 0.5 * cos(2.0 * M_PI * n / (N - 1)));
            break;
        case 2: { // Nuttall
            double x = 2.0 * M_PI * n / (N - 1);
            s_window[n] = (float)(0.355768 - 0.487396 * cos(x) + 0.144232 * cos(2 * x) - 0.012604 * cos(3 * x));
            break;
        }
        default: { // Blackman-Harris
            double x = 2.0 * M_PI * n / (N - 1);
            s_window[n] = (float)(0.35875 - 0.48829 * cos(x) + 0.14128 * cos(2 * x) - 0.01168 * cos(3 * x));
        }
        }
    }
}

esp_err_t dsp_init(void)
{
    s_cfg = kiss_fft_alloc(N, 0, NULL, NULL);
    if (!s_cfg) return ESP_FAIL;
    build_window(0);
    return ESP_OK;
}

void dsp_set_window(uint8_t idx) { build_window(idx); }
void dsp_set_transfer_quiet(bool quiet) { (void)quiet; }

static inline float mag_to_db(float re, float im)
{
    return 10.0f * log10f(re * re + im * im + 1e-12f) - DSP_DB_CALIBRATION_OFFSET;
}

esp_err_t dsp_get_spectrum(float *dst)
{
    size_t got = audio_read_samples(s_samples, N, 15);
    while (got < (size_t)N && got > 0) {
        size_t more = audio_read_samples(&s_samples[got * 2], (size_t)N - got, 15);
        if (more == 0) break;
        got += more;
    }
    if (got < (size_t)N) {
        if (s_have_last) { memcpy(dst, s_last, N * sizeof(float)); return ESP_OK; }
        return ESP_ERR_NOT_FOUND;
    }

    bool iq = sim_audio_get_channels() >= 2;
    for (int i = 0; i < N; i++) {
        float w = s_window[i];
        s_fin[i].r = s_samples[i * 2 + 0] * w;
        s_fin[i].i = iq ? s_samples[i * 2 + 1] * w : 0.0f;
    }
    kiss_fft(s_cfg, s_fin, s_fout);
    for (int i = 0; i < N; i++) s_last[i] = mag_to_db(s_fout[i].r, s_fout[i].i);
    if (!s_have_last)
        fprintf(stderr, "dsp: first spectrum computed (%d bins, %s input)\n",
                N, iq ? "I/Q" : "mono");
    s_have_last = true;
    memcpy(dst, s_last, N * sizeof(float));
    return ESP_OK;
}

void dsp_avg_start(uint32_t frames) { (void)frames; }
bool dsp_avg_ready(float *dst) { (void)dst; return false; } // spur_map.c is stubbed; never armed on desktop

esp_err_t dsp_get_peak_dbm_around_vfo(int center_bin, int half_width_bins, float *peak_dbm)
{
    if (!s_have_last) return ESP_ERR_NOT_FOUND;
    float peak = -300.0f;
    for (int d = -half_width_bins; d <= half_width_bins; d++) {
        int idx = ((center_bin + d) % N + N) % N;
        if (s_last[idx] > peak) peak = s_last[idx];
    }
    *peak_dbm = peak;
    return ESP_OK;
}

esp_err_t dsp_find_peak_hz_around(int32_t center_hz, int32_t radius_hz, int32_t if_offset_hz, int32_t *out_hz)
{
    if (!s_have_last) { *out_hz = center_hz; return ESP_ERR_NOT_FOUND; }
    float bin_hz = (float)DSP_SAMPLE_RATE_HZ / N;
    int center_bin = (int)lroundf((center_hz + if_offset_hz) / bin_hz);
    int radius_bins = (int)(radius_hz / bin_hz);
    if (radius_bins < 1) radius_bins = 1;

    float sum = 0; int count = 0, best_bin = center_bin; float best = -300.0f;
    for (int d = -radius_bins; d <= radius_bins; d++) {
        int idx = ((center_bin + d) % N + N) % N;
        sum += s_last[idx]; count++;
        if (s_last[idx] > best) { best = s_last[idx]; best_bin = center_bin + d; }
    }
    float mean = count ? sum / count : -300.0f;
    if (best - mean > 3.0f) {
        *out_hz = center_hz + (int32_t)lroundf((best_bin - center_bin) * bin_hz);
    } else {
        *out_hz = center_hz;
    }
    return ESP_OK;
}

// --- FT8 slot capture: the firmware's mixer/decimator, desktop edition. ---
// The ring holds the QMX-shaped 48 kHz I/Q stream with the signal at the
// +12 kHz IF (audio_sdl.c synthesizes exactly that from a WAV). Mix down by
// -Fs/4 — the rotation sequence (1, -j, -1, +j), integer swaps — and boxcar-4
// decimate to the 12 kHz real audio ft8_lib decodes. Only the real part is
// kept (the decoder wants real audio; a constant mixer-phase offset just
// phase-shifts each tone, which an FFT magnitude never sees). Consuming the
// ring here is safe: in FT8 mode render.c stops calling dsp_get_spectrum(),
// so this is the ring's only reader — same single-consumer rule as firmware.
static float *s_cap_dst = NULL;
static uint32_t s_cap_target = 0;
static uint32_t s_cap_count = 0;
static int s_mix_k = 0;
static float s_acc = 0.0f;
static int s_acc_n = 0;

static void capture_pump(void)
{
    if (!s_cap_dst) return;
    int16_t buf[512 * 2];
    size_t got;
    while (s_cap_count < s_cap_target &&
           (got = audio_read_samples(buf, 512, 0)) > 0) {
        for (size_t i = 0; i < got; i++) {
            float I = buf[i * 2], Q = buf[i * 2 + 1], re;
            switch (s_mix_k & 3) { // Re((I+jQ)·(-j)^k)
                case 0: re = I; break;
                case 1: re = Q; break;
                case 2: re = -I; break;
                default: re = -Q; break;
            }
            s_mix_k++;
            s_acc += re;
            if (++s_acc_n == 4) {
                if (s_cap_count < s_cap_target)
                    s_cap_dst[s_cap_count++] = s_acc * (0.25f / 32768.0f);
                s_acc = 0.0f;
                s_acc_n = 0;
            }
        }
    }
}

esp_err_t dsp_ft8_capture_begin(float *dst, uint32_t target_samples, uint32_t backfill_samples)
{
    if (!dst || target_samples == 0) return ESP_ERR_INVALID_ARG;
    // Nothing drains the ring between slots in FT8 mode - drop the stale
    // audio so dst[0] is (approximately) the slot boundary, and zero-fill
    // the boundary->arm gap the way the firmware's pre-ring backfill does.
    int16_t scratch[512 * 2];
    while (audio_read_samples(scratch, 512, 0) > 0) { }
    if (backfill_samples > target_samples) backfill_samples = target_samples;
    memset(dst, 0, backfill_samples * sizeof(float));
    s_cap_dst = dst;
    s_cap_target = target_samples;
    s_cap_count = backfill_samples;
    s_mix_k = 0;
    s_acc = 0.0f;
    s_acc_n = 0;
    return ESP_OK;
}

int dsp_ft8_capture_progress(void)
{
    capture_pump();
    return (int)s_cap_count;
}

esp_err_t dsp_ft8_capture_finish(uint32_t timeout_ms)
{
    (void)timeout_ms;
    capture_pump();
    if (s_cap_dst && s_cap_count < s_cap_target)
        memset(&s_cap_dst[s_cap_count], 0, (s_cap_target - s_cap_count) * sizeof(float));
    s_cap_dst = NULL;
    return ESP_OK;
}

esp_err_t dsp_ft8_capture(float *dst_180000, uint32_t timeout_ms)
{ (void)dst_180000; (void)timeout_ms; return ESP_ERR_TIMEOUT; } // legacy one-shot; the port uses begin/finish

void dsp_cw_forward_enable(bool en) { (void)en; }
void dsp_cw_forward(const int16_t *pairs, size_t n_pairs) { (void)pairs; (void)n_pairs; }
size_t dsp_cw_read(int16_t *dst, size_t n_pairs, uint32_t timeout_ms) { (void)dst; (void)n_pairs; (void)timeout_ms; return 0; }

void dsp_set_zoom(float zoom_factor, int pan_offset_bins, int if_bin_shift)
{ (void)zoom_factor; (void)pan_offset_bins; (void)if_bin_shift; }
int dsp_get_zoom_decim(void) { return 1; } // 1 == zoom-FFT inactive, per dsp.h's own contract
float dsp_get_zoom_residual(void) { return 1.0f; }
const float *dsp_get_zoom_spectrum(void) { return NULL; }
