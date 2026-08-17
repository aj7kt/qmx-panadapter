#pragma once
#include <stdbool.h>

#define DEVICE_LIST_MAX 32
#define DEVICE_NAME_MAX 256

typedef struct {
    char names[DEVICE_LIST_MAX][DEVICE_NAME_MAX];
    int count;
} device_list_t;

// SDL audio *capture* devices - what a real QMX's UAC interface (or any
// other mic/line-in) shows up as to the OS. Call after SDL_Init(SDL_INIT_AUDIO).
void device_list_audio_inputs(device_list_t *out);

// Serial ports that look like a USB CDC-ACM device: /dev/cu.usbmodem* /
// /dev/cu.usbserial* on macOS, /dev/ttyACM* / /dev/ttyUSB* on Linux. This is
// exactly what a QMX's CAT interface enumerates as on either OS - no driver
// needed, the OS already has one.
void device_list_serial_ports(device_list_t *out);
