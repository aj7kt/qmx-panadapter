#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Minimal PCM WAV reader: 16-bit mono or stereo only (that covers both the
// real device's decoded-to-int16 format and the FT8 reference captures in
// test/wav_reference*). Always returns data as interleaved int16 pairs -
// mono files get the sample duplicated into both channels so callers can
// treat every source the same way audio_read_samples() does.

typedef struct {
    int16_t *pairs;      // interleaved L/R (or I/Q), num_pairs * 2 int16s
    size_t num_pairs;
    uint32_t sample_rate;
    int source_channels;  // 1 or 2, as read from the file (before mono->stereo duplication)
} wav_data_t;

// Returns false and leaves *out zeroed on failure (prints a reason to stderr).
bool wav_load(const char *path, wav_data_t *out);
void wav_free(wav_data_t *w);
