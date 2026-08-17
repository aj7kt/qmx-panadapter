#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Thread-safe ring buffer of interleaved int16 stereo pairs (L/R or I/Q).
// Mirrors the shape of the real firmware's audio ring buffer closely enough
// that audio_sim.c's audio_read_samples() can present the exact same
// signature as main/audio/audio.h.

typedef struct ring_buffer ring_buffer_t;

ring_buffer_t *ring_buffer_create(size_t capacity_pairs);
void ring_buffer_destroy(ring_buffer_t *rb);

// Drops oldest data on overflow (never blocks the writer) - same "never
// block the producer" rule the real audio_task follows.
void ring_buffer_write(ring_buffer_t *rb, const int16_t *pairs, size_t n_pairs);

// Blocks up to timeout_ms waiting for at least 1 pair, then returns as many
// as are available (up to max_pairs). Returns 0 on timeout.
size_t ring_buffer_read(ring_buffer_t *rb, int16_t *dst, size_t max_pairs, uint32_t timeout_ms);

void ring_buffer_clear(ring_buffer_t *rb);
