# Video engine design

For firmware developers. How GBS Reimagined takes the TV5725 from any state to a stable picture
and keeps it there. The evidence behind each step is in
[gbs-control-mechanics.md](../research/gbs-control-mechanics.md) (what the existing firmwares do,
`§n` below) and [gbs-control-lessons-videopath.md](../research/gbs-control-lessons-videopath.md)
(what their bench measured, `VP §n`). This design keeps the behaviours that are proven and changes
the structure around them.

## Goals and non-goals

- **A picture with zero configuration** from every common 15/31 kHz source on first power-up.
- **Deterministic bring-up from any prior state.** An ESP reset does not reset the TV5725, the
  Si5351 or the SDRAM (VP §1.5), so nothing may depend on power-on defaults.
- **Nothing blocks.** Every multi-step sequence is a state machine ticked from the main loop; no
  `delay()` longer than the microseconds a register latch needs.
- **Observable.** Every machine publishes its state, the reason for its last transition and its
  timings into the status snapshot; every chip write is journaled with an origin.
- **Non-goal for M1:** computed arbitrary output timings. The reference fork's computed-timing
  engine needed a two-pass apply with a cause nobody found (VP §3.1). M1 uses the fixed baseline
  programs, which thousands of boards run today; computed modes are an M2 experiment behind a
  setting.

## Building blocks

| Block | What it is | Where |
|---|---|---|
| Chip access | Segment-cached, burst-limited register access with typed fields | `firmware/src/chip/` |
| Programs | 12 output-mode register images (NTSC/PAL × 960, 1024, 720, 480/576, 1080, 15 kHz) | `schema/programs/` |
| Sections | Mode-detector windows, deinterlacer defaults, HD-bypass path | `schema/programs/section-*` |
| Tuning tables | Per-standard constants applied after a load (ADC filter, PLLAD, oversampling, sync-processor timers, capture window) | `firmware/src/video/tuning.*` |
| Machines | Detector, Loader, Settler, Watcher, FrameLock | `firmware/src/video/` |
| Signal snapshot | One decoded read of the status registers per tick | `firmware/src/video/signal.*` |

## The signal snapshot

Every watcher tick starts with one burst read of `S0_00..S0_1C` (29 bytes, two bursts) decoded into
a plain struct; every decision reads the struct, never the bus. Contents: mode-detect flags
(`STATUS_00/03/04/05`), `HPERIOD_IF`, `VPERIOD_IF`, PLL lock bits, IRQ flags (`STATUS_0F`),
sync-processor status (`STATUS_16`: HS/VS active and polarity), `STATUS_SYNC_PROC_HTOTAL/VTOTAL/HLOW_LEN`.

Decoding rules that encode measured traps:

- Interlace is the **parity of `VPERIOD_IF`** (even = interlaced), never `STATUS_IF_INP_INT`,
  which also reads 1 on 240p (VP §2.2).
- `STATUS_SYNC_PROC_VSACT` is raised only by discrete VSYNC; a sync-on-green or csync source never
  sets it (VP §2.1).
- `STATUS_SYNC_PROC_HTOTAL` counts at the loaded program's sampling rate and overflows on long
  lines; read it as the maximum of three and treat values below 100 with HS active as overflow, not
  loss (§5.2 "CRTC hold").
- PLL lock bits are reported as counts out of the last 8 reads, never as booleans (VP §2.4).

## Input standards

| Code | Standard | Typical source | Output path |
|---|---|---|---|
| 1 | NTSC 15 kHz (240p / 480i) | SNES, PS1, MD, N64 | scaled |
| 2 | PAL 15 kHz (288p / 576i) | European consoles | scaled |
| 3 | EDTV 480p | PS2, GC, Wii, DC (VGA) | scaled |
| 4 | EDTV 576p | | scaled |
| 5 / 6 / 7 | HD 720p / 1080i / 1080p | 360, PS3 component | HD bypass |
| 8 | 25 kHz medium resolution | arcade, X68000 | scaled |
| 9 | stable but unrecognised | computers | scaled (480p program) |
| 13 | PC graphic mode on YPbPr | | HD bypass |
| 14 / 15 | RGBHV scaled / bypass | VGA sources | scaled / RGBHV bypass |

240p and 480i are one standard: switching between them never reloads a program; only the
deinterlacer engages (VP §2.2, §5.5).

## Machines

```text
            ┌──────────┐ source found ┌────────┐ load done ┌─────────┐ clamp set ┌─────────┐
 boot ─────►│ Detector │─────────────►│ Loader │──────────►│ Settler │──────────►│ Watcher │◄─┐
            └──────────┘              └────────┘           └─────────┘           └────┬────┘  │
                 ▲                         ▲                                          │       │
                 │      sync lost (long)   │          format change / user request    │       │
                 └─────────────────────────┴──────────────────────────────────────────┘       │
                                                                FrameLock (when stable) ──────┘
```

All machines share one rule: a machine that changes the video path holds the **video busy**
predicate; the command layer answers "busy" (HTTP 409) to anything that would interleave.

### Boot

1. Recover the I2C bus, probe the TV5725 (scratch read-back on `ADC_UNUSED_69`), probe the Si5351.
2. Hold every block in reset (`RESET_CONTROL_0x46/47` = 0), power the PLLAD down.
3. Clear all six segments (16-byte bursts) — the deterministic starting point.
4. Write the **idle baseline** (§2.3): input-formatter minimum timing, outputs off (DAC powered down,
   no sync out), ADC on for detection with SOG enabled, display PLL off, clock pads, the HD-bypass
   and mode-detect sections, default ADC gain/offset, sync-processor defaults, PLL resets, then
   release only the blocks detection needs (IF, VDS, DEC, MODE, SYNC, INT).
5. Write the **sync-processor baseline** (§3.6) — a table of 28 segment-5 values.
6. ADC black-level calibration on the unconnected input (§4.2), as a machine of ~12 ms slices
   (about two seconds; state `Starting`), then the sync-processor and idle baselines again. The
   offsets are kept for every later program (see [Input levels](#input-levels)).
7. Si5351: crystal load, outputs off, CLK0 parked at 81 MHz and disabled (§1.3).

### Detector

Runs every 500 ms while no source is locked; one state per wait, deadlines instead of sleeps
(§3.2–3.4).

| State | Does | Exit |
|---|---|---|
| `Scan` | Check HS stable on the current input for up to 450 ms | HS → `ProbeRgb` or `ProbeComponent`; none → flip input, `Scan` after 200 ms |
| `ProbeRgb` | VSYNC test (≤ 360 ms), then discrete-VSYNC probe (median of 9 VTOTAL reads) if the flag never rises | VS → `ProbeHv`; none → `WalkSog` (csync/RGBS) |
| `ProbeHv` | HS within 400 ms; csync-on-HS test (field rate measured 3×); 25 kHz probe (16 steps of `MD_HD1250P_CNTRL`) | csync → `Found(RGBS)`; 25 kHz → `Found(MedRes)`; else → `Found(RGBHV)` |
| `WalkSog` / `WalkComponent` | Poll the mode every 2 ms for ≤ 6 s, stepping the SOG slicer every 150/180 polls | mode → `Found`; timeout → `Found` anyway, the Settler copes |

Choosing csync gates the H/V inputs off, which hides a source's own vertical forever; the Watcher
therefore re-runs the discrete-VSYNC probe periodically while classified as csync (§3.4, §5.2).

### Loader

Writes one program and runs the post-load tuning. Input: standard + output preference.

1. **Select the program** (§6.1): NTSC programs for standards 1/3/8/9/14, PAL for 2/4; HD bypass for
   5/6/7/13; RGBHV bypass for 15. "Match source" maps NTSC+1024 → 960 and PAL+960 → 1024.
2. **Blank the output** first: DAC off, sync out off (VP §0 item 9: the HDMI converter must re-lock
   on a format change, so it is dropped deliberately rather than fed garbage).
3. **Write the program** (§2.4) with the load-time patches applied as the bytes stream out —
   keep `S0_46/47` as they were, clear `IF_VS_SEL`/set `IF_VS_FLIP`, clear `IF_INI_ST`, input
   select and clamp mode in `S5_02/03` from the detected input, `SP_SOG_P_ATO` only in `S5_20`,
   `SP_H_PULSE_IGNOR` by SD/HD, sub-coast disabled, `SP_NO_CLAMP_REG` = 1 — then the mode-detect
   and deinterlacer sections, and zero `S3_80..8F`.
4. **Post-load tuning** (§4.1 steps 4–30) driven by the tuning table of the standard, in the proven
   order: clock pad on → sync-processor baseline → clamp held off → per-input colour patches (RGB vs
   YPbPr) → phase and SOG level → per-standard IF/ADC/PLLAD tuning → SNES line-count shift → mirror
   EXT blanking and frame registers → `resetDigital` + `resetPLLAD` → fixed memory/FIFO settings
   → `ResetSDRAM` → re-latch → sync-processor follow-up.
5. Map the program's internal clock code to the Si5351 frequency (`648 MHz / n`) and arm adoption.

The order is not cosmetic: the reference fork lost weeks to a post-load that ran *after* a timing
change and overwrote it (VP §1.6). The geometry is therefore computed after the post-load, from the
state it leaves ([Geometry](#geometry)).

### Settler

The non-blocking half of the post-load (§4.3):

| State | Deadline | Does |
|---|---|---|
| `WaitSync` | 2002 ms | HS stable; dynamic sync-processor update every 4 ms |
| `WaitMode` | 1505 ms | a recognised mode |
| `SogSettle` | 300 ms | optional SOG optimisation when the mode came late and SOG ≥ 7 |
| `Clamp` | 25 tries × 20 ms | measure the clamp window from 16 agreeing line lengths |
| `Finish` | — | clamp locked (`SP_NO_CLAMP_REG` = 0), **then** DAC on; IRQs; unfreeze capture |

The DAC stays off until the clamp locks: before that, black drifts upward and the picture fades in
grey (§4.1 step 1, VP §2.7).

### Watcher

Ticks every 20 ms once a program is loaded (§5). Responsibilities, in pass order:

1. **Debounced sync loss:** three bad reads in a row before reacting (dark content makes SOG
   flicker; a single bad read used to trigger phase re-runs — VP §2.1).
2. **Sync-loss schedule** (§5.3): freeze at 1, SOG nudge at 2, coast defaults at 8, dynamic SP
   update every 27, nudge the mode detector at 38, **fallback program** once at 200, SOG walk and
   input flip at 150/413, blank output after two dead probes, back to the Detector at 2046.
3. **Format change** (§5.4): 3 confirming reads freeze, 8 more confirm with 30 reads, then the
   Loader runs with the new standard.
4. **Stable branch** (§5.4): unfreeze at 1, SP update at 2, phase optimisation 10–60, clamp refresh
   at 45, coast at ≥ 7, clamp at ≥ 4.
5. **Done stage** at stable 36–44: DAC and sync out on; if the Si5351 is present, adopt it
   (disable CLK0, set frequency, enable, wait 800 µs, `PLL648_CONTROL_01` = `0x75`) and establish the
   source/output rate ratio.
6. **Display-clock watchdog:** if the chip has sat on an internal clock for 10 s with the Si5351
   present, adopt again and re-present the output (VP §3.6 — a missed adoption free-runs the output
   17 % fast and nothing on screen shows it).
7. **Interlace follow** (§5.5): `VPERIOD_IF` parity, 2 agreeing reads (8 for csync), toggles the
   motion-adaptive deinterlacer; scanlines only on progressive content.
8. **RGBHV supervision** (§5.6) and the 3 s IRQ housekeeping (§5.7).
9. **Input levels:** a running "calibrate levels" action, or else auto gain when it is on (see
   [Input levels](#input-levels)).

### FrameLock

With the Si5351 adopted, the lock is a **ratio**: set the clock to `clock × sourceRate / outputRate`,
measured on the vsync probe pin, refusing any result outside 20–162 MHz. Measured holding error on
the reference bench: 0.07–0.09 ppm, one frame of slip every 49–70 hours (VP §3.7). The continuous
phase loop of older firmware hunted and froze the picture and is not implemented.

Without the Si5351 (not this board, kept for the HAL's sake): auto best-HTotal plus the
two-line VTOTAL phase controller (§7.2–7.3).

Clock changes under 750 kHz are walked in 1 kHz steps; larger ones are written at once (§1.3). The
pixel clock is clamped to the DAC's 162 MHz in every path (VP §3.6: an unclamped lock once drove it
past 237 MHz).

## Input levels

The ADC's black level (offset) and range (gain, where a higher register value is *less* gain) are
set per channel. All three mechanisms read the ADC through the decimator's test output; how that
output behaves (flag bit, which selector is which channel, why blue needs the matrix bypassed, the
measured gain response) is recorded in [docs/bench/M1.md](../bench/M1.md#reading-the-adc-levels).

| Mechanism | When | What it does |
|---|---|---|
| Black-level calibration | every bring-up, before detection | raises each offset from 0x3D until the channel reads near zero on the unconnected input; falls back to 0x40 if any channel reaches 0x52 |
| Calibrate levels (action) | on request, with a white screen | per channel, bisects the gain register for the most gain that keeps white at or under level 0x7A (0x7C left noise touching the ceiling on the bench), then checks that the field really was white (≥ 35 % of reads near the top); refuses a picture without a large white area or one that clips even at the least gain; component sources calibrate Y and give Pb/Pr the same gain |
| Auto gain (`input.autoGain`, off by default) | stable > 90, clamp set, line PLL locked, SOG clean | samples green and red (61 reads per tick for 30 s after a start or change, 8 after), and on two ceiling reads in a window of twenty raises all three gain registers by 2; only ever lowers the gain |

A new source starts from its input's **base gain**: the last successful calibration for that input,
or the preset 0x7B. Auto gain starts again from there for each source. While the action runs, auto
gain is paused; a source loss, a format change or a reload aborts it and restores the previous
gains. The calibrated gains live in RAM until the settings store persists them (M1).

## Geometry

`output.aspect`, `hPosition`, `vPosition`, `hSize` and `vSize`, in `video/geometry.cpp`. One
owner, one rule: right after the post-load the engine reads the program's own geometry (scales,
display and fetch windows, capture starts) as a **baseline**, and every later write is computed
from that baseline and the settings, never from the chip's current values. A value derived from its
own previous output drifts; on the reference bench a scale that walked down about two counts per
apply tore the picture (VP §3.5).

Owned after a load: `VDS_HSCALE`/`VSCALE` and their bypass bits, `VDS_DIS_HB_*`, `VDS_HB_*`,
`VDS_EXT_HB_*`, `VDS_DIS_VB_*`, `VDS_VB_*`, `VDS_EXT_VB_*`, `IF_HB_SP2` and `IF_VB_ST/SP`. The
frame lock's line change (`applyBestHTotal`) goes through the same computation with the new line
length, and writes the line and the horizontal fields together at a field start. A bus error while
reading the baseline voids it; the engine reads it again at the next geometry change, and until
then the frame lock changes only the line.

**Aspect.** A picture's shape is defined against BT.601: a 4:3 picture is 704 samples at 13.5 MHz
(52.148 µs) wide over the source's full height (480 or 576 lines after the line doubler). The HDMI
converter shows a fixed share of each output line, the active share of the standard timing it
recognises. A per-program table holds that share, the frame's lines and its shape: standard
VESA/CEA timings that the bench captures confirm, and for PAL 576p a share of 0.781 measured on the
bench ([M2](../bench/M2.md#2026-10-14--what-the-converter-shows)). From these the code computes
the scale divisors for the wanted shape:

| Aspect | 16:9 frame (1080p, 720p) | 4:3 frame (960p, 480p, 576p) | 5:4 frame (1024p) |
|---|---|---|---|
| 4:3 | pillarboxed | fills the frame | letterboxed to 960 lines |
| 16:9 | fills the width | letterboxed to ¾ of the height (user decision 2026-10-14) | letterboxed |
| Full | fills the frame | fills the frame | fills the frame |

Sources whose shape is not known (medium resolution, RGBHV, unknown modes) take the program's own
capture as their 4:3 picture. The 15 kHz programs bypass the converter and take position only.

**Windows.** On both axes the display window shows what the program captured at the new scale,
centred where the program centred it (horizontally capped at the converter's visible width, or the
program's own window when that is wider; vertically at the program's window, which already spans
the frame). The fetch blanking (`VDS_HB_*`, `VDS_VB_*`) moves with the display window and keeps the
program's own distances from it: the window's first line and column show the capture start. Offsets
that are not the array's own wrap or tear the picture (VP §3.5), and on this bench moving the fetch
blanking with the window letterboxed cleanly ([M2](../bench/M2.md#2026-10-14--geometry-on-the-board)).
At a divisor other than the program's, the fetch also ends early enough to stay within 1008 samples
per line (`PB_FETCH_NUM` is 256, i.e. 1024): beyond about 1016 a squashed copy of the picture showed
at the window's far edge. Ending the fetch earlier leaves the content where it is.
A picture larger than the window is cropped around its centre: the capture starts later by half the
span that no longer fits. The capture start never moves earlier than the program's: earlier
horizontally shows the decoder's green from before the source's active line, and earlier vertically
wraps into the previous frame, past what the frame memory holds.

**Position** moves the whole window (display, fetch and mask) against the sync: two output units
per step horizontally, one source line per step vertically. The window stays clear of the sync
and inside the frame; where it would cross either, the picture is cut there (vertically by starting
the capture later, never by wrapping). **Size** multiplies the picture by 0.5 % per step on each
axis.

**Limits.**

- The scaler only enlarges horizontally; at 1080p the programs are already 1:1, so horizontal size
  below 0 does nothing there, and PAL at 1080p stays 10 % wider than 4:3 (measured,
  [M2](../bench/M2.md#2026-10-14--geometry-with-a-pal-source)).
- Horizontal size stops at 2× (divisor 512), or at the shape's own divisor where that is lower
  (480p's 4:3 needs 478, measured clean): at 1280x960, divisors under 512 displaced lines
  ([M2](../bench/M2.md#2026-10-14--geometry-on-the-board)).
- 480p and 576p already show the source 1:1 vertically, so they cannot letterbox: there, 16:9
  equals 4:3, and vertical size below 0 does nothing.

## Pass-through (experimental)

Written from the community firmware's sequences (mechanics §6.5, §5.6) and **not yet verified on
this bench**, which has no HD or PC source; the user chose to have it anyway, marked experimental
(2026-10-13). Code in `video/bypass.cpp`.

- **HD bypass.** HD sources (720p, 1080i, 1080p, PC over component) always take it, since no
  program scales them; any other source takes it when the output is set to Passthrough. The ADC
  samples go through the HD block to the DAC (`DAC_RGBS_BYPS2DAC`), YPbPr is converted to RGB
  there, and the scaler, memory and display PLL idle (`PLL648_CONTROL_01` = 0x35). Per-standard
  blanking, sync and sampling values come from the reference. It settles through the normal
  SettleSync / SettleMode / SettleClamp phases; the watcher's usual loss and format-change
  handling applies; frame lock and the clock generator stay out of it.
- **RGBHV bypass.** A source with separate H/V sync that matches no TV mode after the
  medium-resolution walk is taken for a computer: the ADC feeds the DAC directly
  (`DAC_RGBS_ADC2DAC`). A reduced supervisor checks sync stability (300 bad passes for HV, 200
  for composite, then back to detection) and retunes the H-PLL charge-pump band every 900 ms from
  the measured H-PLL rate (bands below 1030 / 2300 / 3200 / 3800 and above).
- **Not done:** scaling RGBHV sources (the reference's mode 14), following the HV sync polarity,
  and the HD-bypass SD line tracking.

Bench so far ([M1](../bench/M1.md#2026-10-13--pass-through)): with the 240p RGBS source and
Passthrough, the board settles in bypass and holds sync; HDMI showed no picture.

## Output modes offered

| Mode | Programs | Notes |
|---|---|---|
| 1080p | NTSC/PAL 1920x1080 | line-sampled 2558, horizontal scaler bypassed |
| 1280x1024 | NTSC/PAL 1024 | |
| 1280x960 | NTSC 960 | exact ×4 of 240p, ×2 of 480i (VP §3.4) — the default for NTSC |
| 720p | NTSC/PAL 720 | |
| 480p / 576p | NTSC 480 / PAL 576 | line doubling only |
| 15 kHz | NTSC/PAL downscale | for CRTs, from 31 kHz sources |
| Passthrough *(experimental)* | HD or RGBHV bypass | HD and PC sources always; any source by choice |

## Safety rules

- Any operation that can blank the output and was started remotely arms a timed **revert**
  (default 20 s) that restores the previous program and settings unless confirmed. Local OLED
  actions confirm themselves.
- Pixel clock is always within 20–162 MHz.
- Writes to read-only fields do not compile; writes outside known fields go through the console's
  expert command only.

## Status published per tick

`engine.state`, `engine.reason`, standard, sync type (HV / csync / SOG), input, interlaced, source
H and V frequency, lines per field, program, output mode, pixel clock (only when the Si5351 drives
the display, otherwise flagged as internal), frame-lock state and error, PLL lock counts, SOG level,
ADC gain/offset, clamp/coast positions, stable counter, time in state.

## Testing

- **Host:** the fake TV5725 from `firmware/test/support` gains scripted status sequences so each
  machine is driven through its states; tests assert termination within deadlines, the order of
  writes for a load, and that nothing writes the bus outside the loop.
- **Bench:** every milestone gate runs the capture-card verification against the 240p Test Suite
  bars for 240p, 480i, 480p and PAL 576i sources, and records the input signature with each result
  (see the bench-verify skill).
