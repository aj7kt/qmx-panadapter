#include "wav_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd_u16le(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

bool wav_load(const char *path, wav_data_t *out)
{
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "wav_load: cannot open %s\n", path);
        return false;
    }

    uint8_t hdr[12];
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "wav_load: %s is not a RIFF/WAVE file\n", path);
        fclose(f);
        return false;
    }

    int channels = 0;
    uint32_t sample_rate = 0;
    int bits_per_sample = 0;
    long data_offset = -1;
    uint32_t data_size = 0;

    // Walk chunks; tolerate any extra chunks (LIST, fact, etc.) between fmt and data.
    while (1) {
        uint8_t chdr[8];
        if (fread(chdr, 1, 8, f) != 8) break;
        char id[5] = {0};
        memcpy(id, chdr, 4);
        uint32_t size = rd_u32le(chdr + 4);
        long chunk_start = ftell(f);

        if (memcmp(id, "fmt ", 4) == 0) {
            uint8_t fmt[16];
            if (size < 16 || fread(fmt, 1, 16, f) != 16) {
                fprintf(stderr, "wav_load: %s has a truncated fmt chunk\n", path);
                fclose(f);
                return false;
            }
            uint16_t audio_format = rd_u16le(fmt + 0);
            channels = rd_u16le(fmt + 2);
            sample_rate = rd_u32le(fmt + 4);
            bits_per_sample = rd_u16le(fmt + 14);
            if (audio_format != 1 /* PCM */ || bits_per_sample != 16) {
                fprintf(stderr, "wav_load: %s is format=%u bits=%d - only 16-bit PCM is supported\n",
                        path, audio_format, bits_per_sample);
                fclose(f);
                return false;
            }
        } else if (memcmp(id, "data", 4) == 0) {
            data_offset = chunk_start;
            data_size = size;
        }

        // Chunks are word-aligned; skip to the next one.
        long next = chunk_start + (long)size + (size & 1);
        if (fseek(f, next, SEEK_SET) != 0) break;
    }

    if (channels == 0 || sample_rate == 0 || data_offset < 0 || data_size == 0) {
        fprintf(stderr, "wav_load: %s missing fmt or data chunk\n", path);
        fclose(f);
        return false;
    }
    if (channels > 2) {
        fprintf(stderr, "wav_load: %s has %d channels - only mono/stereo supported\n", path, channels);
        fclose(f);
        return false;
    }

    size_t bytes_per_frame = (size_t)channels * 2;
    size_t num_frames = data_size / bytes_per_frame;

    int16_t *raw = malloc(num_frames * channels * sizeof(int16_t));
    if (!raw) {
        fclose(f);
        return false;
    }
    if (fseek(f, data_offset, SEEK_SET) != 0 ||
        fread(raw, sizeof(int16_t), num_frames * channels, f) != num_frames * (size_t)channels) {
        fprintf(stderr, "wav_load: %s short read on data chunk\n", path);
        free(raw);
        fclose(f);
        return false;
    }
    fclose(f);

    int16_t *pairs = malloc(num_frames * 2 * sizeof(int16_t));
    if (!pairs) {
        free(raw);
        return false;
    }
    if (channels == 2) {
        memcpy(pairs, raw, num_frames * 2 * sizeof(int16_t));
    } else {
        for (size_t i = 0; i < num_frames; i++) {
            pairs[i * 2 + 0] = raw[i];
            pairs[i * 2 + 1] = raw[i]; // duplicate mono into both channels
        }
    }
    free(raw);

    out->pairs = pairs;
    out->num_pairs = num_frames;
    out->sample_rate = sample_rate;
    out->source_channels = channels;
    return true;
}

void wav_free(wav_data_t *w)
{
    if (!w) return;
    free(w->pairs);
    w->pairs = NULL;
    w->num_pairs = 0;
}
