// The SDL port's device-selection API - the sim_* entry points the *_sdl.c
// port siblings export BEYOND their real header contracts (audio.h/cat.h
// know nothing about picking a source; on hardware there is nothing to
// pick). Consumed by desktop/main.c (CLI args) and the native menus
// (desktop/native_menu_macos.m).
#pragma once
#include <stdbool.h>
#include <stdint.h>

// --- audio source (main/audio/audio_sdl.c) ---
bool sim_audio_select_wav(const char *path);            // replay a 16-bit PCM WAV, looped
bool sim_audio_select_capture(const char *device_name); // live OS audio input (SDL capture name)
void sim_audio_select_none(void);                       // silence
int  sim_audio_get_source_kind(void);                   // 0=none 1=wav 2=capture
const char *sim_audio_get_source_name(void);            // path / device name / ""
uint32_t sim_audio_get_sample_rate(void);
int  sim_audio_get_channels(void);

// --- CAT transceiver (main/cat/cat_sdl.c) ---
// With no serial port selected the CAT layer talks to the mock rig
// (desktop/mock_rig.c) instead - same commands, same replies, no wire.
bool sim_cat_select_port(const char *device_path);      // real serial CAT
void sim_cat_select_none(void);                         // back to the mock rig
const char *sim_cat_get_port(void);                     // "" while on the mock

// --- native menus (desktop/native_menu_macos.m; no-op stub elsewhere) ---
void native_menu_install(void);

// --- FT8 RX (desktop/ft8_rx_sdl.c) ---
// Slot-aligned decode loop using the real ft8_lib; active while the FT8
// screen is up. The port's stand-in for ft8_test.c's slot loop (RX only).
void ft8_rx_sdl_start(void);
