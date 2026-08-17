#pragma once
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

// Tier-1 shim: on desktop there's no internal-vs-PSRAM distinction, so every
// heap_caps_* call just becomes a plain malloc/free. The capability flags
// are accepted and ignored.
#define MALLOC_CAP_SPIRAM   (1 << 0)
#define MALLOC_CAP_INTERNAL (1 << 1)
#define MALLOC_CAP_DMA      (1 << 2)
#define MALLOC_CAP_8BIT     (1 << 3)
#define MALLOC_CAP_DEFAULT  (1 << 4)

static inline void *heap_caps_malloc(size_t size, uint32_t caps) { (void)caps; return malloc(size); }
static inline void *heap_caps_calloc(size_t n, size_t size, uint32_t caps) { (void)caps; return calloc(n, size); }
static inline void  heap_caps_free(void *p) { free(p); }
static inline size_t heap_caps_get_free_size(uint32_t caps) { (void)caps; return 64 * 1024 * 1024; }
static inline size_t heap_caps_get_total_size(uint32_t caps) { (void)caps; return 256 * 1024 * 1024; }
static inline size_t heap_caps_get_largest_free_block(uint32_t caps) { (void)caps; return 32 * 1024 * 1024; }
static inline size_t heap_caps_get_minimum_free_size(uint32_t caps) { (void)caps; return 32 * 1024 * 1024; }
