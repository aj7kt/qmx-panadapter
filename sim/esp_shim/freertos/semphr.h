#pragma once
#include "FreeRTOS.h"
#include <SDL2/SDL.h>

typedef SDL_sem *SemaphoreHandle_t;

static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return SDL_CreateSemaphore(1); }
static inline SemaphoreHandle_t xSemaphoreCreateBinary(void) { return SDL_CreateSemaphore(0); }
static inline SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max, UBaseType_t initial)
{
    (void)max;
    return SDL_CreateSemaphore((Uint32)initial);
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks)
{
    if (!s) return pdFALSE;
    if (ticks == portMAX_DELAY) return SDL_SemWait(s) == 0 ? pdTRUE : pdFALSE;
    return SDL_SemWaitTimeout(s, (Uint32)ticks) == 0 ? pdTRUE : pdFALSE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    if (!s) return pdFALSE;
    return SDL_SemPost(s) == 0 ? pdTRUE : pdFALSE;
}

static inline void vSemaphoreDelete(SemaphoreHandle_t s) { if (s) SDL_DestroySemaphore(s); }
