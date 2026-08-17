#include "ring_buffer.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

struct ring_buffer {
    int16_t *data;        // interleaved pairs, capacity_pairs * 2 int16s
    size_t capacity_pairs;
    size_t head;           // next write index (in pairs)
    size_t count;          // pairs currently held
    SDL_mutex *mtx;
    SDL_cond *cond;
};

ring_buffer_t *ring_buffer_create(size_t capacity_pairs)
{
    ring_buffer_t *rb = calloc(1, sizeof(*rb));
    if (!rb) return NULL;
    rb->data = calloc(capacity_pairs * 2, sizeof(int16_t));
    if (!rb->data) { free(rb); return NULL; }
    rb->capacity_pairs = capacity_pairs;
    rb->mtx = SDL_CreateMutex();
    rb->cond = SDL_CreateCond();
    return rb;
}

void ring_buffer_destroy(ring_buffer_t *rb)
{
    if (!rb) return;
    SDL_DestroyCond(rb->cond);
    SDL_DestroyMutex(rb->mtx);
    free(rb->data);
    free(rb);
}

void ring_buffer_write(ring_buffer_t *rb, const int16_t *pairs, size_t n_pairs)
{
    if (!rb || !pairs || n_pairs == 0) return;

    SDL_LockMutex(rb->mtx);
    // Never block the producer: if this write would overflow, drop the
    // oldest data to make room (same "drop on overflow, never block USB"
    // rule main/audio/audio.c follows).
    if (n_pairs > rb->capacity_pairs) {
        pairs += (n_pairs - rb->capacity_pairs);
        n_pairs = rb->capacity_pairs;
    }
    size_t write_at = (rb->head) % rb->capacity_pairs;
    for (size_t i = 0; i < n_pairs; i++) {
        size_t idx = (write_at + i) % rb->capacity_pairs;
        rb->data[idx * 2 + 0] = pairs[i * 2 + 0];
        rb->data[idx * 2 + 1] = pairs[i * 2 + 1];
    }
    rb->head = (write_at + n_pairs) % rb->capacity_pairs;
    rb->count += n_pairs;
    if (rb->count > rb->capacity_pairs) {
        rb->count = rb->capacity_pairs; // oldest got overwritten
    }
    SDL_CondSignal(rb->cond);
    SDL_UnlockMutex(rb->mtx);
}

size_t ring_buffer_read(ring_buffer_t *rb, int16_t *dst, size_t max_pairs, uint32_t timeout_ms)
{
    if (!rb || !dst || max_pairs == 0) return 0;

    SDL_LockMutex(rb->mtx);
    if (rb->count == 0) {
        SDL_CondWaitTimeout(rb->cond, rb->mtx, timeout_ms);
    }
    if (rb->count == 0) {
        SDL_UnlockMutex(rb->mtx);
        return 0;
    }
    size_t n = rb->count < max_pairs ? rb->count : max_pairs;
    // Oldest data starts n_pairs before head, wrapped.
    size_t read_start = (rb->head + rb->capacity_pairs - rb->count) % rb->capacity_pairs;
    for (size_t i = 0; i < n; i++) {
        size_t idx = (read_start + i) % rb->capacity_pairs;
        dst[i * 2 + 0] = rb->data[idx * 2 + 0];
        dst[i * 2 + 1] = rb->data[idx * 2 + 1];
    }
    rb->count -= n;
    SDL_UnlockMutex(rb->mtx);
    return n;
}

void ring_buffer_clear(ring_buffer_t *rb)
{
    if (!rb) return;
    SDL_LockMutex(rb->mtx);
    rb->head = 0;
    rb->count = 0;
    SDL_UnlockMutex(rb->mtx);
}
