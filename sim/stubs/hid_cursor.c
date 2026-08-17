// Real implementation (matches main/hid_cursor.c's contract) - trivial
// enough that there's no reason to stub it, even though this pass drives
// the pointer through LVGL's own SDL mouse indev instead (see
// desktop/display.c) rather than through this USB/BLE accumulator path.
#include "hid_cursor.h"

static int s_x = 640, s_y = 360;
static uint8_t s_buttons = 0;
static int s_wheel = 0;
static bool s_present[HID_CURSOR_SRC_COUNT];

void hid_cursor_apply(int dx, int dy, uint8_t buttons)
{
    s_x += dx; s_y += dy;
    if (s_x < 0) s_x = 0; if (s_x > 1279) s_x = 1279;
    if (s_y < 0) s_y = 0; if (s_y > 719) s_y = 719;
    s_buttons = buttons;
}

void hid_cursor_get(int *x, int *y, uint8_t *buttons)
{
    if (x) *x = s_x;
    if (y) *y = s_y;
    if (buttons) *buttons = s_buttons;
}

void hid_cursor_add_wheel(int clicks) { s_wheel += clicks; }
int hid_cursor_take_wheel(void) { int w = s_wheel; s_wheel = 0; return w; }

void hid_cursor_set_present(hid_cursor_src_t src, bool present)
{
    if (src < HID_CURSOR_SRC_COUNT) s_present[src] = present;
}

bool hid_cursor_present(void)
{
    for (int i = 0; i < HID_CURSOR_SRC_COUNT; i++) if (s_present[i]) return true;
    return false;
}
