# Dual-target improvements — investigation list

Improvements considered for BOTH targets (Tab5 firmware + SDL port) at once.
Distinct from the upstream `TODO.md` (the author's master list): every item
here is bound by two ground rules.

1. **The SDL port never diverges behaviorally.** It exists to prototype what
   the device will do; a sim-only capability teaches the wrong lessons and
   produces designs the hardware can't honor. (Precedent: the "background
   FT8 decode in the sim only" idea was considered and rejected 2026-08-17.)
2. **Hardware feasibility is measured before anything is designed.** This
   codebase's history is unambiguous: every "obvious" resource assumption
   has been wrong when measured (see CLAUDE.md's `MALLOC_CAP_DMA`, cyan-flash
   and SD-wedge sagas). The budget sheet below is the starting point, with
   its unknowns stated; each item lists the measurements that would settle
   it, using instrumentation the firmware already has.

## Hardware budget sheet (what we know / don't)

Everything below is from CLAUDE.md's measured notes — cite it, don't re-derive.

| Resource | Known state | Confidence |
|---|---|---|
| Core 0 idle, FT8 mode | ~70–80% (after the v0.19.3 render gate) | measured |
| Core 0 idle, panadapter mode | **0–5% — an UNEXPLAINED regression** (historically ~14–35%; v1.8.3 notes it, A/B ruled out the zoom FIR) | measured but not understood |
| Core 1, FT8 decode | STFT+decode ~1–2.3 s per 15 s slot (FT4: ~2.3 s in 7.5 s — tight) | measured |
| Display | ~13 fps landscape (software 90° rotation ≈ 50% cost); canvas invalidations drag the whole flush pipeline — the reason FT8 mode stops painting the spectrum at all | measured |
| USB ISO audio | 320 ms of queued transfers; any stall of the completion pump longer than that = silent audio loss with zero error status (the #51 class). Decode storms are the known stall source | measured, root-caused |
| Internal heap, FT8 mode | ~41 KB free / 13 KB largest block — "stable but tight" | measured |
| `MALLOC_CAP_DMA` | ~40 KB with WiFi up (post-`.bss` fix) | measured |
| PSRAM | ~28 MB free — effectively unconstrained | measured |
| Audio ring | ONE consumer by design (`ui_mode` switches fft_task's role); `dsp.c` already keeps a continuous FT8 pre-ring while in FT8 mode | design fact |

Available instrumentation: `cpu_stats` per-core idle% (O(1), safe),
`RX xport` 1 Hz ISO stats from the UAC fork, `FT8 arm: start=` window-tiling
deltas (~180000 or something is stealing audio), per-slot `slotdiag`, and
the dev-only `resmon`. The SDL port measures nothing useful about hardware —
its role is proving the *design and UX*, never the budget.

## Item template

```
### I-n: <name>
Want: <the user-visible improvement, one sentence>
Known: <budget-sheet facts that bear on it>
Measure first: <specific numbers, from which instrumentation, that gate the design>
Design sketch: <only after the numbers say yes>
Sim role: <what the port builds/proves, identically gated>
```

## Items

### I-1: FT8 RX while the panadapter view is active
**Want:** switch Panadapter ↔ FT8 without the decode list going stale —
decoding continues in the background, spectrum/waterfall stay live.

**Known:** the blocker is CPU, not architecture. The single-consumer ring is
not the obstacle it looks like — `fft_task` is the sole consumer in both
modes and already feeds a continuous FT8 pre-ring in FT8 mode; feeding
*both* the spectrum FFT and the pre-ring is an extension of its existing
role, not a redesign. The real questions: (a) panadapter mode currently has
**no core-0 headroom at all** (0–5% idle, unexplained), and (b) the decode
burst + LVGL decode-list rebuild is exactly the storm that caused the #51
USB audio loss — adding the full render pipeline on top risks re-opening it.
Memory looks fine (monitors/waterfalls live in PSRAM; internal cost of the
FT8 monitor pool is already paid in FT8 mode).

**Measure first (in order):**
1. Root-cause the core-0 0–5% idle in panadapter mode. Nothing can be
   decided while that number is both terrible and unexplained. (`cpu_stats`,
   A/B against older builds — v1.8.3 already exonerated the zoom FIR.)
2. With that fixed/understood: prototype the combined load on hardware
   behind a dev flag (capture+decode running, panadapter view up) and watch
   `FT8 arm: start=` tiling + `RX xport` for a full session — the #51
   signature is invisible to every software counter except window tiling.
3. Decode-time delta when core 0's helper (`ft8_dec0`) is competing with
   the render/rotation pipeline (per-slot `slotdiag`).

**Design sketch (contingent):** `fft_task` feeds spectrum + pre-ring in both
modes; decode tasks run regardless of view; the FT8 *render* gate stays
exactly as is (the FT8 screen's canvases still don't paint when hidden —
this changes what computes, not what draws). Possible fallback if the
budget says no: decode-while-panadapter only when zoom = ×1 (zoom FFT is
the marginal core-1 cost), or decode every Nth slot.

**Sim role:** implement behind the same flag with the same gating, tee'd
ring under the same "fft_task owns both" shape — so the UX (list stays warm,
what the status line shows) is proven in the sim while the budget is proven
on the bench.

### I-2: Decode-list retention across view switches
**Want:** a quick trip to the panadapter and back shouldn't empty the list.

**Known:** rows expire after `FT8_ROW_STALE_SEC` (90 s) — a deliberate
design ("a live picture of who's on frequency *now*, not a history log"),
so this is policy, not capability; zero budget impact on either target.
Note: if I-1 lands, this mostly dissolves (the list stays fresh because
decode never stopped) — which argues for deciding I-1 first.

**Measure first:** nothing — but check the operator intent recorded in
`ft8_screen.c`'s comments before changing the policy.

**Design sketch:** options, cheapest first: (a) pause aging while the FT8
view is hidden (rows freeze instead of dying — precedent: the TX-parity
aging pause already does exactly this during CQ runs); (b) longer window;
(c) a separate scrollback/history view, which is a new feature, not a knob.

**Sim role:** identical change, and the natural place to try (a) vs (c) on
a screen before touching firmware.

### I-3: Global decoder enable + decoder selection in the title bar
**Want:** one always-visible control that says whether the digital decoder
is running and WHICH decoder (FT8 or FT4 today; the selection is meant to
grow — WSPR/JS8/CW-decode candidates later), and toggles it — instead of
the state living implicitly in "which screen am I on".

**Naming hazard, decided up front:** this is the DECODER selection
(`ft8_op_mode` today), NOT the transceiver's operating mode — and the top
bar already has a "Mode" control that means the radio mode (USB/CW/DiGi via
CAT `MD`). Two adjacent title-bar controls both labeled "mode" would be a
standing confusion; the new control needs a distinct name ("Decoder",
"Digi") in every surface — UI label, settings key, web API — from day one.

**Known:** negligible budget (UI + a state flag). The semantics depend on
I-1: "globally enabled" only means something once the decoder can run while
the panadapter is visible — until then this control is just a fancier way
to switch screens. The top bar is 60 px and already carries freq | mode |
s-meter | burger; adding a tappable element there walks straight into two
documented traps: the LVGL reverse-creation-order hit-testing saga (v1.5.0
— top-bar hit zones are screen children, foregrounded on a keepalive, and
have swallowed overlay touches before) and the pointer-colour promise
(v1.8.0 — a new control must be hot, or deliberately `UI_FLAG_NOT_HOT`).
The top-right 200×120 px of the spectrum is also a deliberate tap deadzone.

**Measure first:** nothing (CPU-trivial) — but sequence after I-1's
verdict, since the control's meaning depends on it.

**Design sketch:** decoder name + running/stopped in one pill, in the top
bar's existing hit-zone framework, opted into the pointer contract.
Switching decoders moves a real state that re-grids the slot clock
(`ft8_op_mode_set` already owns clearing the decode list on protocol
change — reuse it, don't fork it). For future decoders: the enum and the
slot-period lookup (`ft8_op_mode_slot_ms`) are the extension points —
"add a decoder" should mean adding an entry there plus its engine, never a
second selection mechanism. A decoder with no slot grid (WSPR's 2 min, a
continuous CW decoder) will stress the 15 s assumptions baked into the
slot bar/countdown UI — worth a note in that design when it comes.

**Sim role:** the whole design iteration — placement, size, what the states
look like — is pure UI and transfers 1:1. Prototype here first.

### I-4: Combined view — decodes beside a live waterfall (3 kHz above dial)
**Want:** the FT8 screen shows the decode list AND a waterfall of the
0–3 kHz audio window above the tuned frequency, WSJT-X wide-graph style.

**Known — the big cost-saver:** the decoder ALREADY computes this exact
waterfall. `monitor_process()` builds an STFT magnitude array covering
200–3000 Hz at 6.25/freq_osr Hz resolution, one row per 80 ms subblock,
streamed during capture — that is precisely the display being asked for.
So the DSP cost of this view is ZERO on both targets; the cost is only
*painting*: a canvas in FT8 mode, which the v0.19.3 render gate currently
forbids because canvas invalidations drag the whole flush + 90°-rotation
pipeline on core 0. But that gate was about the full-screen panadapter
canvases repainting invisibly at 10 Hz — a deliberately smaller waterfall
(say ~1280×200, one row per ~80 ms, written with the same double-height
scroll trick as `render_waterfall.c`) invalidates proportionally less.
Rotation cost scales with flushed pixels.

**Measure first:** core-0 idle + `FT8 arm: start=` window tiling with a
prototype canvas of the target size repainting at row rate during real
FT8 capture on hardware — i.e. does the painting alone re-open the #51
stall class. (This item does NOT depend on I-1: decode already runs on
this screen. It's the painting budget, not the decode budget.)

**Design sketch:** render `mon->wf` rows into a canvas as they stream
(capture task already touches each row; hand it to a paint step the same
way `dsp_ft8_capture_progress()` exposes progress). Frequency axis is
audio Hz 0–3 k above dial. I-5's markers land on this view for free.

**Sim role:** build the view here first against the port's real monitor —
layout, palette, row rate, list-beside-waterfall arrangement all transfer
1:1; only the painting budget needs the bench.

### I-5: TX/RX frequency indicator lines on the waterfall
**Want:** vertical markers on the waterfall for our TX tone and the
RX/partner frequency.

**Known:** cheap, and the mechanics already exist — the panadapter
waterfall has a trail-free overlay cursor (`s_wf_cursor`, driven from
`ui_push_spectrum`) and v1.8.2 already prints the RIT offset onto the
waterfall. TX tone is `ft8_tx`'s state (the tone modal/occupancy strip
read it); partner freq is `ft8_qso_get_priority_freq()`. On the I-4 view
the mapping is trivial (tone Hz → x). On the panadapter waterfall it's
dial + tone through the existing freq→x math.

**Measure first:** nothing — overlay objects, not canvas repaints.

**Design sketch:** two overlay lines (TX = the ARMED/ACTIVE amber/red the
status line already uses; RX/partner = a second colour), shown only when
the underlying state exists — a marker for a tone that isn't armed is a
lie. Follow the pointer-contract rules if they're made tappable.

**Sim role:** identical code both targets; prototype colours/visibility
rules here.

### I-6: Monospaced font for the decode list
**Want:** decode rows in a monospaced font so message text and columns
align by character.

**Known:** the firmware ALREADY EMBEDS a monospace font —
`font_qmx_mono_25.c` (JetBrains Mono, size 25, shipped for the QMX
terminal screen) — and the SDL port compiles it too (it's in the ui/
glob). So a zero-flash-cost experiment exists at size 25; if 25 is the
wrong size for rows, a new LVGL-converted size costs flash (the app
partition has headroom — the whole manual is 136 KB of 8 MB) and a
`lv_conf.h` entry in the sim. The row pool (40 rows, fixed geometry,
column x-positions tuned in v1.3.1) is laid out for montserrat metrics —
row height and column alignment must be re-checked, which is exactly the
reflow class CLAUDE.md warns about (section height and `y +=` move
together).

**Measure first:** nothing (rendering cost identical) — flash size of any
new font size is the only number, and it's known at build time.

**Design sketch:** try `font_qmx_mono_25` on the rows in the sim first;
if too large, generate JetBrains Mono at the right size. Columns can then
align by character count instead of hand-tuned pixel offsets — which
would retire a small class of alignment bugs.

**Sim role:** the entire visual iteration. This is the single most
sim-friendly item on the list — font, spacing, and column layout transfer
exactly, and a screenshot comparison decides it.
