# panadapter_sdl — the real UI, running on desktop

This directory is the **SDL port** of the panadapter (see `docs/porting.md`
for the platform-boundary contract it builds against).

This is not a reimplementation: it compiles the
**actual** `main/ui/*.c` — 24 of the 26 real files, unmodified, including all
9,964 lines of `ui.c` itself — against LVGL's own SDL2 desktop backend
(built into LVGL 9.2.2, the exact version this project pins). Edit real UI
code, rebuild, see it on your Mac/Linux desktop in seconds — no ESP-IDF
build, no flashing, no hardware.

**Verified working, not just "it compiles":** boots to the real panadapter
screen at the real layout (`top=60px spectrum=200px labels=32px
waterfall=370px bottom=36px` — matches `docs/architecture.md` exactly,
because it's the same code computing it), builds the settings drawer, tune
modal, memory modal, identity/onboarding, and the FT8 view's 40-row decode
pool, loads settings, and renders continuously (~9% CPU, steady) for 45+
seconds with no crash.

## How this is possible

`main/ui/ui.c` directly `#include`s ~40 other project headers — CAT, audio,
DSP, settings, WiFi config, FT8 engine, spots, memory channels, the on-device
manual reader. Not a clean rendering layer. So this isn't "port the UI" —
it's "provide a desktop implementation or an honest stub for everything the
UI touches," split into three tiers:

### Tier 1 — mechanical ESP-IDF shims (`esp_shim/`)
Drop-in headers with the same names ESP-IDF ships (`esp_log.h`,
`esp_timer.h`, `esp_heap_caps.h`, `freertos/{FreeRTOS,task,semphr}.h`,
`esp_lcd_touch.h`, `driver/i2c_master.h`, `esp_http_server.h`,
`esp_app_desc.h`, `bsp/m5stack_tab5.h`, `esp_lcd_panel_ops.h`). FreeRTOS
tasks/mutexes/semaphores are backed by real SDL2 threading primitives
(`SDL_Thread`, `SDL_sem`) — not fakes, an actual working scheduler, just not
FreeRTOS. `xTaskCreatePinnedToCore` ignores stack size/priority/core (the
desktop OS scheduler handles it); `pdMS_TO_TICKS`/tick rate match the real
firmware's `CONFIG_FREERTOS_HZ=1000` (1 tick = 1 ms) so timing math behaves
the same.

### Tier 2 — real port implementations (`main/**/*_sdl.c`)
The modules that actually produce content, reimplemented with real (if
different) mechanics behind the exact same header contracts as the real
firmware. These live **in `main/`, as siblings of the hardware files they
mirror** (`display_sdl.c` next to `display.c`, and so on) — so a header
change and its port implementation sit in the same directory and the same
review. The ESP-IDF build never compiles them (`main/CMakeLists.txt` lists
its SRCS explicitly). What stays here in `desktop/` is the port's own
scaffolding: the entry point (`main.c`, the `app_main()` stand-in) and the
helpers `audio_sdl.c` builds on (`ring_buffer.c`, `wav_loader.c`, plus
`device_list.c` — audio/serial device enumeration, kept for the Roadmap's
device picker).
- **`display_sdl.c`** — `lv_sdl_window_create()` + `lv_sdl_mouse_create()`
  (LVGL's own SDL backend) instead of the MIPI-DSI/BSP bring-up.
  `display_lock()`/`unlock()` is a real `SDL_mutex`, same role
  `esp_lvgl_port`'s lock plays for cross-thread LVGL access.
- **`settings_sdl.c`** — all 93 functions from the real `settings.h`, backed by
  one in-memory struct **persisted to `~/.config/panadapter_sdl/settings.bin`**
  (versioned whole-struct dump; a file from a build with a different struct
  layout is discarded and defaults win). A background thread saves 2 s after
  any change — so "Don't show again", callsign, mode etc. survive relaunches.
- **`audio_sdl.c`** — replays a `.wav` file rate-exactly (absolute-clock
  pacing) into the same ring-buffer shape `audio_read_samples()` describes.
  A demodulated-audio recording (mono) is **upconverted to the QMX's 48 kHz
  I/Q at the +12 kHz IF**, so it displays at the right place relative to the
  dial; replay restarts on each 15 s slot boundary so a one-slot recording
  plays like live air. Live capture from any OS audio input is the third
  source (a real QMX's UAC interface is exactly that).
- **`dsp_sdl.c`** — real windowed FFT via `kiss_fft` (already vendored for
  `ft8_lib`) instead of esp-dsp's assembly kernels. `dsp_get_spectrum()` and
  `dsp_get_peak_dbm_around_vfo()` are real, because `render.c` calls them
  directly; zoom-FFT honestly reports its own documented "inactive" state
  (`dsp_get_zoom_spectrum()` returns `NULL`, `dsp_get_zoom_decim()` returns
  `1`) rather than faking a zoom mode.
- **`cat_sdl.c`** — real Kenwood `FA;`/`MD;` polling over a real serial port if
  you pass `--cat /dev/tty...` (a QMX's CDC-ACM interface is an ordinary
  serial port to any desktop OS — no shim needed to talk to a real radio
  plugged into this machine). Otherwise a fixed simulated frequency/mode.
- **`mem_channels.c`**, **`hid_cursor.c`** — small enough that a real
  in-memory implementation was less work than a stub.

### Tier 3 — honest stubs (`stubs/`)
Everything else `ui.c` and its screen/modal files link against but this pass
doesn't reimplement: the FT8 protocol engine (`ft8_qso.c`, `ft8_tx.c`,
`ft8_test.c`, `ft8_robot.c`, `ft8_hound.c`, `ft8_greylist.c`,
`ft8_pileup.c`), WiFi, the ADIF log, the SD archive, the diag log, CW audio,
I/Q balance, the spur map, the snap-on keyboard, BLE/USB mouse (LVGL's SDL
mouse indev replaces these), and two whole UI files replaced outright
(`reader_view.c` needs cJSON + the embedded manual blob; `qmx_term_view.c`
needs a second real serial port to the radio's menu system — both
hardware/data-file dependent enough to defer). Every stub function answers
"idle / not running / nothing to report" — the same state the real firmware
is in before its engine's first tick, not a fake positive.

## What this proves — and what it doesn't yet

**Real:** the panadapter screen's actual widget-construction code, actual
layout math, actual settings load path, actual spectrum/waterfall data path
(WAV → real FFT → the real `render.c`/`render_waterfall.c` → the real
canvases `ui.c` draws to).

**Also real: FT8 RX.** `desktop/ft8_rx_sdl.c` runs a wall-clock-aligned slot
loop through the real `dsp.h` capture contract (implemented in `dsp_sdl.c`:
−Fs/4 mix + ÷4 decimate to 12 kHz, the firmware's own math) into the REAL
`ft8_lib` decoder and the real decode list. Verified: a 15 s WebSDR capture
decodes 13 stations every slot. `--ft8` starts on the FT8 screen.

**Not yet:** FT8 TX + the QSO machine (RX decodes, the engine that answers
is stubbed), WiFi/web UI, ADIF logging, ADIF/QRZ/eQSL/LoTW upload, the
on-device manual, the QMX terminal, multi-touch gestures (LVGL's SDL mouse
indev is one pointer, not five fingers).

## Build & run

Needs SDL2 (dev package) and CMake ≥ 3.16 — no SDL2_ttf (fonts come from
LVGL itself), and no ESP-IDF: LVGL is taken from
`managed_components/lvgl__lvgl` when present (i.e. after any `idf.py` run),
otherwise fetched automatically from GitHub at the pinned v9.2.2 — so this
builds on a machine that has never seen ESP-IDF, which is also how CI builds
it (`.github/workflows/sdl-port-build.yml`).

```sh
# macOS
brew install sdl2 cmake
# Debian/Ubuntu
sudo apt install libsdl2-dev cmake build-essential

cmake -S sim -B sim/build
cmake --build sim/build -j
./sim/build/panadapter_sdl test/wav_reference/191111_110200.wav
# real serial CAT to an actual QMX plugged into this machine:
./sim/build/panadapter_sdl some.wav --cat /dev/cu.usbmodemXXXX
```

Good example inputs already in the repo: `test/wav_reference/*.wav` (real
off-air FT8 captures, 12 kHz mono). Mono gives a one-sided spectrum; a
stereo I/Q `.wav` (L=I, R=Q) gives the true two-sided display — `dsp_sdl.c`
picks complex vs. real FFT from the channel count automatically. On macOS a
QMX's CAT port appears as `/dev/cu.usbmodem*`; on Linux as `/dev/ttyACM*`.

## Menus & keyboard shortcuts (macOS)

Native menu bar (`desktop/native_menu_macos.m`; the key equivalents ARE the
keyboard shortcuts — Linux has neither yet, CLI args only):

| Menu | Item | Key | Does |
|---|---|---|---|
| *App* | Settings… | ⌘, | pick the audio input + transceiver (below) |
| File | Open WAV… | ⌘O | inject a WAV file as the audio source |
| File | CAT Log | ⌘L | live window of all in/out CAT traffic |
| View | Settings Drawer | ⌘D | toggle the right-edge drawer (edge-swipe on glass) |
| View | Panadapter / FT8 | ⌘T | toggle the base screen (left-edge swipe on glass) |
| View | Memory Channels | ⌘M | open the memory modal (bottom-edge swipe on glass) |

## The mock transceiver

With no serial port selected, CAT talks to an in-process **mock rig**
(`desktop/mock_rig.c`) that speaks real QMX CAT command strings — formats
verified against the vendor CAT manual for firmware 1_04_004 and pinned by
`test/mock_rig_harness.c` (it even reproduces the radio's MD8 quirk: `MD;`
reports the *pre*-Tune mode while tuning). The Settings dialog picks the
variant — QMX 80–20 m, QMX 20–10 m, or QMX+ 160–6 m — which changes the band
list the real band picker sees. Everything in `cat_sdl.c` flows through one
`transact()` choke point (serial *or* mock), which is why the **CAT Log**
window shows identical traffic either way, including the live FA/MD poll.

## Roadmap

Roughly in order of value for "prototype/test locally before hardware":

1. **Linux parity for the menus** — the device picker, WAV injection, CAT
   Log and view shortcuts are macOS-native (`native_menu_macos.m`); Linux
   currently gets CLI args only. An SDL keyboard hook or a small LVGL dialog
   would close the gap.
2. **FT8 TX / QSO machine** — RX decode is real now; compiling or porting
   `ft8_qso.c`/`ft8_tx.c` (against the mock rig's CAT) would let full QSOs
   be exercised on the desktop, which is most of the point of the sim.
3. **A `reader_view.c` that reads the real `docs/mkdocs/**` tree directly**
   (no embedding step needed on desktop) would make the on-device manual
   browsable here too.
