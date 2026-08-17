#pragma once
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

// Tier-1 shim: microsecond timestamp only (the one thing main/ui/*.c and
// friends actually call esp_timer for). No callback/timer-object support -
// nothing in the ported files creates an esp_timer, they only read the clock.
static inline int64_t esp_timer_get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}
