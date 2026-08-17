# Fork additions & divergence from upstream

This fork (aj7kt) tracks [SteffenLav/qmx-panadapter](https://github.com/SteffenLav/qmx-panadapter).
Upstream is a solo project that does not accept PRs; this fork does. The
policy here is **additive**: upstream files are touched as little as
possible (so pulling upstream stays a clean fast-forward or trivial merge),
and everything fork-specific lives in new files documented below. Upstream's
own docs — including `docs/architecture.md` — are deliberately left as the
author wrote them; this file is the fork's counterpart, not a replacement.

## What this fork adds

### The SDL desktop port (`sim/`)
The headline addition: the **real firmware UI running on macOS/Linux** — 24
of the 26 real `main/ui/*.c` files (all of `ui.c`), the real `render.c`/
`render_waterfall.c`, and the real ft8_lib decode pipeline, compiled against
LVGL's own SDL2 backend. Edit real UI code, rebuild in seconds, no ESP-IDF,
no flashing, no hardware. A real QMX plugged into the dev machine works
directly (its CDC-ACM/UAC interfaces are ordinary desktop serial/audio
devices); without one, an in-process **mock transceiver** speaks the real
CAT protocol (verified against the vendor manual for fw 1_04_004) and a WAV
recording replays as the radio's 48 kHz I/Q stream, slot-aligned so FT8
genuinely decodes. Native macOS menus provide device pickers, WAV
injection, a live CAT-traffic log, and keyboard shortcuts for the edge
panels. Full detail: `sim/README.md`.

### The platform boundary (`docs/porting.md`)
The contract that makes the port maintainable instead of a fork-of-a-fork:
a three-layer model (portable core / platform port / peripherals), an OSAL
defined as the ESP-IDF/FreeRTOS subset the core actually uses
(`sim/esp_shim/` is its spec), five per-module contracts implemented as
`*_sdl.c` siblings inside `main/` next to the hardware files they mirror,
and the rule that the transceiver is a *peripheral* (two byte streams: CAT
serial + I/Q audio), not part of any platform. The ESP-IDF build never
compiles `*_sdl.c` — `main/CMakeLists.txt` lists its sources explicitly.

### CI for the boundary (`.github/workflows/sdl-port-build.yml`)
Builds the SDL port on every push touching `main/**` or `sim/**` — the
drift alarm that catches a firmware change the port doesn't cover at push
time. Needs no ESP-IDF on the runner (LVGL is fetched at the pinned v9.2.2
when `managed_components/` is absent).

### Containerized firmware build (`build-docker.sh`)
`./build-docker.sh` wraps `docker|podman run --rm` on the stock
`espressif/idf:v5.4.4` image — official, multi-arch (native on Apple
Silicon), the same image CI uses, and it ships everything needed (git,
patch, the IDF env in its entrypoint), so no custom image and no host
toolchain install. Before the command (default `idf.py build`) it applies
all six standing patches and resolves `managed_components/` when missing,
so a fresh clone produces a fully-patched, flash-quality build. Any
command works: `./build-docker.sh idf.py menuconfig`,
`./build-docker.sh bash`.

### Investigation list (`docs/dual-target-todo.md`)
Improvements considered for both targets at once, each gated on measured
hardware feasibility, with a budget sheet compiled from CLAUDE.md's
measured facts. Ground rule recorded there: **the SDL port never diverges
behaviorally from the device** — it prototypes what the hardware will do.

### Small in-place changes to upstream files
Kept to the absolute minimum; the current complete list:

| File | Change | Why |
|---|---|---|
| `main/ui/ui.h` / `ui.c` | added `ui_drawer_is_open()` (one-line getter) | the port's View menu toggles the drawer |
| `docs/qmx-reference/SOURCES.md` | one entry | records the cached CAT 1_04_004 manual the mock rig implements |
| `.gitignore` | two entries (`/sim/build/`, `/stage1_diag.csv`) | port build output; ft8_lib's decode.c writes Stage-1 diag CSV in the CWD on host builds |
| `.github/workflows/close-pull-requests.yml` | **removed** (on `main`) | upstream auto-closes PRs; this fork accepts them |
| `C:devqmx-panadaptertestwav_reference/` (literal name, `:` as U+F03A) | **removed** | upstream copy accident — all 27 files verified byte-identical to `test/wav_reference/` |
| `tools/patches/apply_*.ps1` (all six) | **removed**, replaced by standard unified diffs | the edits now live in `tools/patches/*.patch` (applicable with `git apply` / `patch`, available everywhere IDF is; `apply_patches.sh` applies all six, used by firmware CI); the byte-exact `esp_hosted_sdio_drv.c.patched` store/restore file is retired, its content encoded in the diff. Patch output verified byte-identical to the original scripts' |
| `main/manual.bin` | still committed, but **CI regenerates it** for its build | fork policy: the artifact carries fresh docs; the committed blob stays for local/upstream compatibility (deleting it would conflict on every upstream docs release) |
| `test/wav_reference_full/temp_repo` | **removed** (orphan gitlink) | upstream accident: a nested git clone committed as a submodule pointer with no `.gitmodules` entry — made `actions/checkout` warn `No url found for submodule path ... exit code 128` on every CI run |

Everything else fork-side is a new file. `test/mock_rig_harness.c` follows
upstream's own harness convention (links the real code, build line in its
header).

## How updates flow

Pull upstream `main`, merge or rebase `feat/sdl-port` (or its successors)
on top. Conflicts should be near-nil by construction: the table above is
the entire in-place-change surface. If a merge ever conflicts outside those
files, something violated the additive policy — fix the policy violation,
not the merge.
