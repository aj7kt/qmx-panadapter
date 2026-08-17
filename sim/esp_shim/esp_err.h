#pragma once
// Tier-1 mechanical shim: just enough of ESP-IDF's esp_err.h for desktop
// builds. See sim/README.md.
//
// The REAL esp_err.h includes stdint.h, stdio.h, assert.h and stdlib.h, and
// project headers rely on those guarantees without including them directly
// (render_waterfall.h uses uint8_t via this chain; ft8_time_modal.c calls
// atoi()). Mirror the real header exactly — macOS's libc masks a missing
// stdint.h transitively, glibc does not, so this is what keeps the Linux CI
// build honest.
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

typedef int esp_err_t;

#define ESP_OK          0
#define ESP_FAIL        -1
#define ESP_ERR_NO_MEM        0x101
#define ESP_ERR_INVALID_ARG   0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE  0x104
#define ESP_ERR_NOT_FOUND     0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT       0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC    0x109
#define ESP_ERR_INVALID_VERSION 0x10A
#define ESP_ERR_INVALID_MAC    0x10B
#define ESP_ERR_NOT_FINISHED   0x10C

#define ESP_ERROR_CHECK(x) do { (void)(x); } while (0)

static inline const char *esp_err_to_name(esp_err_t e) { (void)e; return "ESP_ERR"; }
