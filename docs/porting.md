# Porting: the platform boundary

This project has two build targets today: the **Tab5 firmware** (ESP-IDF,
`main/CMakeLists.txt`) and the **SDL desktop port** (`sim/`,
macOS/Linux, no ESP-IDF and no hardware). This document is the contract that
keeps them building from the same core code — and the map for any future
target (STM32, Android, another SBC).

The boundary was not designed up front; it was *discovered* by making the
real UI compile on a desktop, and then formalized. That matters: everything
below is empirically the minimum, not a speculative abstraction.

## The three layers

```
┌─────────────────────────────────────────────────────────────┐
│ Portable core — one copy, every target                      │
│   ui/*  render/*  ft8 engine + ft8_lib  util/ (portable     │
│   set)  CAT *protocol* logic  ADIF/LoTW generators          │
├─────────────────────────────────────────────────────────────┤
│ Platform port — one per target                              │
│   OSAL (tasks/mutexes/log/timer/heap) + the five module     │
│   backends: display, settings, audio-in, serial, FFT kernel │
│   Tab5: the existing main/*.c hardware code                 │
│   SDL:  main/**/*_sdl.c + sim/esp_shim/             │
├─────────────────────────────────────────────────────────────┤
│ Peripherals — reached THROUGH a port, owned by neither      │
│   the transceiver (see below), GPS-over-CAT, PSK Reporter…  │
└─────────────────────────────────────────────────────────────┘
```

## The transceiver is a peripheral, not a platform

The radio is deliberately **not** part of the platform layer, and platform
artifacts are deliberately not named after it. From the core's point of view
the transceiver is exactly two byte streams:

- a **CAT serial stream** (Kenwood-style protocol — `FA;`/`MD;`/`FW;` etc.)
- an **IQ audio stream** (stereo PCM, 48 kHz on the current radio)

Each *port* supplies the transport for those streams (Tab5: USB CDC-ACM +
UAC host; SDL: a POSIX serial device + WAV replay or an OS audio input —
a QMX plugged into a desktop machine presents both streams as ordinary OS
devices, no USB-host stack needed). The *core* supplies the protocol.

A port may also *emulate* the peripheral: the SDL port's mock rig
(`sim/desktop/mock_rig.c`, selectable QMX variants) terminates the CAT
byte stream in-process, speaking the same command strings the real radio
does (manual-verified, pinned by `test/mock_rig_harness.c`) — so core code
cannot tell the difference, which is the test of the boundary being real.

The QRP Labs QMX/QMX+ is the current — and only — implementation of that
peripheral, and `cat.c`'s quirk handling (the SSB-filter three-write dance,
`Q9` session state, `MD8;` readback…) is effectively the QMX driver. If a
second transceiver is ever targeted, the split is: quirk handling becomes
per-radio, the poll loop and transport stay where they are. Don't pre-build
that abstraction; do keep new radio-specific logic inside `cat.c` (or a
sibling), never in the core.

## The OSAL: a documented subset of ESP-IDF/FreeRTOS

The portable core does **not** use a neutral invented platform API. It calls
ESP-IDF and FreeRTOS directly — `xTaskCreatePinnedToCore`, `ESP_LOGI`,
`esp_timer_get_time`, `heap_caps_malloc`, semaphores — and *that subset* is
the platform API. Each non-ESP port implements it:

- **The spec is `sim/esp_shim/`.** Those headers are the complete,
  empirically-derived list of what the core actually needs (~30 functions
  across `esp_log.h`, `esp_timer.h`, `esp_err.h`, `esp_heap_caps.h`,
  `freertos/{FreeRTOS,task,semphr}.h`, plus a few type-only headers). On the
  SDL port they are backed by SDL2 threads/semaphores — a real scheduler,
  just not FreeRTOS.
- **Why not a neutral API:** it would mean rewriting thousands of call
  sites for zero functional gain, and this codebase's memory discipline
  (`MALLOC_CAP_SPIRAM` vs internal, task pinning, stack sizes — see
  CLAUDE.md) is *encoded in those call sites*. Other ports simply map the
  caps flags to plain `malloc` and ignore core pinning.
- **Growing it:** if core code starts using an ESP-IDF function the shim
  lacks, the SDL build breaks at compile time and the fix is a few lines in
  `esp_shim/`. That is the drift alarm working as designed — add the shim
  function, don't `#ifdef` the core.

## The five module contracts (Tier 2)

These modules produce real content and get a **real reimplementation** per
port, as a sibling file in the same directory, named `<module>_<port>.c`:

| Contract | Tab5 backend | SDL backend |
|---|---|---|
| `display/display.h` | MIPI-DSI + BSP + touch | `display_sdl.c` — LVGL's own SDL2 driver |
| `storage/settings.h` | NVS + debounced flush | `settings_sdl.c` — in-memory struct |
| `audio/audio.h` | USB UAC host + ring | `audio_sdl.c` — WAV replay on a pacing thread |
| `cat/cat.h` | USB CDC-ACM | `cat_sdl.c` — POSIX termios serial |
| `dsp/dsp.h` | esp-dsp kernels | `dsp_sdl.c` — kiss_fft |

Rules:

- **A sibling implements the whole header or answers honestly.** Every
  function either works for real or returns the header's own documented
  "inactive / not available" state — never a fake positive. (Example:
  `dsp_sdl.c`'s `dsp_get_zoom_spectrum()` returns `NULL`, which is `dsp.h`'s
  documented zoom-off state.)
- **Adding a function to one of these headers means touching every
  `*_sdl.c` sibling in the same change.** They live in the same directory
  precisely so this is visible in review; the CI build
  (`.github/workflows/sdl-port-build.yml`) fails the push if it's missed.
- **The ESP-IDF build never compiles `*_sdl.c`** — `main/CMakeLists.txt`
  lists its SRCS explicitly (no globs), so port siblings are invisible to
  the firmware by construction. Keep it that way; do not convert that list
  to a glob.

## Honest stubs (Tier 3) stay with the port

Subsystems a port doesn't model (on SDL today: the FT8 engine's internals,
WiFi, SD archive, diag log, BLE…) are stubbed in `sim/stubs/`, not
in `main/`. The line: a Tier-2 sibling is an *alternate real backend* and
belongs beside its header; a Tier-3 stub is a *port-specific absence* and
belongs in the port tree. Promoting a stub to a sibling (e.g. wiring real
FT8 decode into the SDL port — `ft8_lib` is already fully portable) moves
it from `stubs/` into `main/` under this contract.

## What is deliberately NOT done (yet)

- **No physical `core/` vs `ports/` split.** The ESP-IDF component layout,
  the release tooling, and every path in CLAUDE.md assume `main/`. The
  boundary is enforced by convention + CI instead; do the file move only
  when a second *hardware* target is actually being built, at which point
  it is mechanical because the discipline already held.
- **No radio abstraction layer.** One radio exists. See above.
