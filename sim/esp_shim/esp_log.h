#pragma once
#include <stdio.h>

// Tier-1 shim: ESP_LOG* -> stderr. Real firmware routes these through
// diag_log.c's vprintf hook; on desktop, stderr is our diag log.
typedef enum { ESP_LOG_NONE, ESP_LOG_ERROR, ESP_LOG_WARN, ESP_LOG_INFO, ESP_LOG_DEBUG, ESP_LOG_VERBOSE } esp_log_level_t;

#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "W (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stderr, "I (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)

static inline void esp_log_level_set(const char *tag, esp_log_level_t level) { (void)tag; (void)level; }
