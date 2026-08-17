#include "device_list.h"
#include <SDL2/SDL.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>

void device_list_audio_inputs(device_list_t *out)
{
    out->count = 0;
    int n = SDL_GetNumAudioDevices(1 /* iscapture */);
    for (int i = 0; i < n && out->count < DEVICE_LIST_MAX; i++) {
        const char *name = SDL_GetAudioDeviceName(i, 1);
        if (!name) continue;
        snprintf(out->names[out->count], DEVICE_NAME_MAX, "%s", name);
        out->count++;
    }
}

static bool looks_like_modem_port(const char *name)
{
#if defined(__APPLE__)
    // Prefer the "cu." (call-out) devices over "tty." - the tty. variant
    // blocks open() waiting for carrier detect, cu. does not, and a QMX's
    // CDC-ACM port never asserts DCD.
    return strncmp(name, "cu.usbmodem", 11) == 0 ||
           strncmp(name, "cu.usbserial", 12) == 0 ||
           strncmp(name, "cu.SLAB_USB", 11) == 0;
#else
    return strncmp(name, "ttyACM", 6) == 0 || strncmp(name, "ttyUSB", 6) == 0;
#endif
}

void device_list_serial_ports(device_list_t *out)
{
    out->count = 0;
    DIR *d = opendir("/dev");
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && out->count < DEVICE_LIST_MAX) {
        if (!looks_like_modem_port(ent->d_name)) continue;
        snprintf(out->names[out->count], DEVICE_NAME_MAX, "/dev/%s", ent->d_name);
        out->count++;
    }
    closedir(d);
}
