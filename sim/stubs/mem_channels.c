// Real implementation (matches main/storage/mem_channels.h's contract),
// in-memory instead of NVS-backed - same pattern as desktop/settings.c.
#include "mem_channels.h"
#include <string.h>

static mem_slot_t s_slots[MEM_SLOTS];
static bool s_demo_shown = false;

void mem_channels_init(void) { memset(s_slots, 0, sizeof(s_slots)); }

bool mem_channels_get(int idx, mem_slot_t *out)
{
    if (idx < 0 || idx >= MEM_SLOTS) return false;
    *out = s_slots[idx];
    return true;
}

void mem_channels_set(int idx, const mem_slot_t *slot)
{
    if (idx < 0 || idx >= MEM_SLOTS) return;
    s_slots[idx] = *slot;
}

void mem_channels_clear(int idx)
{
    if (idx < 0 || idx >= MEM_SLOTS) return;
    memset(&s_slots[idx], 0, sizeof(s_slots[idx]));
}

bool mem_channels_demo_shown(void) { return s_demo_shown; }
void mem_channels_mark_demo_shown(void) { s_demo_shown = true; }
