// SDL-port sibling of audio.c — same audio.h contract, desktop backend.
// Compiled ONLY by the SDL port (sim/); the ESP-IDF build's explicit
// SRCS list in main/CMakeLists.txt never includes *_sdl.c. See docs/porting.md.
//
// Three selectable sources (File menu / Settings dialog / CLI — see
// sim/desktop/sim_devices.h): a WAV file replayed on a pacing thread, LIVE
// capture from any OS audio input (a real QMX's UAC interface is exactly
// that — SDL converts whatever the device delivers to 48 kHz stereo S16,
// the same stream shape the firmware's UAC host sees), or silence.
//
// WAV replay SYNTHESIZES the QMX's stream: a demodulated-audio recording
// (mono — WebSDR/WSJT-X captures) is upsampled to 48 kHz and mixed up to
// the +12 kHz IF the radio presents (see CLAUDE.md "12 kHz IF offset"), so
// it lands where the UI expects signals relative to the dial and the FT8
// capture path (dsp_sdl.c) can mix it back down exactly like the firmware
// does. The +12 kHz shift at 48 kS/s is exactly Fs/4, so the mixer is the
// rotation sequence (1, +j, -1, -j) — integer swaps, no trig. Replay also
// RE-ALIGNS to the wall-clock 15 s FT8 slot grid: when the file runs out,
// silence fills until the next boundary and the file restarts there, so a
// one-slot recording plays like live air and decodes.
#include "audio.h"
#include "ring_buffer.h"
#include "wav_loader.h"
#include "sim_devices.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RING_CAPACITY_PAIRS 96000

static ring_buffer_t *s_rb = NULL;
static wav_data_t s_wav;
static SDL_Thread *s_wav_thread = NULL;
static volatile bool s_wav_stop = false;
static SDL_AudioDeviceID s_cap_dev = 0;
static uint32_t s_sample_rate = 48000;
static int s_channels = 2;
static uint32_t s_dropped_total = 0;
static int s_source_kind = 0; // 0=none 1=wav 2=capture
static char s_source_name[512] = "";

// Upconvert a demodulated-audio WAV to the QMX's 48 kHz I/Q shape: linear
// ×U upsample of the (left-channel) audio, then multiply by e^{+j·2π·12k·t}
// = (+j)^k at 48 kS/s. Replaces wav->pairs in place; requires the source
// rate to divide 48000 (12000, 8000, 16000, 24000, 48000 all do).
static bool upconvert_to_iq(wav_data_t *wav)
{
    if (wav->sample_rate == 0 || (48000 % wav->sample_rate) != 0) return false;
    uint32_t U = 48000 / wav->sample_rate;
    size_t n_in = wav->num_pairs;
    size_t n_out = n_in * U;
    int16_t *out = malloc(n_out * 2 * sizeof(int16_t));
    if (!out) return false;
    for (size_t k = 0; k < n_out; k++) {
        size_t i = k / U, f = k % U;
        float a0 = wav->pairs[i * 2];
        float a1 = (i + 1 < n_in) ? wav->pairs[(i + 1) * 2] : 0.0f;
        int16_t a = (int16_t)lrintf(a0 + (a1 - a0) * ((float)f / (float)U));
        int16_t I = 0, Q = 0;
        switch (k & 3) { // (+j)^k
            case 0: I = a; break;
            case 1: Q = a; break;
            case 2: I = (int16_t)-a; break;
            default: Q = (int16_t)-a; break;
        }
        out[k * 2] = I;
        out[k * 2 + 1] = Q;
    }
    free(wav->pairs);
    wav->pairs = out;
    wav->num_pairs = n_out;
    wav->sample_rate = 48000;
    return true;
}

static int wav_pacer_thread(void *arg)
{
    (void)arg;
    static int16_t silence[2 * 480];
    memset(silence, 0, sizeof(silence));
    // Absolute-clock pacing: SDL_Delay(N) sleeps AT LEAST N ms, so a
    // fixed-chunk-per-tick pacer runs ~10% slow and drifts off the 15 s slot
    // grid within a couple of slots (observed: third slot decoded nothing).
    // Instead, each tick writes however many pairs are DUE by wall clock
    // since this playback pass started - rate-exact regardless of scheduler
    // jitter, so the file stays locked to the boundary it restarted on.
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int64_t t0_ms = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
    int64_t last_slot = t0_ms / 15000;
    int64_t rate_k = s_wav.sample_rate / 1000; // pairs per ms
    size_t pos = 0;
    bool waiting = false; // file exhausted, holding silence for the boundary
    while (!s_wav_stop) {
        clock_gettime(CLOCK_REALTIME, &ts);
        int64_t ms = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
        int64_t slot = ms / 15000;
        if (slot != last_slot) {
            last_slot = slot;
            if (waiting) { // restart ON the boundary
                pos = 0;
                waiting = false;
                t0_ms = slot * 15000LL;
            }
        }
        if (!waiting) {
            int64_t due = (ms - t0_ms) * rate_k;
            if (due > (int64_t)pos) {
                size_t n = (size_t)(due - (int64_t)pos);
                if (n > 4800) n = 4800; // cap a catch-up burst at ~100 ms
                size_t remain = s_wav.num_pairs - pos;
                if (n >= remain) n = remain;
                if (n > 0) ring_buffer_write(s_rb, &s_wav.pairs[pos * 2], n);
                pos += n;
                if (pos >= s_wav.num_pairs) {
                    // A file that is an exact multiple of 15 s ends ON a
                    // boundary - and the slot-change branch above has already
                    // consumed that crossing by the time we get here, which
                    // would idle the NEXT slot entirely (observed: decodes
                    // alternated good slot / silent slot). If we are just
                    // past a boundary, restart anchored to it immediately;
                    // otherwise hold silence for the next one.
                    if (ms % 15000 < 1000) {
                        pos = 0;
                        t0_ms = (ms / 15000) * 15000LL;
                    } else {
                        waiting = true;
                    }
                }
            }
        } else {
            ring_buffer_write(s_rb, silence, 480); // keep the stream live
        }
        SDL_Delay(5);
    }
    return 0;
}

static void capture_cb(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    ring_buffer_write(s_rb, (const int16_t *)stream, (size_t)len / 4); // S16 stereo
}

static void stop_source(void)
{
    if (s_wav_thread) {
        s_wav_stop = true;
        SDL_WaitThread(s_wav_thread, NULL);
        s_wav_thread = NULL;
        wav_free(&s_wav);
    }
    if (s_cap_dev) {
        SDL_CloseAudioDevice(s_cap_dev);
        s_cap_dev = 0;
    }
    if (s_rb) ring_buffer_clear(s_rb);
    s_source_kind = 0;
    s_source_name[0] = '\0';
}

esp_err_t audio_init(void)
{
    SDL_InitSubSystem(SDL_INIT_AUDIO); // LVGL's SDL backend only inits video
    s_rb = ring_buffer_create(RING_CAPACITY_PAIRS);
    return s_rb ? ESP_OK : ESP_FAIL;
}

bool sim_audio_select_wav(const char *path)
{
    wav_data_t wav;
    if (!wav_load(path, &wav)) return false;

    uint32_t src_rate = wav.sample_rate;
    int src_ch = wav.source_channels;
    bool synthesized = false;
    if (!(src_ch == 2 && src_rate == 48000)) {
        // Anything that isn't already the radio's native 48 kHz I/Q is
        // treated as demodulated audio and upconverted to it (see the file
        // comment). A stereo non-48k file gets its left channel used.
        synthesized = upconvert_to_iq(&wav);
        if (!synthesized)
            fprintf(stderr, "audio: %u Hz doesn't divide 48000 - replaying raw "
                            "(display positions will be wrong)\n", src_rate);
    }

    stop_source();
    s_wav = wav;
    s_sample_rate = wav.sample_rate;
    s_channels = 2; // the ring now holds true I/Q either way (except raw fallback)
    if (!synthesized && src_ch == 1) s_channels = 1;
    s_wav_stop = false;
    s_wav_thread = SDL_CreateThread(wav_pacer_thread, "wav_pacer", NULL);
    if (!s_wav_thread) { wav_free(&s_wav); return false; }
    s_source_kind = 1;
    snprintf(s_source_name, sizeof(s_source_name), "%s", path);
    fprintf(stderr, "audio: replaying %s (%u Hz %d ch source%s, %.1f s; "
                    "restarts on each 15 s slot boundary)\n",
            path, src_rate, src_ch,
            synthesized ? ", upconverted to 48 kHz I/Q at +12 kHz IF" : "",
            (double)s_wav.num_pairs / s_wav.sample_rate);
    return true;
}

bool sim_audio_select_capture(const char *device_name)
{
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = capture_cb;
    // allowed_changes = 0: SDL converts whatever the device really delivers
    // to exactly the spec above, so the ring always holds 48 kHz S16 pairs.
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(device_name, 1, &want, NULL, 0);
    if (!dev) {
        fprintf(stderr, "audio: could not open capture '%s': %s\n", device_name, SDL_GetError());
        return false;
    }
    stop_source();
    s_cap_dev = dev;
    s_sample_rate = 48000;
    s_channels = 2;
    s_source_kind = 2;
    snprintf(s_source_name, sizeof(s_source_name), "%s", device_name);
    SDL_PauseAudioDevice(dev, 0);
    fprintf(stderr, "audio: capturing from '%s' (48000 Hz stereo)\n", device_name);
    return true;
}

void sim_audio_select_none(void) { stop_source(); }

int sim_audio_get_source_kind(void) { return s_source_kind; }
const char *sim_audio_get_source_name(void) { return s_source_name; }
uint32_t sim_audio_get_sample_rate(void) { return s_sample_rate; }
int sim_audio_get_channels(void) { return s_channels; }

size_t audio_read_samples(int16_t *dst, size_t max_pairs, uint32_t timeout_ms)
{
    if (!s_rb) return 0;
    return ring_buffer_read(s_rb, dst, max_pairs, timeout_ms);
}

size_t audio_ring_backlog_pairs(void) { return 0; }
uint32_t audio_get_dropped_total(void) { return s_dropped_total; }
bool audio_uac_active(void) { return s_source_kind != 0; }
void audio_request_reset(void) { }
void audio_reset_frame_alignment(void) { }
void audio_usb_shutdown(void) { }
