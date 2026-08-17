#pragma once
// Tier-1 shim: just enough FreeRTOS-shaped API, backed by SDL's threading
// primitives (already a hard dependency of this build, and portable across
// macOS/Linux without POSIX-semaphore platform gotchas). Real firmware runs
// at CONFIG_FREERTOS_HZ=1000, i.e. 1 tick == 1 ms - matched here so
// pdMS_TO_TICKS() round-trips exactly like on the real device.
#include <stdint.h>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define pdPASS 1
#define pdFAIL 0
#define pdTRUE 1
#define pdFALSE 0

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY ((TickType_t)0xFFFFFFFFUL)
#define portTICK_PERIOD_MS 1
#define tskNO_AFFINITY (-1)
