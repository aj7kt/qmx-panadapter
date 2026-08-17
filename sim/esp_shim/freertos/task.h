#pragma once
#include "FreeRTOS.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

static inline void vTaskDelay(TickType_t ticks) { SDL_Delay((Uint32)ticks); }

typedef struct { TaskFunction_t fn; void *arg; } sim_task_trampoline_t;

static inline int sim_task_entry(void *p)
{
    sim_task_trampoline_t *t = (sim_task_trampoline_t *)p;
    TaskFunction_t fn = t->fn;
    void *arg = t->arg;
    free(t);
    fn(arg); // real FreeRTOS task functions never return (they loop forever)
    return 0;
}

// stack size / priority / core id are accepted and ignored - the desktop
// OS scheduler handles this. Real firmware pins DSP/audio/render/CAT to
// specific cores for latency reasons that don't apply here.
static inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack,
                                                  void *arg, UBaseType_t prio, TaskHandle_t *out, int core_id)
{
    (void)stack; (void)prio; (void)core_id;
    sim_task_trampoline_t *t = malloc(sizeof(*t));
    t->fn = fn; t->arg = arg;
    SDL_Thread *th = SDL_CreateThread(sim_task_entry, name, t);
    if (out) *out = (TaskHandle_t)th;
    return th ? pdPASS : pdFAIL;
}

static inline BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack,
                                      void *arg, UBaseType_t prio, TaskHandle_t *out)
{
    return xTaskCreatePinnedToCore(fn, name, stack, arg, prio, out, -1);
}

static inline void vTaskDelete(TaskHandle_t h) { (void)h; /* real tasks here never exit */ }

static inline TickType_t xTaskGetTickCount(void) { return (TickType_t)SDL_GetTicks(); }
static inline void vTaskDelayUntil(TickType_t *prev_wake, TickType_t inc)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t target = *prev_wake + inc;
    if (target > now) SDL_Delay((Uint32)(target - now));
    *prev_wake = xTaskGetTickCount();
}
