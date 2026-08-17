#pragma once
// Tier-1 shim: util/status.c reads esp_app_get_description()->version for
// the bottom-bar firmware label.
typedef struct { const char *version; } esp_app_desc_t;
static inline const esp_app_desc_t *esp_app_get_description(void)
{
    static const esp_app_desc_t d = { .version = "sim-replica" };
    return &d;
}
