#pragma once
// Tier-1 mechanical shim: just enough of ESP-IDF's esp_err.h for desktop
// builds. See sim/README.md.
//
// stdlib.h: real ESP-IDF's header chain pulls this in transitively from
// several places, so project files (e.g. ft8_time_modal.c's atoi() call)
// rely on it being ambiently available without including it themselves.
// esp_err.h is close to universally the first project include, so it's the
// natural place to replicate that.
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
