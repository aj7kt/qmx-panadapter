#include "cat_log.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LOG_LINES 500
#define LINE_MAX 96

static char s_lines[LOG_LINES][LINE_MAX];
static int s_head = 0, s_count = 0;
static SDL_mutex *s_mtx = NULL;
static cat_log_listener_t s_listener = NULL;

void cat_log_init(void)
{
    if (!s_mtx) s_mtx = SDL_CreateMutex();
}

void cat_log_append(const char *dir, const char *text)
{
    if (!s_mtx) return; // not initialised - drop rather than race
    char line[LINE_MAX];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    localtime_r(&ts.tv_sec, &tmv);
    snprintf(line, sizeof(line), "%02d:%02d:%02d.%03d %s %s",
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (int)(ts.tv_nsec / 1000000),
             dir, text);

    cat_log_listener_t fn;
    SDL_LockMutex(s_mtx);
    snprintf(s_lines[s_head], LINE_MAX, "%s", line);
    s_head = (s_head + 1) % LOG_LINES;
    if (s_count < LOG_LINES) s_count++;
    fn = s_listener;
    SDL_UnlockMutex(s_mtx);

    if (fn) fn(line); // outside the lock - the listener may do UI marshalling
}

void cat_log_set_listener(cat_log_listener_t fn)
{
    if (!s_mtx) cat_log_init();
    // Snapshot the ring, install the listener, then replay the snapshot.
    static char replay[LOG_LINES][LINE_MAX]; // one-shot path, static is fine
    int n;
    SDL_LockMutex(s_mtx);
    n = s_count;
    for (int i = 0; i < n; i++) {
        int idx = (s_head - n + i + LOG_LINES) % LOG_LINES;
        memcpy(replay[i], s_lines[idx], LINE_MAX);
    }
    s_listener = fn;
    SDL_UnlockMutex(s_mtx);
    if (fn)
        for (int i = 0; i < n; i++) fn(replay[i]);
}
