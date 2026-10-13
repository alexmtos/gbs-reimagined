# Reference lessons: the TV5725 video path, as measured by the gbs-control fork

> **Provenance.** Knowledge extracted on 2026-10-13 from the documentation of the gbs-control fork
> this project builds its knowledge on: a local branch of
> [JuergenLeber/gbs-control](https://github.com/JuergenLeber/gbs-control) with extensive bench notes
> (docs dated up to 2026-09-24), itself descended from
> [ramapcsx2/gbs-control](https://github.com/ramapcsx2/gbs-control). Citations point into that
> fork's `docs/` and source files. Every measurement here is *theirs*: treat it as a hypothesis for
> this project's bench until reproduced (see [the research index](README.md)).


Read-only extraction (2026-10-13) of the bench knowledge in the existing project's `docs/`, for
**GBS Reimagined**: a from-scratch firmware for the same board. That board has a Tvia TV5725 with
SDRAM, a Si5351 on the output pixel clock, an ESP8266, an MS9288A VGA-to-HDMI converter and an
LM1881 sync separator. This file records facts, limits, mechanisms and failure modes, not code.

## How to read this

Source tags. All files are under the reference fork's `docs/` directory.

| tag | file | coverage |
|---|---|---|
| T&B | `timing-and-blanking.md` | read in full |
| RTP | `retrotink-parity.md` | read in full |
| SEN | `sensors.md` | read in full |
| BEH | `behaviour-controls.md` | read in full |
| CAL | `calibration-defaults.md` | read in full |
| REG | `register-map.md` | structure only: the header, group headings and a few field loci |
| DSR | `datasheets/README.md` | supplementary, read in full. RTP and SEN cite it for the MS9288A facts. |
| FWD | `firmware-design.md` | supplementary, selected sections only: sync detection, the post-load, the watchdog, the frame lock and the phase measurement |
| FP | `first-picture.md` | supplementary |

Citations take the form `[T&B § "heading"]`. Headings are quoted verbatim and sometimes shortened
with "…".

**Caveats that apply to everything below**

- **Their bench chain.** GBS board (the RetroScaler "GBSC" variant of the GBS-8200) → VGA →
  on-board MS9288A → HDMI → splitter → NZXT Signal HD60 capturing at 1920x1080
  [RTP § "The standing rules"; CAL § "The bench"; DSR § "Vendor documentation for the board"].
  When they write "black" or "no picture", they mean the *capture card* saw none. The converter's
  state cannot be read, and the HD60 has its own limits [RTP § "A third black screen, and the
  pattern is now worth naming"].
- **Their sources.** PlayStation 1 (240p), PlayStation 2 (480i NTSC, and PAL 576i on 2026-09-06
  and 2026-09-24) and Mega Drive (240p/224p/480i), mostly running the 240p Test Suite. The bench
  never had an HD source, an RGBHV/PC source or a 288p source [FWD § "What is left of the waits,
  measured 2026-09-15"; T&B § "2026-09-24 — the first PAL source this bench has had…"].
- **T&B is not chronological.** Many of its sections carry "superseded" or "withdrawn" banners. This
  digest reports the **latest surviving state** and marks what was superseded. T&B itself
  catalogues 8+ measurement errors (Appendix A), so a single-run result in their docs is an
  anecdote by their own rule.
- Where this digest adds an inference of its own, it is marked **(inference, not in docs)**.

---

## 0. Twelve things to carry over (summary)

1. **The output pixel clock has to beat the input pixel clock by about 1.13–1.16x**, or the output
   scans with no data or the sink will not lock. The fix is to widen horizontal blanking, which
   raises the clock without touching the active area or the rate
   [T&B § "The rule: the output clock has to beat the input's"; § "Widen the blanking instead of
   only warning"].
2. **The Si5351 drives the display only while `PLL648_CONTROL_01` (s0_41) = 0x75.** Every other
   code is a fixed internal rate; 0xA5 is 162 MHz, which gives a 70.1 Hz free-run on a 2080x1111
   raster. Any output rate derived from the Si5351's *programmed* frequency is then false
   [T&B § "2026-09-04 — the free-run happens outside the bench…"; SEN § "Source rates, derived"].
3. **The four FIFO line flags are hand-tuned and cannot be derived.** For 1080p-class rasters they
   are `PB_MAST_FLAG_REG` 45, `PB_GENERAL_FLAG_REG` 0, `RFF_MASTER_FLAG` 36 and `RFF_GENERAL_FLAG`
   52. They must be written *after* every video-path reset. Without them, lines are displaced
   horizontally and snap back [T&B § "2026-08-29 — the FIFO line flags are the cause of per-line
   jitter…"; FWD § "Every apply must end by writing the FIFO line flags"].
4. **`VDS_HSYNC_RST` and `VDS_VSYNC_RST` hold total−1.** Status counters such as
   `STATUS_SYNC_PROC_HTOTAL` hold the plain count [SEN § "The totals are total-minus-one, and
   that was a real bug"].
5. **The scale registers are divisors:** `VDS_xSCALE = 1024 × input / output`. An `hscale` that
   was one count off through integer truncation tore the picture. Round, and never re-derive a
   value from your own previous output [T&B § "2026-09-02 — the tearing was integer truncation,
   and one count of hscale is enough"].
6. **The memory-fetch blanking `VDS_HB_SP/ST` must match what the input formatter captured.**
   Offsets that are 4–80 units wrong give wrapped content, a band at the left, or tearing on every
   line. Take the distances from a known-good array rather than from constants
   [T&B § "2026-08-30 — the left artefacts were the horizontal blanking start"; § "2026-09-19, on
   the board — the picture came back wrapped…"; BEH § "Trims…"].
7. **A clean picture needs two things:** an integer vertical ratio (within about 0.5%), and an
   output rate equal to the source rate (within about 0.001 Hz). The frame lock that sets the
   Si5351 frequency from the measured rate ratio holds 0.07–0.09 ppm. The old phase loop on top of
   it hunted and froze the picture
   [T&B § "2026-08-29, later — 1920x1080 reverted…"; RTP § "Locked, which is the default — 0.07
   to 0.09 ppm"; FWD § "The frame time lock hunts…"].
8. **The working pixel-clock window on their chain was about 104.5 MHz to 162 MHz.** The bottom is
   a sink floor specific to their converter and capture card; the top is the TV5725 DAC rating.
   Vertical totals up to 1600 held at 138 MHz, so the clock is the real ceiling
   [T&B § "It is the pixel clock, and nothing else"; § "2026-09-14 — the series above changed two
   things at once…"].
9. **The MS9288A is a second digitiser with its own MCU, and it is not on I2C.** Its lock loss
   looks like "chip healthy, output black". The recovery is to power the DAC down, disable the
   sync-out pad, then bring both back (re-present), not to reset the chip
   [RTP § "The trigger, and a recovery that is not a restart"; DSR § "What the MS9288A datasheet
   does and does not settle"].
10. **Most of the RetroTINK-style features dissolved when measured:**
    - noise reduction wrecks the picture;
    - the V-IIR filter belongs to the vertical downscaler;
    - Field Flip and the interlaced-output registers are inert;
    - pseudo-interlace and black-frame insertion are impossible on ESP loop timing.

    [RTP sections, see §7]
11. **The chip cannot see the console's dot clock or its active height.** 256, 320 and 640
    horizontal modes are bit-identical in all 1536 bytes, as are 224-line and 240-line rasters, so
    per-console sampling and framing need a user control or a table
    [T&B § "2026-09-06 — A1: the chip cannot see which horizontal mode the console is in"; BEH
    § "Zoom — `geo.zoom`"].
12. **Their method rules:** verify the state stayed applied, look at the frame rather than a
    summary metric, measure rates rather than single samples, and use the branch point as the
    control [T&B § "Method: the pattern that produced eight errors"; RTP § "The standing rules"].

---

## 1. TV5725 architecture as they understood it

### 1.1 Board, buses and pins

- **Parts.** TV5725 plus SDRAM (the stock design), ESP8266 module, Si5351, SSD1306 OLED, rotary
  encoder, MS9288A VGA-to-HDMI and LM1881 sync separator. The stock Myson MTV230MV controller is
  still fitted but is not on the ESP's I2C bus [DSR § "Vendor documentation for the board"; § "Not
  identified"].
- **I2C scan, both directions.**

  | address | device |
  |---|---|
  | `0x17` | TV5725 |
  | `0x3c` | SSD1306 |
  | `0x50` | blank EEPROM: reads 0xFF everywhere; the stock controller's memory, unused |
  | `0x60` | Si5351 |

  [SEN § "The I2C bus, and the HDMI output"]
- **The MS9288A answers at no address.**
  - Pin 4, `MCUSEL`, floats, so the converter's internal MCU firmware is in charge.
  - Its I2C slave pins are 28/29. A register set exists behind them (the MS9282 brief shows it),
    but no map is published.
  - It has separate digital sync inputs: **HSIN (pin 34) and VSIN (pin 35)**, plus an analogue
    SOG (pin 42). "The converter wants explicit horizontal and vertical sync."

  [DSR § "What the MS9288A datasheet does and does not settle"; § "What the MS9282 brief adds…"]
- **Clocks and sync parts.** The Si5351 generates the output pixel clock on the **TV5725's pin
  40**. The LM1881 is the analogue front end for the RGBS jack and is not programmable
  [DSR § "The rest of the board"].
- **D1 mini pins.**

  | pins | use |
  |---|---|
  | D1/D2 | I2C, shared by the TV5725, Si5351 and OLED |
  | D6 (GPIO12) | `DEBUG_IN_PIN`: the chip's test bus carries vsync here for frame-lock and latency timing |
  | D3, D5, D7 | encoder |
  | D4 (GPIO2) | status LED; must be high at boot |
  | D8 (GPIO15) | free; must be low at boot |
  | D0 (GPIO16) | free; has no interrupt |
  | A0 | repurposed as VCC sense |
  | TX/RX | serial console at 115200 |

  [RTP § "Every pin of the D1 mini, and who holds it"]
- **Power.** The board takes 5 V and is "fussier about a weak supply than it looks" [FP § "What
  you need"]. The ESP 3.3 V rail read 3.47 V (3466 mV) [SEN § "The supply rail needs
  `ADC_MODE(ADC_VCC)`"; BEH § "The sleep timer was removed…"].
- **SDRAM.** The silkscreen says "2MB SD RAM" (16 Mbit, 1M×16). The part was not identified, and
  `MEM_DQM_DLY_REG` differs between factory presets [DSR § "Not identified"].
- **Inherited power notes.**
  - Memory clock at 108 MHz: −5%.
  - Both phase controls off: 506 → 493 mA.
  - `VDS_TAP6_BYPS` on: −2%.

  [FWD § "Power notes, inherited"]

### 1.2 Register space, segments and naming

- **Size and access.** 6 segments × 256 bytes = **1536 bytes**, readable raw per segment
  (`/api/dump?seg=0..5`). Their curated "state window" covered only 563 of the 1536
  [T&B § "No chip register distinguishes the two cases"; REG header: "563 bytes in 18 read
  windows"].
- **RD-5725-1.1 describes 558 registers:** 48 read-only and 510 read/write. All 48 read-only
  registers are contiguous, **s0_00–s0_2F**. A write to a read-only register is silently discarded
  and reads back unchanged. Eleven fields had been mis-curated as writable: `HPERIOD_IF`,
  `VPERIOD_IF`, `CHIP_ID_*` and six test-bus readouts [DSR § "Which registers can be written,
  settled"].
  - A different extraction counted "569 documented registers" [T&B § "The FIFO request policy…"].
- **Segment roles**, reconstructed from REG field loci and T&B's per-segment bisection
  [REG group headings; T&B § "2026-08-30 — bisecting the program write by segment…"]:

| seg | blocks | representative fields |
|---|---|---|
| 0 | read-only status (s0_00–2F); display PLL and memory clock (s0_40–43); pads and DAC (s0_44, 48, 49, 4F); soft resets (s0_46/47); test bus (s0_4D); blank control (s0_50); GPIO (s0_52); interrupts (s0_58); chip OSD (s0_90…) | `STATUS_00`, `HPERIOD_IF` s0_06, `VPERIOD_IF` s0_07, `PLL_MS` s0_40[4:3], `PLL648_CONTROL_01` s0_41, `DAC_RGBS_PWDNZ` s0_44.0, `PAD_SYNC_OUT_ENZ` s0_49.2, `PAD_CKIN_ENZ` s0_49.0, `OUT_SYNC_SEL` s0_4F, `TEST_BUS_SEL` s0_4D |
| 1 | input formatter; HD-bypass output path (s1_30…); firmware stash; mode detector (s1_60…) | `IF_HSYNC_RST` s1_0F, `IF_HB_SP2`, `IF_VB_SP`, `GBS_PRESET_ID` s1_2B, `MD_USER_DEF_VCNTRL/HCNTRL` s1_80/81 |
| 2 | deinterlacer: motion-adaptive and diagonal bob | `DEINT_00` s2_00, `MAPDT_VT_SEL_PRGV` s2_16, `MADPT_*` |
| 3 | VDS: output timing and scale (s3_00/01…), picture processing (s3_26…s3_73), PIP (s3_80) | `VDS_HSYNC_RST` s3_01, `VDS_HSCALE/VSCALE` (share byte s3_17), `VDS_Y_GAIN` s3_35, `VDS_Y_OFST` s3_3A, `VDS_UV_BLK_VAL` s3_3F |
| 4 | memory controller and the four FIFOs | `SDRAM_RESET_CONTROL` s4_00, `CAPTURE_ENABLE` s4_21, `PB_MAST_FLAG_REG` 4_2C, `PB_GENERAL_FLAG_REG` 4_2D, `PB_CAP_OFFSET` 4_37–39 |
| 5 | ADC; input PLL (PLLAD, s5_11–18); sync processor (s5_20…) | `ADC_INPUT_SEL` s5_02, `PLLAD_MD` s5_12, `PA_ADC_S` s5_18, `SP_*` coast s5_38/39/3E, clamp s5_56/57, `SP_H_CST_SP` s5_4F |

- **Their registry.**
  - **958 fields in 35 groups.**
  - Risk classes: safe 782, needs-reset 1, critical 175. The counts drift between versions: RTP
    records "783 safe and 174 critical" after one field was reclassed.
  - State layers: global 504, per-mode 339, not restorable 115.
  - **285 fields have a datasheet locus that was never exercised.**
  - 91 fields are gated (inert while a bypass bit is set).
  - Locus notation is `sS_AA bit:width`.

  [REG § "TV5725 field reference"; § "What the curation covers so far"; RTP § "The isolation,
  which is the finding"]
- **Name prefixes by block.**

  | block | prefixes |
  |---|---|
  | sync processor | `SP_` |
  | input formatter | `IF_` |
  | mode detector | `MD_` |
  | ADC and input PLL | `ADC_`, `PA_ADC_`, `PLLAD_` |
  | decimator | `DEC_` |
  | deinterlacer | `MADPT_`/`MAPDT_` (motion-adaptive), `DIAG_BOB_`, `DEINT_` |
  | memory and FIFOs | `MEM_`, `SDRAM_`, `CAP_`, `PB_`, `RFF_`, `WFF_` |
  | VDS (sub-blocks below) | `VDS_` |
  | display PLL | `PLL_`, `PLL648_` |
  | output pads | `DAC_`, `PAD_`, `OUT_` |
  | resets | `SFTRST_`, `RESET_CONTROL_` |
  | status | `STATUS_` |
  | other | `TEST_BUS_`, `GPIO_`, `INT_`, `OSD_`, `PIP_`, `HD_` (HD-bypass path) |

  VDS sub-blocks: `VDS_PK_` peaking, `VDS_NR_` noise reduction, `VDS_C1/C2_` selective
  correctors, `VDS_SK_` skin tone, `VDS_NS_` saturation, `VDS_BLEV_` black/white expansion,
  `VDS_SVM_` scan-velocity modulation, `VDS_BLUE_` blue stretch [REG group headings].
- **Datasheet traps.**
  - `pdftotext -layout` misaligns the bit tables: descriptions land beside the wrong bit.
  - The MS9288A PDF is Chinese-only, and only PyMuPDF extracts its text.
  - `VDS_UV_BLK_VAL` is the only field the vendor calls signed (top bit is the sign).

  [DSR § "The scaler"; § "One field is signed…"; § "What the MS9288A datasheet does and does not
  settle"]
- **Wrong annotations in their own map.** REG's "Measured effect: no-effect" for `VDS_HSCALE` is
  wrong [T&B § "`VDS_HSCALE` is not "no-effect", and the datasheet formula holds"]. The whole
  `memory` group (68 fields) is "locus from the datasheet, never exercised" [T&B § "What it means
  for the feature"].

### 1.3 How the picture flows (their model, assembled)

```
console --(RGBS via LM1881 | RGB+SOG | component | RGBHV)--> 3 ADC input sets (2 with SOG)
  -> ADC (sampled by input PLL PLLAD; phase PA_ADC_S; gain/offset ADC_*CTRL; clamp from SP)
  -> sync processor SP (measures H total / V total / H sync width, SOG slicer, coast, clamp timing)
  -> decimator (DEC_*)
  -> input formatter IF (line doubler for SD; capture window; HPERIOD_IF/VPERIOD_IF; mode detector MD)
  -> deinterlacer (motion-adaptive / bob; works through SDRAM via write/read FIFOs WFF/RFF)
  -> memory: capture FIFO -> SDRAM frame buffer -> playback FIFO (line-flag request policy)
  -> VDS: vertical+horizontal scaler (+ line buffer), picture processing, output timing generator
  -> YUV->RGB (VDS_CONVT_BYPS) -> triple DAC (<=162 MHz) + HS/VS pads
  -> MS9288A: re-digitises VGA with its own 10-bit ADCs, auto phase/gain/offset, enhancement,
     "Auto Position (Blank) Detection" -> HDMI (1080p60 output)
  (HD bypass path: input routed around the scaler; VDS registers then describe nothing)
```

Evidence for each link:

- The input formatter line-doubles SD before the scaler [T&B § "The arithmetic it rests on"; §
  "2026-08-29 — the source is 240p…"].
- Scanline doubling "happens before the scaler and not in it" [BEH § "Measured (PlayStation 1 in
  240p, NTSC, capture at 1080p)"].
- `VDS_HB_SP` "is used to get data from memory" [BEH § "Trims…"].
- There are four FIFOs [SEN § "Memory FIFOs"].
- The MS9288A re-digitises and runs its own processing [DSR § "What the MS9288A datasheet does
  and does not settle"; § "What the MS9282 brief adds…"].
- HD bypass goes around the scaler [SEN § "Passthrough was reporting stale numbers"].

### 1.4 Clock domains

- **Input PLL (PLLAD), which sets the ADC sampling clock.**
  - `PLLAD_MD` is the multiplier, i.e. **samples per input line**: 2345 in the 1280x960-era
    arrays, 2558 in upstream's 1920x1080 array.
  - `PLLAD_ND` is the input divider; only the array ever sets it.
  - `PLLAD_KS` is the VCO post-divider, with bands of 162–80, 80–40 and 40–20 MHz and 20 MHz down
    to the minimum.

  [REG § "Input PLL"; T&B § "What the two arrays actually disagreed about"; § "2026-09-19,
  later…"]
- **One IF unit is two ADC samples:** the IF counts at half the ADC sample rate. "46 ADC-unit
  difference, 23 IF units." `IF_HSYNC_RST` (the IF line length) is about half of `PLLAD_MD`: 1172
  against 2345 and 1279 against 2558
  [T&B § "The chip already measures the cause"; § "It is a different pipeline, not a different
  window"].
- **Display PLL (PLL648), `PLL648_CONTROL_01` at s0_41.**

  | code | display clock |
  |---|---|
  | 0x25 | 40.5 MHz (internal) |
  | 0x85 | 108 MHz (internal) |
  | 0x95 | 129.6 MHz (internal) |
  | 0xA5 | 162 MHz (internal) |
  | **0x75** | routed to the external pad, i.e. the Si5351 |

  0x75→0xA5 flips the sub-fields `PLL_VS4` 3→2, `PLL_2XV` 1→0, `PLL_4XV` 0→1
  [T&B § "Why: the clock has no way to bootstrap itself"; § "2026-09-04…"; § "2026-09-02 — what
  the apply leaves behind, byte by byte"]. `GBS_PRESET_DISPLAY_CLOCK` stashes the preset's PLL648
  value [REG § "Project state"].
- **Memory clock: `PLL_MS`, s0_40 bits 4..6.**

  | value | memory clock |
  |---|---|
  | 0 | 108 MHz |
  | 1 | 81 MHz |
  | 2 | from FBCLK (pin 110) |
  | 3 | 162 MHz |
  | 4 | 144 MHz |
  | 5 | 185 MHz |
  | 6 | 216 MHz |
  | 7 | 129.6 MHz (the shipped value) |

  The SDRAM bus is 32 bits wide (`MEM_MBUS32OR16_SEL` = 1) [T&B § "The overclock exists"; REG §
  "Display PLL"].
- **Timing measured by the ESP.** The chip's test bus is routed to D6: `TEST_BUS_SEL` 0 gives
  input vsync and 2 gives output vsync. Edges are timed in ESP cycle ticks at **6.25 ns**, since 2
  ticks = 12.5 ns and 3 200 054 ticks = 20.0003 ms [RTP § "A5 — the buffer latency…"; FWD § "What
  the phase measurement's aggressive actions actually buy (2026-09-07)"; T&B § "What holds"].
- **(Inference, not in docs) — HPERIOD_IF and the internal codes look 27 MHz-derived.**
  `HPERIOD_IF` ≈ 429 for a 63.5 µs line and ≈ 214 for 480p, which matches a 27 MHz reference
  counted /4; the internal PLL648 codes (40.5/108/129.6/162 MHz) are also 27 MHz-derived. SEN
  deliberately refuses to convert `HPERIOD_IF` to time because "the oscillator frequency is not
  exposed" [SEN § "Source"].

### 1.5 Soft resets (the bit map)

| `RESET_CONTROL_0x46` (s0_46) bit | block | `RESET_CONTROL_0x47` (s0_47) bit | block |
|---|---|---|---|
| 0 `SFTRST_IF_RSTZ` | input formatter | 0 `SFTRST_DEC_RSTZ` | decimator |
| 1 `SFTRST_DEINT_RSTZ` | deinterlacer | 1 `SFTRST_MODE_RSTZ` | mode detect |
| 2 `SFTRST_MEM_FF_RSTZ` | wff/rff/playback/capture | 2 `SFTRST_SYNC_RSTZ` | sync processor |
| 3 `SFTRST_MEM_RSTZ` | memory controller | 3 `SFTRST_HDBYPS_RSTZ` | HD bypass |
| 4 `SFTRST_FIFO_RSTZ` | FF64/FF512 | 4 `SFTRST_INT_RSTZ` | interrupts |
| 5 `SFTRST_OSD_RSTZ` | OSD | | |
| 6 `SFTRST_VDS_RSTZ` | VDS | | |

[T&B § "2026-08-30 — the datasheet on the reset controls, and one real causal finding"]

- **"resetDigital".** Upstream writes 0x46 = 0x41 and then 0x7F. That holds the deinterlacer,
  memory pointers, controller, FIFOs and OSD in reset **while the IF and VDS keep running**, then
  releases them all [same section].
- **Measured constraint.** The good state needs the producer (IF) and the consumer (VDS) live
  while the capture and playback pointers are released.

  | release sequence | good boots |
  |---|---|
  | original 0x41→0x7F | about 4 in 10 |
  | staged release, IF/VDS kept running | 4 in 8 |
  | IF and VDS stopped (staged release) | 0 of 7 |
  | IF and VDS stopped (all off, then all on) | 0 of 5 |

  [T&B § "Stopping the IF and the VDS guarantees the bad state"]
- **`ResetSDRAM`** is `SDRAM_RESET_CONTROL`=0x02; `SDRAM_RESET_SIGNAL`=1; =0;
  `SDRAM_RESET_CONTROL`=0x82, with no wait [T&B § "Six things in that sequence that can produce
  the bistability"].
- **Holding 0x46 = 0x00 across the 432-byte program write and restoring afterwards gives no
  picture at all** (8 boots) [T&B § "Holding the blocks in reset across the write does not
  work"].
- **Prepare the front end before loading a program for a new format:** the ADC and PLLs first
  (5_00, 0_40 bit 2, 0_43 bit 5; the last two clock the IF), then wait out the settling.
  Otherwise the capture side has no working clock and shows "fading patterns … a decaying frame
  buffer" [FWD § "Format-change classification"].
- **Register changes in the FIFO and buffer group act only through a path reset.**
  `PB_CAP_OFFSET` written to 264 and back left the state unchanged, so any sweep that skips the
  reset between points "is measuring nothing" [T&B § "And a method note that invalidates earlier
  probing"].

### 1.6 Programs ("presets") and the post-load sequence

- **A "program" is a 432-byte array covering all six segments.** `writeProgramArrayNew()` writes
  it 16 bytes at a time over I2C onto a **running** chip. It reads `RESET_CONTROL` 0x46/0x47 at the
  start and writes them back inside segment 0, so the blocks are live while segments 1–5 are still
  being written: "for several milliseconds the chip runs a half-old, half-new configuration"
  [T&B § "Why the program write is a plausible race"]. The write costs **about half a second of
  I2C** [FWD § "The post-load waits are a machine"].
- **Upstream had twelve factory presets**, identified by `presetID`: 0x05/0x15 is 1080p and
  0x06/0x16 is downscale. The set is NTSC/PAL × {1280x960, 1280x1024, 1280x720, 720x480/768x576,
  1920x1080, downscale}, reconstructed from SEN's per-preset sweep; no single doc lists all twelve
  [FWD § "The preset identity the base program still carries"; CAL § "`wantStepResponse`…"; T&B §
  "`doPostPresetLoadSteps()` was rewriting…"; SEN § "Swept across every supported resolution"].
  - Presets differ from one another in **31 fields outside the VDS block**: clocks, input
    formatter, memory/FIFOs, decimator and sync processor [T&B § "The design conclusion"].
  - Field by field: **508 constant, 40 vary only with the output geometry, 6 only with the input,
    66 with both** [T&B § "What to do next" (2026-08-25 round)].
- **Preset names do not describe the raster.**
  - The NTSC "1280x1024/1280x720/1280x960/720x480/downscale" presets all carry an active window
    of about 1980 units with `VDS_HSCALE` 512.
  - "1920x1080" carries htotal 1601, a 1008-unit window and hscale 1023, i.e. the horizontal
    scaler bypassed.

  [SEN § "What the datasheet did not settle: the active width"]
- **Displays identify the mode by its rates, not its register shape.** 1602 × 1126 @ 107.968 MHz
  gives a 67.396 kHz line and a 59.854 Hz frame, which a display accepts as 1080p (standard 2200 ×
  1125 @ 148.5 MHz gives 67.500 kHz and 60.000 Hz) [SEN § "What the display is locking to"].
- **The post-load is `doPostPresetLoadSteps()`: 835 lines writing 138 distinct fields.**
  - It **rewrites geometry**: `VDS_VSCALE` in 7 places (683, 546, 455, 410, 402, 400, chosen by
    presetID), `VDS_DIS_VB_ST/SP`, `VDS_VB_SP` and `VDS_VSCALE_BYPS`. Which branch runs depends on
    a `videoStandardInput` that is being re-measured during the relock, which made results look
    random [T&B § "`doPostPresetLoadSteps()` was rewriting the geometry that had just been
    applied"].
  - It also performs:
    - the resets and `PLLAD_LEN`;
    - `CAP_SAFE_GUARD_EN = 0` (switching off the guard that stops the capture pointer running into
      the playback pointer);
    - `RFF_WFF_OFFSET = 0`;
    - `PB_CAP_OFFSET` only for standards 3/4, as `PB_FETCH_NUM + 4`;
    - `ResetSDRAM`;
    - waits for sync (≤2.0 s), mode (≤1.5 s) and clamp (≤0.5 s);
    - unfreeze, capture centring and the picture base.

    [T&B § "2026-08-30 — the boot pipeline, read end to end"; FWD § "The post-load waits are a
    machine"]
  - It flattens `VDS_EXT_*` onto `VDS_DIS_*` unconditionally, so the array's own external windows
    never reach the chip [T&B § "2026-09-19, on the board…"].
- **Apply order matters.** It must be *program → post-load → timing*. The post-load running last
  overwrote the requested vertical scale; once the order was fixed, a requested `vscale` 824 stuck
  where 683 had replaced it before [T&B § "`doPostPresetLoadSteps()` was rewriting…"].
- **Per-mode registers are rewritten by every program load.** The MD user slot and
  `medResLineCount` must be re-applied after each load [RTP § "What this makes possible, and the
  part that needs no cleverness"; FWD § "The custom input mode is the chip's slot…"].
- **The NTSC and PAL 1280x960 arrays differ in 74 of 432 bytes.**
  - 35 of those are in segment 3.
  - 21 are rewritten at runtime anyway (`IF_HSYNC_RST`, `IF_HB_SP2`, `IF_VB_ST/SP`, `PLLAD_MD`,
    `PB_CAP_OFFSET`, `SP_*`).
  - **18 are array-only:**
    - `IF_LINE_SP`;
    - SDRAM timing `MEM_ACT/PCHG/CAS/RAS/CS0/CS1_*`;
    - the guards `CAP_SAFE_GUARD_A/B` and `WFF_SAFE_GUARD_A/B`;
    - `PB_GENERAL_FLAG_REG`, `PB_CAP_BUF_STA_ADDR`;
    - `PB_DOUBLE_REFRESH_EN`, `WFF_LINE_FLIP`;
    - `PLLAD_ND`.

  [T&B § "What the two arrays actually disagreed about"]
- **Design conclusion.** "Applying a timing cannot be patching the VDS block." A timing must sit
  on a coherent full program. The fork's last design reads the raster from upstream's
  `ntsc_1920x1080` array (identity 5) and sets the Si5351 to `htotal × vtotal × measured field
  rate`, with the vertical scale adapted per standard (455 NTSC, 546 PAL)
  [T&B § "The design conclusion"; § "2026-09-19, later — the base program is the 1920x1080
  array…"; § "2026-09-24 — …"].
- **Upstream wrote the program once per boot; the fork's computed-mode boot wrote it three
  times.** The program write is the only operation measured to re-roll chip-internal settling
  state [T&B § "The original writes the program once per boot; we write it three times"].

### 1.7 Output register semantics worth knowing before writing any code

- **Totals are total−1 in the control registers.**
  - Datasheet: "HSYNC_RST = A−1; in field double mode (A/2 − 1)".
  - Two mode bits change the meaning: `VDS_DFIELD_EN` (s3_00 bit 2: field doubling, halves the
    horizontal count, "output timing is interlaced") and `VDS_HALF_EN` (s3_00 bit 6). Keep both 0
    and treat `VDS_DFIELD_EN` as critical.
  - Status registers report raw counts.

  [SEN § "The totals are total-minus-one, and that was a real bug"]
- **Scale registers are divisors.** `SCALE = 1024 × (input/output)`, and **`scale × window` is
  invariant**: `VDS_VSCALE × (VDS_DIS_VB_ST − VDS_DIS_VB_SP) / 1024 = source active lines`. On the
  1280x960 base, 512 × 962 / 1024 = 481. The scale fields share a byte at s3_17
  [T&B § "The arithmetic it rests on"; § "`VDS_HSCALE` is not "no-effect"…"; § "Measured on both
  standards…"].
- **Three horizontal windows** [BEH § "Trims…"; T&B § "A gap the order fix itself opened…"]:

  | register moved +200 alone | effect on the frame |
  |---|---|
  | `VDS_DIS_HB_SP` (display window, a *mask*) | clean cut; picture did not move; no tearing |
  | `VDS_HB_SP` (memory-fetch blanking) | picture shifted 75 px right and every line tore |
  | `VDS_EXT_HB_SP` (external blanking) | nothing visible on the capture card |
  | `IF_VB_SP` +3 (capture start) | picture moved 12 px: the capture start positions, it does not mask |

- **Blanking wraps around the line.** The blanking region runs from `VDS_HB_ST` through the end
  of the line and round to `VDS_HB_SP`. Content that runs past the right edge reappears at the left
  [T&B § "2026-08-30 — the left artefacts were the horizontal blanking start"].
- **There is no valid data beyond the capture window.** Opening `VDS_DIS_HB_ST` past it (1356 →
  1400) shows a band of green noise from the line buffer [T&B § "Two readings that were wrong on
  the way…"].
- **Full-saturation green is Y=U=V=0,** "what the decoder gives for data outside the clamped
  active video" [T&B § "What it was"].

---

## 2. Input side

### 2.1 Inputs, SOG and sync types

- **ADC input selection, `ADC_INPUT_SEL` (s5_02 bits 6–7).**

  | value | input |
  |---|---|
  | 00 | R0/G0/B0/SOG0 |
  | 01 | R1/G1/B1/SOG1 |
  | 10 | R2/G2/B2, with no SOG |
  | 11 | reserved |

  "The third input has no sync-on-green pin — this is a property of the chip"
  [REG `ADC_INPUT_SEL`; SEN § "Analog front end"; FP § "No signal on …"]. The two SOG-capable
  sets are different connectors and "one of them carries component". Choosing by connector is the
  only option, because three analogue voltages cannot be told apart [T&B § "Two sources, one
  configuration"]. **Not in their docs:** which physical jack maps to which value. Verify on the
  board.
- **The RGBS jack goes through the LM1881** sync separator, which "explains what the TV5725's sync
  processor receives on that input" [DSR § "The rest of the board"].
- **`STATUS_SYNC_PROC_VSACT` is raised only by discrete VSYNC.** A SOG source never raises it,
  because its vertical comes out of the sync separator and shows only in the line count. Reading it
  as "no vertical" is wrong for most consoles [SEN § "Source"].
- **The discrete-VSync probe** (`probeDiscreteVSync()`) [FWD § "`probeDiscreteVSync()`…"]:
  - **0_16 bit 3 ("VS active") is unreliable.** An Atari ST in mono (501 lines @ 71 Hz) never sets
    it, yet the sync processor counts the line total perfectly. Judge by a measured line count that
    holds still: the median of nine reads, two thirds agreeing.
  - CSync sources read 0 or wander when the HV input is connected.
  - The probe turns SOG slicing off for about 1/3 s, so it is retried every ~3 s (150 passes at
    20 ms) for never-named sources, or ~30 s for ones that were named once.
- **Choosing CSync is "a one-way door".** `syncTypeCsync` gates the discrete H/V input off, after
  which nothing can show that the source has its own vertical, so a source misjudged as CSync
  stays misjudged: H-locked at best, V free-running, rolling picture
  [FWD § "The CSync switch is a one-way door"; § "`probeDiscreteVSync()`…"].
- **SOG controls.** The slicer level `rtLock->currentLevelSOG` runs 0..31. Unstable SOG sets
  `STATUS_INT_SOG_BAD` (s0_0F), and the SP has SOG polarity auto-correct and invert bits
  (`SP_SOG_P_ATO`/`_INV`) [SEN § "Analog front end"; § "Locks"; REG § "Sync processor"].
- **SOG is marginal on dark content.** The HS-active flag flickers for a frame and must be
  **debounced**. A single bad read triggered a sync-loss chain and `optimizePhaseSP()` re-runs that
  "show up as pixels jumping around" [FWD § "HSACT debounce"].

### 2.2 Standards and how detection works

- **`videoStandardInput` codes.**

  | code | standard |
  |---|---|
  | 0 | none |
  | 1 | NTSC: 240p/480i at 60 Hz |
  | 2 | PAL: 288p/576i at 50 Hz |
  | 3 | 480p |
  | 4 | 576p |
  | 5 | 720p, passed through |
  | 6 | 1080i, passed through |
  | 7 | 1080p, passed through |
  | 8 | medium-res PC modes (named via `MD_HD1250P_CNTRL`) |
  | 9 | stable-unknown: line count steady for 255 reads |
  | 13 | VGA–SXGA, passed through |
  | 14 | RGBHV scaled |
  | 15 | RGBHV bypass |

  10–12 are unassigned [FWD § "The video standard, by name"].
- **240p and 480i are the same standard to the firmware.** Switching between them does not reload
  or renegotiate anything; only the deinterlacer engages [RTP § "B3 — the 240p/480i switch costs
  nothing…"].
- **The mode is decided in one place,** `modeRead()`, from `STATUS_00`, `STATUS_03`, `STATUS_04`,
  `STATUS_05` and `STATUS_16` [RTP § "The stated risk is not the shape of the work"].
- **Detection is a sequence of steps.**
  - RGB: up to 360 ms waiting for VSync and 400 ms for HSync, a 120 ms settle, three field-rate
    decodes and sixteen 30 ms medium-res steps, or a 6 s window polling every 2 ms while walking
    the SOG level.
  - Component: the same 6 s window.

  [FWD § "The input detection is a machine"]
- **Format change.** Sort source and preset into the same three bands and act when they disagree
  (a PAL-like 312 lines against an NTSC-like 262). With the H/V input gated off, the SP free-runs
  its vertical and "reads as a rock-steady format change" that never happened, so ask the H/V
  pins first [FWD § "Format-change classification"].
- **CRTC timing changes** (e.g. Amstrad CPC PAL lines 3× longer) overflow the 12-bit
  `STATUS_SYNC_PROC_HTOTAL`, which then reads garbage (9) while HSACT stays high. Do not freeze and
  do not treat it as sync loss. Block recovery while it lasts, then run a targeted frame-sync and
  phase reset [FWD § "CRTC timing-change detection"].
- **Fallback preset.** Sources the MD unit has no profile for (an Atari ST at 35.7 kHz) sit at
  mode 0 forever, because resolving needs H lock and lock needs a preset. Load a fallback once per
  sync loss (at `noSyncCounter` 200), then re-probe the sync type
  [FWD § "The fallback preset, and probing straight after it"].
- **`STATUS_IF_INP_INT` is not an interlace flag.** It reads 1 for 240p and for 480i. The real
  test is the **parity of `VPERIOD_IF`** (even = interlaced, odd = progressive), plus
  `MAPDT_VT_SEL_PRGV`. The interlaced signatures are 522/524/526 and 622/624/626
  [RTP § "`STATUS_IF_INP_INT` is not an interlace flag…"; RTP § "Session B…"; SEN § "Source"].
- **The chip cannot see the console's horizontal mode.**
  - PS1 at 256, 320 and 640 wide: whole 1536-byte captures differ only in free-running counters
    (0 bytes outside them).
  - `srcHTotal` 2345, `hPeriodIf` 428, `srcHLowLen` 170, `PLLAD_MD` 2345 and 15.554 kHz in all
    three.
  - The checkerboard period in the capture did change exactly with the mode (11.11, 8.85 and 4.42
    px).

  "Per-console sample-rate detection from the chip is impossible." [T&B § "2026-09-06 — A1…"]
- **The chip cannot see the console's active height.** A Mega Drive in 224-line mode sends the
  same raster as 240: `VPERIOD_IF` 525, `srcVTotal` 260, 15.584 kHz, 36 543 628 Hz. It draws 224
  lines and fills the rest with border. "The chip has no active-area detector" [BEH § "Zoom —
  `geo.zoom`"; T&B § "Two sources, one configuration"].

### 2.3 The user-defined mode slot (works; one slot only)

- **What the slot is.** `MD_USER_DEF_VCNTRL` (s1_80) and `MD_USER_DEF_HCNTRL` (s1_81) are
  vertical and horizontal "period numbers". `STATUS_IF_INP_USER` (s0_05 bit 0) asserts **only when
  both match**; either one alone never asserts. The parking value is 255/255
  [RTP § "The chip has a user-defined mode slot…"; BEH § "What the chip provides"].
- **Measured on a Mega Drive at 480i** (`VPERIOD_IF` 524, `HPERIOD_IF` 429):
  - V=32 asserts at H=173–174;
  - V=31 asserts at H=173, V=33 does not;
  - V=32/H=172 does not assert.

  [RTP § "Measured on the bench"] BEH reports the window over several runs as V 31–32 and H
  172–174: two or three values wide, dithering by one between runs, so keep the middle
  [BEH § "Measured" (Custom input mode)].
- **Encoding.** V=32 is the same encoding as `MD_NTSC_INT_CNTRL` uses for interlaced NTSC, and it
  "comes out near `VPERIOD_IF >> 4`". In practice, sweep until the bit asserts ("learn mode")
  [RTP § "Measured on the bench"; BEH § "What the chip provides"].
- **The match is parallel, not a fallback.** `STATUS_00` stays 0x8f and the chip's own
  classification is untouched, so consult the slot **after** every automatic check. It **needs
  automatic detection running**: setting `MD_DET_BYPS_H/_V` = 1 stops it asserting
  [RTP § "Measured on the bench"; FWD § "The custom input mode is the chip's slot…"].
- **What the `MD_*_USER_ID` and `MD_DET_BYPS_*` bits really are.** They are not mode selectors but
  the **stable/unstable estimation** controls (S1_82 horizontal, S1_83 vertical), plus the
  sync-connected and mode-change indicators [RTP § "Two method notes"].
- **The slot is per-mode, so a program load rewrites it.** Re-apply it after every load
  [RTP § "What this makes possible…"].

### 2.4 Source measurements and what they mean

| register | unit and meaning | pitfalls |
|---|---|---|
| `STATUS_SYNC_PROC_HTOTAL` | sampling clocks per line, i.e. it counts at the preset's `PLLAD_MD` rate | **not absolute**: the same 480p source reads 2345 under one preset and 2558 under another (~10%); 12-bit, overflows on long lines; flickers (single-digit garbage between good reads); read the highest of three [SEN § "Why `HTOTAL` is in "sampling clocks"…"; T&B § "The standard modes, measured with repetition"; FWD § "`probeDiscreteVSync()`…"] |
| `STATUS_SYNC_PROC_VTOTAL` | lines per field | 259 (a 240p source), 260 (MD 224/240p), 524 (480p); flickers like HTOTAL [T&B § "The input changed…"; § "2026-08-29 — the source is 240p…"; BEH § "Zoom…"] |
| `STATUS_SYNC_PROC_HLOW_LEN` | H sync width in sampling clocks | **reads garbage for a while after a preset load** (it was 80); occasionally halves (82 against 170); can be stable but **stale** after a console swap [T&B § "It needs a settled reading"; § "The sync reading has to outlive the source change"; § A1] |
| `HPERIOD_IF` | IF's own oscillator count (REG: "total pixels / 4") | 428–430 for 15.5 kHz, 214 for 480p; dithers 429/430; read-only [SEN § "Source"; T&B § "HPERIOD_IF is not the detector either"] |
| `VPERIOD_IF` | lines **after the formatter**, i.e. after doubling | 523 (240p doubled), 524 (480i/480p), 525 (MD 224/240p), 624 (PAL 576i); stays 523 even with the doubler bypassed [T&B § "Bypassing the doubler is worse, measured"] |

- **Rates derived without disturbing the chip** (the output is phase-locked, so output refresh =
  source field rate) [SEN § "Source rates, derived"]:
  - source line rate (kHz) = `outRefreshHz × srcVTotal / 1000`;
  - source pixel clock = `srcHTotal × srcVTotal × outRefreshHz`. 240p gives 2558 × 259 × 59.96 =
    **39.7 MHz** (line 15.53 kHz) and 480p gives 2345 × 524 × 60.04 = **73.8 MHz**.
  - These are suppressed outside 20–130 Hz, or when the external generator is not driving the
    display.
- **Their intrusive true-rate measurement** (`getSourceFieldRate`/`getOutputFrameRate`/
  `getPllRate`) retunes the test bus, enables the debug pin and times a pulse. "Accurate and
  intrusive", so run it only on demand. It needs a running video path: `getSourceFieldRate()`
  returns 0 at boot before a picture exists [SEN § "What counts as a sensor here"; T&B § "Why: the
  clock has no way to bootstrap itself"].
- **Lock bits.** `STATUS_MISC_PLLAD_LOCK` dithers even on a perfectly stable picture (ten reads:
  `0 0 1 1 1 1 1 0 0 0`; counts out of 8: `8 7 8 8 5 0 8 8 8 0`). `STATUS_MISC_PLL648_LOCK` read
  8/8 every time. Report counts, not booleans, and treat input-PLL flicker as a property of the
  bit [SEN § "Locks"].
- **Status reads are cheap and safe.** Eight consecutive reads of the whole sensor set left
  `TEST_BUS_SEL` and `PAD_BOUT_EN` untouched [SEN § "What counts as a sensor here"].

### 2.5 ADC sampling and phase

- **`PLLAD_MD` (samples per line) is a critical field.**
  - Changing it alone (2450/2558/2660, read back to confirm) did **nothing**: the picture was
    identical and `IF_HSYNC_RST` stayed 1172. Toggling `PLLAD_CONTROL_00_5x11` to relatch changed
    nothing either.
  - Upstream's 1080p array carried a *consistent pair* in its array: `PLLAD_MD` 2558 with
    `IF_HSYNC_RST` 1279.

  [T&B § "PLLAD_MD does nothing on its own"; § "It is a different pipeline, not a different
  window"]
- **`IF_HSYNC_RST` is a preset constant.** It is written only as 0x3FF in the reset path; the live
  value comes from the array (s1_0F). It is the denominator of every input-side quantity. **Setting
  it to 1279 on a mode sampled at 2345 tore the frame in half**, with green bands
  [T&B § "IF_HSYNC_RST is a preset constant…"; § "But 1279 tears the frame…"].
- **Sampling should match the output raster.**
  - Sampling at 2345 into a 2080-unit output line (hscale 625, a 1.638x magnification) made "every
    output sample a blend, none a copy". The horizontal energy was a third of upstream's.
  - Upstream's 1080p array samples at 2558 into a 1602-unit line with the horizontal scaler
    bypassed (1:1).
  - After switching to that array, horizontal detail doubled, but was still about ¾ of upstream's.

  [T&B § "2026-09-19 — the three halves are 7.5% wide…"; § "2026-09-19, later…"; § "What the
  three symptoms measure now"]
- **Sampling phase.** `PA_ADC_S` (5 bits) was already optimal: 74.84% against 74.88% for the best
  of 16 phases measured [CAL § "What none of this fixed"]. The firmware also runs
  `optimizePhaseSP()` in recovery [FWD § "HSACT debounce"].
- **ADC lock positions.** Upstream wrote `DEC_WEN_MODE = 1`, commented "1 keeps ADC phase
  consistent, around 4 lock positions vs totally random" [T&B § "Two comments in the original that
  matter"].
- **Input low-pass versus interpolator.** `IF_HS_SEL_LPF` = 0 is the interpolator: more luma
  bandwidth and a third less chroma aliasing on a one-pixel stripe (87.14% against 84.06% luma
  modulation). On real game content the difference is only −0.39% of horizontal detail. The HD
  path already used 0 [CAL § "`IF_HS_SEL_LPF` — 1 becomes 0"; RTP § "`IF_HS_SEL_LPF` — real, and
  too small to name"].

### 2.6 Gain, offset and auto-gain

- **ADC gain registers `ADC_RGCTRL/GGCTRL/BGCTRL` run 0..255, and a lower number means more
  gain.** The preset value is **0x7B (123)** [CAL § "What the cold boot does"].
- **Their auto-gain loop was broken by design.**
  - It starts at `AUTO_GAIN_INIT` 0x48 (72) on every cold boot, which is brighter than the preset.
  - `runAutoGain()` only ever increments: it reads `TEST_BUS_2F` up to 61 times and walks the gains
    in steps of 2.
  - Measured: 0x48 at post-load, 0x94 at +100 s, 0xA6 48 s later "still climbing". The picture
    starts bright and dims for as long as the board is on.
  - The default reverted to **off**: the gain stays at the preset's 0x7B, as upstream does.
  - A correct loop would need to lower as well as raise, and start from the preset's value.
  - With auto-gain on, it also waits for 90 stable passes and a positioned clamp.

  [CAL § "`enableAutoGain` — off became on…"; BEH § "Automatic ADC gain — `enableAutoGain`"; §
  "Why `enableAutoGain` is not just `DEC_TEST_ENABLE`"]
- **Auto-gain shares a bit with other code.** It uses `DEC_TEST_ENABLE` (the decimator test bus),
  which `calibrateAdcOffset()` borrows temporarily, so the bit cannot also store the user's intent
  [BEH § "Why `enableAutoGain` is not just `DEC_TEST_ENABLE`"].
- **Auto-gain's one benefit, from a start of 123** on a Mega Drive: it backed the gains off from
  123 to 133, taking peak luma from 218.0 to 215.1 (the chain ceiling is 218.5) and the fraction of
  the frame clipped from 0.235% to 0.000% [CAL § "What August measured, and what it did not"].
- **White balance is per source, and per-channel trim was declared a non-goal.**
  - The PlayStation needed gains 157/186/208 to bring a grey field from 26.1 levels of spread to
    0.0.
  - The Mega Drive measured 1.0 level of spread with equal gains.
  - Auto-gain moves all three gains together, so it overwrites any trim.

  [CAL § "The per-channel trim is a non-goal"]
- **Offsets.** One session read gains 208/208/208 and offsets 62/61/61 [T&B § "What the same frame
  says about colour"]. "The chip has an automatic offset loop" that overwrites hand-set black
  levels [FP § "What the board decides for itself"]. The offset calibration
  (`calibrateAdcOffset()`) is gated by `enableCalibrationADC` and runs on the next source
  detection [BEH § "Calibrate the ADC — `enableCalibrationADC`"].

### 2.7 Clamp and coast

- **Clamp sequence on a program load** [FWD § "Clamp position and the dark hold"]:
  - The program write **unlocks the clamp** by patching 5_57; every preset load forces
    `SP_NO_CLAMP_REG` = 1, and only a successful clamp-position run clears it.
  - Until the clamp is set, "black drifts upwards" (the AC-coupled input has nothing holding the
    back porch), so **hold the output dark** until the clamp locks.
  - `updateClampPosition()` needs **16 agreeing `HPERIOD_IF` readings**. It gets up to 0.5 s early
    (25 attempts 20 ms apart), then retries from the loop.
  - Some csync SD sources never set the IF mode bits. If the code returns early for them, the
    clamp stays off for good: "a grey that fades in".
- **Clamp fields:**
  - `SP_CLAMP_MANUAL` (software-commanded clamp; s5_56 bit 2);
  - `SP_CLAMP_INV_REG`;
  - `SP_NO_CLAMP_REG` (s5_57 bit 0);
  - `SP_CLP_SRC_SEL` (clamp clock source);
  - the clamp window `SP_CS_CLP_ST`/`SP_CS_CLP_SP` (s5_41/s5_43, 12 bits);
  - the sync-separation window `SP_CS_HS_ST`/`SP_CS_HS_SP`;
  - the separated-vertical positions `SP_SDCS_VSST/VSSP`. Upstream issue #401 found the post-load
    writes these differently depending on the preset's origin [T&B § "2026-08-26 — the cause of
    half the problem…"].

  [REG § "Sync processor"]
- **Coast fields:**
  - `SP_PRE_COAST` and `SP_POST_COAST` (coast lines before and after vsync; s5_38/39);
  - `SP_H_COAST`, `SP_DIS_SUB_COAST`;
  - `SP_COAST_INV_REG`, `SP_NO_COAST_REG`, `SP_COAST_VALUE_REG` (s5_57).

  [REG § "Sync processor"]
- **The watcher owns the sync-processor timing.** Clamp position, coast lines and sync protection
  are "all rewritten by the watcher as the source settles" [FP § "What the board decides for
  itself"].
- **Each re-apply needs settling time:** "the PLLs locking again, the clamp and the coast
  re-establishing" [T&B § "The fix, and the detail that made it fail"].
- **Thresholds on the stable-pass counter** (`continousStableCounter`): clamp retry at 4,
  `autoBestHtotal` at 10, frame-time lock at 20, external-clock sync at 35, auto-gain at 90 [FWD §
  "`continousStableCounter` and mode-less sources"].

### 2.8 Input formatter: line doubler, capture window, centring

- **The line doubler is active for SD.** `IF_LD_RAM_BYPS` = 0. 240p becomes about 480 lines
  before the scaler, so the true chain is **240 → doubler ×2 → 480 → scaler ×2 → 960**.
  - Bypassing the doubler and doing ×4 in the scaler (`VDS_VSCALE` 256) is **worse** (SOME PICTURE
    at mean 58.92 against BARS at 64.81).
  - `VPERIOD_IF` stays 523 when bypassed.

  [T&B § "2026-08-29 — the source is 240p…"; § "Bypassing the doubler is worse, measured"]
- **Horizontal capture window registers** [T&B § "What the registers actually do"; § "And it
  closes off one register"]:
  - `IF_HB_SP2` is a **pure translation**: 1 unit moves the picture 2 output px at hscale 512.
  - `IF_HB_ST2` is **inert**: it sits past the source's active area (tested at 1124, 1056, 996 and
    1202).
  - `VDS_HSCALE` sets the size.
- **Vertical capture window registers.** `IF_VB_SP` is a translation of about 2.25 output px per
  unit; `IF_VB_ST` must move with it (`IF_VB_SP − 2`). `IF_INI_ST` does nothing
  [T&B § "The vertical, found the same way"].
- **The centring formulas:**

  `IF_HB_SP2 = IF_HSYNC_RST × activeCentre − (outputWindow/2) × VDS_HSCALE / 1024`

  `IF_VB_SP = VPERIOD_IF × activeCentre − (outputLines/2) × VDS_VSCALE / 1024`

  [T&B § "The rule that follows"; § "The vertical, found the same way"]
- **How the centre constant evolved, from latest to earliest:**
  - Anchored to the array's own capture window, `(IF_HB_SP2 + IF_HB_ST2)/2`, corrected by the sync
    width slope referenced to HLOW 170 [T&B § "The capture centring had to be re-anchored…"].
  - Sync-width fit `centre = 735 − 0.437 × HLOW`. The slope is a real measurement (MD HLOW 216, PS
    170, active start about 20 IF units apart); **the 735 is not**, because absolute position moved
    16 units (32 px) across an HDMI relock (165.8 against 149.4). Validated once in-session: MD
    predicted 145, measured 143.4 [T&B § "The chip already measures the cause"; § "And here is
    what this bench cannot tell you"; § "The formula is right"].
  - Per console: MD centred at `IF_HB_SP2` 146, PS1 at 170 [T&B § "One constant cannot serve two
    consoles"].
  - Constants: NTSC 506 → 547 permille (horizontal); vertical 478 → 463 (NTSC), 486 (PAL) [T&B §
    "The constant"; § "The fix: the vertical scale is derived…"].
- **Sync-low readings need care.** Use them only once settled: four reads in a row within 120..320
  agreeing to ±4, up to 300 ms. Re-centre about 4 s after a source change, because a stable
  reading can still be **stale** [T&B § "It needs a settled reading"; § "But it was reading the old
  console's sync"].
- **The capture window is wider than the source's active video** (990 against about 907 input
  units). The excess is blanking and "has to land somewhere". Match the captured span to the real
  active width [T&B § "Where it ended, from a cold boot"].
- **A guard that silently dropped writes.** The vertical write was guarded with `sp >= 4` when
  the correct value was 3, so every write was dropped. An upstream `needPostAdjust` (RGBHV) block
  hard-coded `IF_VB_SP` 8 and `IF_VB_ST` 6. Fixing the guard (`sp >= 2`) recovered 12 lines (rows
  0..1061 → 0..1077) [T&B § "2026-08-30 — the vertical was being cut, and the guard was why"].

---

## 3. Output timing

### 3.1 How an output timing is computed and applied (the last design that worked)

- **The fork's timing engine.**
  - **CVT-RB-style computation:** htotal, vtotal, sync, windows and the clock from width, height
    and rate.
  - **Applied on a full base program,** in the queued sequence *PROGRAM → POSTLOAD → TIMING*, then
    a path reset, a clock ramp and a deferred settled pass. The deferred pass runs aspect, centring
    and user overrides, then **the FIFO line flags, last**.
  - The HTTP call returns 202 and the main loop does the work. A 20 s auto-revert is armed unless
    confirmed (but not at boot).

  [T&B § "The design conclusion"; § "Where it hooks in"; § "2026-08-30 — the boot pipeline, read
  end to end"; FWD § "Every apply must end…"]
- **Three things touch a timing after it is applied:**
  - the 20 s revert;
  - the sync watcher reloading the preset;
  - `externalClockGenSyncInOutRate()` recomputing the clock.

  [T&B § "Three things touch the timing after it is applied"]
- **Order traps:**
  - The post-load overwrote the geometry, which is why the order is *program → post-load →
    timing*.
  - `VDS_EXT_*` must be re-copied from `DIS` after the timing.
  - The FIFO flags are undone by any later path reset.

  [T&B § "`doPostPresetLoadSteps()` was rewriting…"; § "A gap the order fix itself opened…"; §
  "Order matters, and it is a real defect"]
- **"Apply twice."**

  | apply variant | with picture |
  |---|---|
  | one pass (same 18 s wait before capture) | 7 of 15 |
  | two passes (same 18 s wait) | 15 of 15 |
  | two passes started back-to-back, no pause | 3 of 5 (no better than one) |
  | earlier campaign, before the fix | 3 of 8 |
  | after the fix | 13 of 13 |

  - The second pass needs a **pause of about 4 s** between passes. The apply goes from about 4 s to
    about 12 s.
  - Waiting 53 s after a single pass does not rescue a black screen.
  - The cause is unknown. Nothing readable differs: three bytes, all measurements.
  - **On a black state, taking `VDS_VSYNC_RST` to another value and back rescued 3/3.** Doing the
    same back-and-forth inside the apply did **not** prevent the black (2 of 5).
  - Later caveats:
    - The evidence was gathered with a detector blind to tearing.
    - `DEFAULT_PASSES = 1` gave "no picture at all on six of eight boots".
    - One-pass applies came out black 4/4.
    - The two-pass apply is also what leaves the display PLL at 0xA5.

  [T&B § "The coin toss was per-apply…"; § "The comparison that decides"; § "Bisecting the second
  pass"; § "The obvious remedy that did not work"; § "What this does to `DEFAULT_PASSES = 2`"; §
  "The original writes the program once…"; § "2026-09-02, later — the display clock…"]
- **The first apply after a restart is clean; repeated applies accumulate damage.**

  | position after a restart | clean | torn |
  |---|---|---|
  | first apply | 4 | 0 |
  | second apply | 2 | 2 |
  | third and later | 0 | 6 |

  After three applies, the capture card stopped locking entirely. The causes were later found
  (§3.6, §4): the truncating aspect pass and the missing FIFO flags. After both fixes, four applies
  in a row were clean 4/4 with the deinterlacer on
  [T&B § "2026-09-01, later still — re-measured at the right rate…"; § "Three applies and the
  capture card stops locking"; § "2026-09-02 — fixed…"].
- **Do not chain window changes.** Three window changes in a row locked the HDMI converter and the
  capture card out until a board restart, while the chip was healthy. "One change from a settled
  state." [T&B § "One thing to know before using it"; RTP § "The standing rules"]
- **The requested rate is ignored; output = source rate.** Requests for 54, 53, 50 or 60 Hz all
  ended at **59.854 Hz**, the source rate. `externalClockGenSyncInOutRate()` sets the generator to
  `(sfr/ofr) × f`, and it runs outside the `enableFrameTimeLock` test. So "1536x1152@50" describes
  a PAL *source*, not a setting [T&B § "2026-09-14 — the rate you ask for is not the rate you
  get…"].
- **A single static array cannot serve both NTSC (59.86 Hz) and PAL (50 Hz)** with one
  `htotal × vtotal × clock`, so the clock must be computed from the measured source rate
  [T&B § "Why the output mode cannot be a static array"].

### 3.2 The pixel-clock rule versus the input clock

- **The measurement** (480p input at 73.8 MHz, active 1280x724, vtotal 750, htotal varied):

  | htotal | clock | result |
  |---|---|---|
  | 1440 | 64.8 MHz | black |
  | 1700 | 76.6 MHz | black / no frame |
  | 1800 | 81.1 MHz | 0/3 |
  | 1850 | 83.3 MHz | no frame |
  | **1900** | **85.6 MHz** | **BARS** |
  | 2000 | 90.1 MHz | 2/2 |
  | 2398 | 108.0 MHz | 3/3 |

  The boundary is **1.13–1.16 × the input clock**. The same geometries at 240p (input 39.7 MHz)
  were 1.64× and always worked. "The output has to consume pixels faster than the input produces
  them, with margin." [T&B § "The rule: the output clock has to beat the input's"]
- **This is a physical limit with no remedy,** distinct from the per-apply coin toss: htotal 1800
  stayed 0/3 after the two-pass fix [T&B § "The control: the clock limit was not erased along with
  it"].
- **How the firmware uses it.** It estimates the input clock as
  `STATUS_SYNC_PROC_HTOTAL × STATUS_SYNC_PROC_VTOTAL × rate`, and **warns below 1.20×**. The rate
  must be the output **in effect** (it follows the input), not the requested rate [T&B § "The rule…";
  § "Widen the blanking instead of only warning"].
- **The estimate swings about 10% with the preset** (73.8 against 80.4 MHz), because HTOTAL
  counts at the preset's sampling rate. Raising the margin to 1.32 did not fix 1280x1024 (1 of 4)
  and cost a lot of blanking (720x480 → htotal 3568) [T&B § "The standard modes…"; § "The clock
  margin does not explain the 1280x1024"].
- **Widen the blanking.** When the clock is too low, raise `htotal` (rounded to a multiple of 8);
  this "increases the clock without touching the active area, the rate or the visible geometry".
  The factory presets do the same: the 720p one is 2402 total for 1982 active. Against a 480p
  input:

  | mode | before | after |
  |---|---|---|
  | 720x480@60 | ht880, 26.2 MHz, warning | ht2976, 88.8 MHz |
  | 768x576@50 | ht928, 27.5 MHz, warning | ht2992, 88.7 MHz |
  | 1024x768@60 | ht1184, 56.0 MHz, warning | ht1872, 88.5 MHz |
  | 1280x720@60 | ht1440, 64.0 MHz, warning | ht2000, 88.6 MHz |
  | 1280x960@60 | ht1440, 85.2 MHz, warning | ht1496, 88.6 MHz |
  | 1280x1024@60 | refused | ht1440, 91.0 MHz |
  | 1920x1080@60 | refused | ht2080, 138.5 MHz |
  | 1920x1080@50 | refused | ht2080, 115.0 MHz |

  [T&B § "Widen the blanking instead of only warning"] With a 240p input the same engine gave
  720x480 at 1600 × 499 / 47.727 MHz: the widening is relative to the input estimate
  [SEN § "The timing engine gets the geometry right where the presets do not"].

### 3.3 Hard limits on the output side

| limit | value | what sets it | source |
|---|---|---|---|
| DAC clock | **162 MHz** | TV5725 DS-5725-3.2 Table 19; the DAC comes first in the chain | DSR § "What the MS9288A datasheet does and does not settle"; T&B § "2026-09-15 — the frame lock could drive the clock past the DAC…" |
| MS9288A sampling/TMDS | **165 MHz** | MS9288A datasheet §2. "The wrong number to guard on", since 162 comes first | DSR (same); SEN § "The HDMI converter is not on the bus" |
| clock held at vtotal 1111 | every point from 138 to **160.8 MHz** gave a picture | bench, 2026-09-14 | T&B § "2026-09-14 — the series above changed two things at once…" |
| sink floor (their converter and HD60) | black ≤ **104.0 MHz**, picture ≥ **104.9 MHz**; `CLOCK_SINK_FLOOR` 104.5 MHz (warn only) | bench, 1111-line rasters at about 59.85 Hz; "a property of the converter and capture card on this bench, not of the TV5725" | T&B § "It is the pixel clock, and nothing else" |
| converter input resolution | "Supports input resolution up to 1080P" (MS9282 family brief) → "1080 lines is the practical ceiling of this chain" | vendor brief | DSR § "What the MS9282 brief adds…" (but see the 1200-line results below) |
| converter output | 1080p@60 is listed under output | MS9288A datasheet | DSR (same) |
| vertical total guard `VTOTAL_SAFE` | 1000 → 1130 → **1200** (2026-09-02) → **1600** (2026-09-14) | bench; "the real ceiling is the clock" | T&B § "2026-09-02 — the vertical ceiling…"; § "2026-09-14 — the series above…" |

- **The pixel-clock sweep** that set the floor:
  - Carried: 104.9, 106.5, 106.9, 109.9, 110.1, 117.0, 123.0, 129.7, 130.2, 138.3, 142.0, 152.9 and
    153.8 MHz.
  - Black: 90.8, 95.8, 99.9, 102.9 and 104.0 MHz.
  - The same 1280x1080 active area with htotal widened went black at 1501 (99.9 MHz) and 1546
    (102.9 MHz), and had a **picture at 1576 (104.9 MHz)**, 1651, 1951 and 2300.

  [T&B § "It is the pixel clock, and nothing else"]
- **Vertical-total history:**
  - Clean at 1111 (138.5 MHz), 1150 (143.4 MHz) and **1200 (149.6 MHz, 4/4)**. At **1250 (155.5
    MHz) the board stopped answering twice and reset itself after about 40 s** (2026-09-02)
    [T&B § "2026-09-02 — the vertical ceiling, measured at last…"].
  - 2026-09-14: holding the clock at about 138 MHz, vtotal 1250 (ht 1842), 1500 (ht 1535) and
    **1600 (ht 1439)** all held with a picture. The 1920-wide/1250/155.5 MHz point that had reset
    the board **held for 70 s**, and nothing reset across 1096 s [T&B § "2026-09-14 — the series
    above changed two things at once…"].
  - Raising the memory clock (`PLL_MS`) to 162 or 216 MHz did not change the 1250 failure, so it
    is "not bandwidth". The clock reached 155.83 MHz and the board died at the video-path reset
    that followed [T&B § "2026-09-02 — can an overclock buy the band above 1200?…"].
- **Superseded early claims:**
  - "vtotal 1024 crashes the board" was overturned: 1024 gave a picture 2/3 with no crash
    [T&B § "`vtotal = 1024` crashes the board…" (superseded); § "The `vtotal` 1000 ceiling was
    from invalidated data"].
  - The "vtotal bands" seen with a 240p input (750/800 work, 820–900 do not, 1000 works) **moved
    when the input changed to 480p**, and later turned into a rate of about ⅓. They were
    per-apply chance plus instrument error, not a rule [T&B § "The apply procedure is sound…"; §
    "The input changed on its own mid-bench…"; § "The second phenomenon with 480p…"].
- **Modes the chain carried (2026-09-14),** each confirmed on its first picture:

  | mode | vtotal | clock | result |
  |---|---|---|---|
  | 1920x1080 control | 1111 | 138.3 MHz | 6 pictures |
  | 1920x1200 | 1235 | 153.8 MHz | picture |
  | 1600x1200 | 1235 | 130.1 MHz | picture |
  | 1600x1080 | 1111 | 117.0 MHz | picture |
  | 1500x1080 | 1111 | 110.1 MHz | picture |
  | 1440x1080 | 1111 | 106.4 MHz | 2 pictures, 2 black |
  | 1280x1080 | 1111 | 95.8 MHz | 0 of 3 (below the floor) |
  | 1280x960 | 988 | 85.2 MHz | 0 of 1 |
  | 1280x1440 | 1481 | 127.6 MHz | chip coherent, **HD60 black** |

  [T&B § "2026-09-14 — which modes the chain actually carries…"; § "2026-09-14 — the series
  above…"] **This conflicts with DSR's "1080 lines is the practical ceiling"**: two 1200-line modes
  carried a picture through the MS9288A to the HD60. See §10.
- **The capture card has its own limits.** It "will not display" 1280x960 (htotal 1440, 85 MHz)
  and is "fixed at 1920x1080" for 720p120 and 1080p24. The HD60 "has refused unusual shapes before"
  [T&B § "2026-08-30 — the aspect rule reached only the boot mode"; RTP § "Not evaluated, for want
  of hardware or material"; T&B § "2026-09-14 — the series above…"].
- **Receivers refuse in different ways.** At 1440x900, an out-of-range output showed "no signal /
  out of range" on a monitor while the capture card said black frame [T&B § "The symptom on the
  monitor is not what this document said"].

### 3.4 Vertical scaling, integer ratios and why 960 was the 240p default

- **The acceptance criterion for a base mode is not "gives a picture".** It is:
  - `outActiveLines / sourceActiveLines` within about **0.5% of an integer**;
  - `measuredOutputHz − measuredSourceHz` under about **0.001 Hz**.

  A mean/standard-deviation picture check is blind to line shimmer and slow tear bars
  [T&B § "2026-08-29, later — 1920x1080 reverted…"].
- **1080 lines from 480 shimmers.**

  | window | ratio from the 480 doubled lines | ratio from 240p |
  |---|---|---|
  | 1085 lines | ×2.2604 | — |
  | 962 lines (1280x960) | ×2.0042 | — |
  | 964 lines (first fix) | ×2.0000 | ×4.0167 |
  | **960 lines (86..1046)** | ×2.0000 | **×4.0000** |

  - "960 is not the historical default by accident: 240p × 4 and 480i × 2 are both exact."
  - The 1080p rate error first blamed (0.00729 Hz, a 137 s beat) was a reading taken before the
    clock settled. Settled, it was +0.00005 Hz, like 1280x960. The non-integer ratio alone caused
    the shimmer.

  [T&B § "2026-08-29, later — 1920x1080 reverted…"; § "2026-08-29 — 1920x1080 again…"; §
  "2026-08-29 — the source is 240p…"]
- **Integer scale ladder** (`VDS_VSCALE = 1024/N`, window = `N × srcLines`):

  | N | vscale | ratio error |
  |---|---|---|
  | 1 | bypass | exact |
  | 2 | 512 | exact |
  | 3 | 341 | +0.098% |
  | 4 | 256 | exact |

  - SD sources reach the scaler as 480 lines, so the "5x" of 240p is a 2.5× scale.
  - N=3 (1440 lines) was blocked at 1280/1440 wide by the guard alone (127.8/142.0 MHz). At 1920
    wide it needs 184.5 MHz (over the DAC limit).
  - N=4 (1920 lines) is beyond the DAC at every width.
  - A sweep of 208 modes: 176 accepted; the smallest scales seen were hscale 528 and vscale 455.

  [T&B § "The arithmetic it rests on"]
- **PAL cannot have ×2 inside the 1126-line array.** 576 × 2 = **1152**, more than vtotal 1126. It
  would need a taller vtotal (about 1160) or ×1 (letterboxed) [T&B § "PAL cannot have this, and the
  arithmetic says why"].
- **The PAL vertical scale was NTSC-shaped.** `VDS_VSCALE` 456 fills 1080 lines from 481 source
  lines, so PAL lost 95 of its 576.
  - Fix: derive it from `sourceActiveLines()`: 480 NTSC, 576 PAL, 0 for anything else.
  - The corrected values are 455 (NTSC) and 546 (PAL), with the PAL vertical centre at 486
    permille.
  - It regressed once when the base array stopped being recomputed. The fix is now anchored to the
    array (`vscaleForSource`).
  - PAL sweep: 455 cut the bottom; 544 and 546 complete; 589 brings junk in at the bottom.

  [T&B § "The defect: the vertical scale is NTSC-shaped…"; § "The fix: the vertical scale is
  derived…"; § "2026-09-24 — …"]
- **The upstream 1080 array is slightly inconsistent.** Its window is 1085 lines, but scale 455
  targets 1080 (480 × 1024 / 455 = 1080.4). Whether the 5 extra lines are intended is unmeasured
  [T&B § "The fix, and why it is anchored rather than computed"].
- **Integer scale changes only the window and the scale registers,** never the totals or the
  clock, so it cannot cause a rate mismatch. Ask for a taller timing first, then multiply inside
  it [T&B § "2026-08-29 — integer scale and stretch…"; § "Measured"].
- **A derived value that carries its own previous result forward cannot be tested by looking at
  it.** The same `VDS_VSCALE` 456 appeared in captures from three different sources: "a constant
  wearing a measurement's clothes" [FWD § "Two numbers that must come from the source, not from the
  last value"].

### 3.5 Horizontal scaling, aspect and where the sink puts the picture

- **One count of `VDS_HSCALE` tears the picture.**

  | state | hscale | edge spread (px) |
  |---|---|---|
  | boot | 625 | 0.43 / 0.00 (clean) |
  | apply 2 | 624 | 16.50 / 10.90 (torn) |
  | apply 3 | 623 | 18.10 / 10.33 (torn) |
  | back to | 625 | 0.42 / 0.08 (clean) |

  The aspect pass truncated both its divisions and walked hscale down about 2 counts per apply
  (623, 621, 619 …). The fix is to round both divisions and skip the pass when the window is
  already right [T&B § "2026-09-02 — the tearing was integer truncation…"; FWD § "`applySourceAspect()`
  must not re-derive from its own output"].
- **The sink centres the active area between consecutive hsyncs.**
  - Moving hsync by +64 moved the picture by −63 px.
  - A CVT-RB 1920x1080 window (htotal 2080, hsync 8..40, display 120..2040) landed **+40 units**
    right: half the blanking asymmetry.
  - Fix: `disHbStop = (htotal − hActive)/2`.
  - **Vertical:** the converter follows the standard. Do not centre it; shifting it made things
    worse.

  [T&B § "2026-08-30 — the answer: the generated mode's active window is not centred in the line"]
- **The converter places the picture itself.** It finds the active picture and places it
  ("Auto Position (Blank) Detection"), which also explains the black bars [DSR § "What the MS9282
  brief adds…"]. Each mode's own active area is mapped onto the same 1920x1080 frame: 0.969 px per
  unit on a 1982-unit window, against 1.000 on a 1920-unit one [T&B § "The 4% error reported
  earlier was a unit mistake, not the chip"].
- **Capture-card framing offsets** were about 109 px (left), 127 (right) and 31–32 lines
  (top/bottom). The 4:3 window sat 29 px left of centre on the card [BEH § "Trims…"; § "Two
  things the frames showed…"].
- **Aspect history:**
  - **The "three halves" constant was 7.46% too wide.** Measured against upstream by Sobel-profile
    scale fitting: horizontal 1.07554, vertical 1.00090; upstream itself is 0.21% from the
    DuckStation emulator.
  - A circle test could not see it, because a circle stays round under any equal scaling.
  - Constants fail here: output units are not square (on the 1602 raster, 1920 px span 1397 units,
    so 1 unit = 1.374 px), and the capture window's share of the line belongs to the array.
  - Final rule: **4:3 = the window the base program carries; 16:9 = four-thirds of it.** It is
    refused if it leaves under 32 px of blanking; on the boot mode 16:9 needs 1344 of 1602 units.

  [T&B § "2026-09-19 — the three halves are 7.5% wide…"; § "The first fix was a constant…"; BEH §
  "Source aspect — `geo.aspect`"]
- **`VDS_HB_*` offsets must be the array's own.**
  - On the old raster, `VDS_HB_ST = start + 4` let content wrap. The clean range was 1740–1810,
    with a band at ≥1830 and also at ≤1700, so the fix was `start − 60`.
  - On the 1602 raster the same constants were 80 units wrong (`VDS_HB_SP` 344 against the array's
    264, i.e. 110 px), and the picture came out wrapped.
  - Final rule: record the distance of each blanking edge from the display window as the array
    leaves it.
  - This also ended the "bistable boot race" saga: a wrapped band whose visibility varied from
    boot to boot.

  [T&B § "2026-08-30 — the left artefacts were the horizontal blanking start"; § "2026-08-30 —
  item 1 closed"; § "2026-09-19, on the board…"; BEH § "Source aspect…"]
- **Zoom only enlarges, additively (0–50%).**
  - Zooming below 100% tore the picture over "a band of uninitialised memory".
  - `wantH` must not exceed `htotal − 33`.
  - Zoom must multiply the *wanted* width before the window, fetch blanking and scale are derived.
    Multiplying `VDS_HSCALE` after the fact desynchronised `VDS_HB_SP` and tore every line.
  - Pitch check: pitch = 32768 / `VDS_VSCALE`. 455, 425, 396 and 506 gave 72.00, 77.00, 82.50 and
    64.75, against 72.00, 77.04, 82.80 and 64.80 predicted.

  [BEH § "Zoom — `geo.zoom`"]
- **Trims are masks.** Trimming moves only `VDS_DIS_*` and `VDS_EXT_*`; never the fetch blanking
  or the capture start [BEH § "Trims…"].

### 3.6 Si5351 pixel clock: generation, adoption, ramping

- **Where it plugs in.** The Si5351 feeds TV5725 pin 40, and the display PLL takes it **only while
  `PLL648_CONTROL_01` = 0x75** [DSR § "The rest of the board"; T&B § "2026-09-04…"].
- **Bootstrap problem.** The compiled presets carry internal codes (e.g. 0xA5) because they were
  "written for a board with no external generator". After loading, the firmware must **adopt**
  the generator.
  - Adoption saves the old value, **disables the Si5351 output for 800 µs** and writes 0x75. Do
    this only at a settled moment.
  - Adopting from inside the apply made the first-apply edge spread go from about 2.6 to about 13
    px. Adopting from the deferred pass gave black 5/5.

  [T&B § "2026-09-02, later — the display clock…"; FWD § "The external display clock is switched
  at a settled moment, on purpose"]
- **The adoption race.** The switch is consumed once per program load, by one of two loop
  branches.
  - The first branch fires when `continousStableCounter` is in **35–45** (or the watcher is off)
    and adopts. In clean restarts it adopted at **36**: "inside the window by one".
  - The second branch, past 45, clears the flag **without adopting**. The board then free-runs on
    the internal PLL at 162 MHz, i.e. **70.1 Hz on 2080 × 1111 (17% fast)**, until the next load.
  - Interlaced sources settle slower and hit the second branch.
  - A two-pass apply re-writes 0xA5 after the switch.
  - The 70 Hz state was also seen on one unassisted boot after OTA. The capture card happily locks
    at 70 Hz, so frames do not reveal it.

  [T&B § "2026-09-02 — one branch of two forgot to adopt…"; § "2026-09-04 — the free-run happens
  outside the bench…"; FWD § "The display-clock watchdog…"]
- **Their mitigation: a watchdog.** If the display has been off the generator for **10 continuous
  seconds**, with the watcher on, the generator present and no operation pending, it re-adopts and
  then re-presents the output once. A simulated miss was adopted at 10.2 s and re-presented at
  12.7 s; healthy applies flash through 0xA5 in under 2 s [FWD § "The display-clock watchdog…";
  RTP § "The automatic trigger, and the pass it was owed"].
- **Sensors lie when the generator is not in charge.** Pixel clock, refresh and line rates
  computed from the *programmed* Si5351 frequency are false while the chip is not on 0x75; they
  read 59.937 while the real output was 70.107. Gate them on 0x75, or measure real edges
  [SEN § "Source rates, derived"; RTP § "A hole this exposed in the existing sensors"].
- **Clock seeding at boot.** `getSourceFieldRate()` returns 0 before a picture exists, so seed
  `htotal × vtotal × nominal rate` (59.94 or 50). For 2080 × 1111 × 59.94 = 138 514 147 Hz, the
  board came up at **138 514 144 Hz**. The upstream bootstrap: approximate (table) clock first,
  exact clock once the source is measurable. The Si5351 default after detect-and-initialise was
  **81 MHz** [T&B § "Why: the clock has no way to bootstrap itself"; § "2026-08-30 — the two-stage
  clock bootstrap…"].
- **Ramping:**
  - The timing apply ramps the pixel clock in **250 kHz steps** [T&B § "2026-08-30 — the boot
    pipeline…"].
  - A revert from 64 MHz back to 108 MHz took **529 ramp steps**, a few per loop pass, and left the
    board "answering nothing for about two minutes". It was slow, not crashed: allow minutes after
    an apply or revert [T&B § "The unresponsive stretch after an apply has a measured cause"; SEN §
    "What the revert cost, and what the breadcrumb said"].
  - `setExternalClockGenFrequencySmooth` ramps in **1 kHz increments only for changes under 750
    kHz**. Larger jumps go to the Si5351 in one `setFreq` [T&B § "2026-09-15 — the frame lock could
    drive the clock past the DAC…"; FWD § "Why it hunts, in the algorithm"].
- **Clamp the clock, always.** A requested 50 Hz output from a 60 Hz source made the lock multiply
  the clock by 1.197 every pass (138.3 → 165.5 → 198 → 237 MHz), past the DAC's 162. The board left
  the network and needed the power pulled; this is "a hypothesis that fits, not a reproduction".
  The fix: `DAC_CLOCK_MAX_HZ`/`MIN_HZ` clamps, and the lock refuses ratios that need an
  out-of-range clock. The rate guards had only checked each rate was in 47–86 Hz [T&B § "2026-09-15
  — the frame lock could drive the clock past the DAC, and nothing stopped it"].
- **Disabling the generator** drops the board onto internal dividers at the next boot. The picture
  survives but free-runs at 70.1066 Hz [BEH § "Ignore the external clock generator…"; RTP §
  "Unlocked, and this is the finding worth keeping"].

### 3.7 Frame lock

- **With the generator, the lock is a ratio, not a phase loop.** The generator is set to
  `(sourceFieldRate / outputFrameRate) × currentFreq`, with a 5 s retry
  [T&B § "2026-09-14 — the rate you ask for…"; FWD § "The phase loop no longer runs where the
  generator is adopted"].

  | measured with the Si5351 adopted | value |
  |---|---|
  | input = output | 59.8251 Hz |
  | spread within a 16-sample burst | 0.025–0.14 µs |
  | walk between bursts | +0.066 to +0.094 µs/s |
  | relative error | **0.07–0.09 ppm**: one frame of slip every 49–70 hours |
  | PAL | 49.99919 Hz out against 49.99913 Hz source; periods equal to the tick (20.0003 ms = 3 200 054 ticks) |

  [RTP § "Locked, which is the default — 0.07 to 0.09 ppm"; T&B § "What holds"]
- **The phase loop hunted, and was removed.** `runFrequency()` used
  `correction = 0.0038 × latency_err_frames`, clamped per step to **0.06%** (`MAX_CORRECTION`
  0.0006), every 1.67 s, with no bound on the total.
  - With the lock on and nothing happening, the output wandered **59.820–59.973 Hz (0.256%)**: 36
    000 times worse than the ratio lock. The tick blocked 83–131 ms.
  - Across 240p↔480i menu changes it caused freezes of 2.10, 2.32 and 2.38 s; 615 of 5400 frames
    were black.
  - Isolated, the MS9288A *followed* a +83 kHz ramp (0.06%) and 1 kHz/2 kHz steps. The freeze came
    from the repetition or combination, not the step size, so lowering `MAX_CORRECTION` would not
    have helped.
  - Fix: no phase loop while the generator is adopted. The output then held 59.823 Hz with 0.000%
    range and a 1 ms tick, and the RAM it used was freed.

  [FWD § "The frame time lock hunts…"; § "Neither ingredient reproduces it on its own"; § "The
  phase loop no longer runs…"; § "The fix holds…"]
- **Without a generator** (inherited `FrameSync`) there are two methods:
  - Method 0 nudges output hsync timing and grows the vertical.
  - Method 1 adjusts the video clock directly, clamped to 0.06% of the input rate, because "a Dell
    U2312HM lose[s] sync on a 0.1% change — exactly the 59.94↔60 step".
  - "Some displays lose sync when the rate is nudged."
  - That path was **never measured** on their bench.

  [BEH § "Frame time lock — `enableFrameTimeLock`"; § "Lock method — `frameTimeLockMethod`"; FWD
  § "The fix holds…"]
- **Without any frame relationship** (no Si5351): the output free-runs 17% fast, beating at 10.28
  Hz (the phase wraps every 97 ms), and "the buffer is repeating or dropping a frame ten times a
  second" [RTP § "Unlocked, and this is the finding worth keeping"].

### 3.8 Latency findings

- **The instrument.** The input vsync and then the output vsync are timed on D6 through the test
  bus; the offset is "where in the output frame the input field sits", i.e. buffer occupancy.
  - It is **modulo one input period**, so it cannot see whole frames of buffering.
  - It costs 268 B of RAM and 1448 B of flash.
  - One sample blocks about 20–25 ms (100 ms deadline), so take one per loop pass and **never block
    in the network handler**.
  - It also measures the real output rate by counting edges, which works when the derived figures
    cannot.

  [RTP § "A5 — the buffer latency…"; SEN § "Buffer phase, and what it measures"]
- **The phase is arbitrary per boot but stable within one.** The same source and mode read 12.13
  ms (805 of 1111 lines) before a restart and 3.25 ms (216 lines) after. At 50 Hz PAL: 5.905 ms,
  σ 0.000 over 24 samples [RTP § "The absolute phase is not a property of the board"; T&B § "What
  holds"].
- **Interlaced inputs cannot be phase-measured.** Their vsync is two interleaved events, so the
  phase scatters over 225–1540 µs while the *rate* stays locked (59.9389 in, 59.9390 out). Guard
  any fitted drift figure: suppress it when the phase wraps within a burst, or when the spread
  between bursts exceeds ¼ frame (a false "−340 ppm" got through before) [RTP § "The latency
  instrument tells progressive from interlaced"].
- **Absolute end-to-end latency was never measured** (no 240 fps camera). RetroTINK "Buffer
  Length" parity holds in stability, not in absolute value [RTP § "Not evaluated: the absolute
  latency"; § "What this number is not"].
- **The old sampling code over-protected.** It disabled the watchdog, set WiFi light sleep, waited
  `delay(7)` and allowed a 3-million-iteration spin, which bought nothing: the spin cost 619 ms per
  sample when no edge came. A 100 ms deadline measures the same period (2 669 374–2 669 378 ticks)
  [FWD § "What the phase measurement's aggressive actions actually buy (2026-09-07)"].
- **240p↔480i switching costs nothing.** Not one of 6600 recorded frames lacked a picture, and
  there was no HDMI renegotiation [RTP § "B3 — the 240p/480i switch costs nothing…"].

---

## 4. Deinterlacing

- **Engagement is automatic, from the line count.** Even `VPERIOD_IF` means interlaced, and
  `MAPDT_VT_SEL_PRGV` = 0 means the motion-adaptive block is engaged; on a progressive source it
  reads 1 and the block is idle. "Bob" is "what the chip does with the RAM passes bypassed". The
  `MADPT_*` fields live in segment 2, and **measuring them on a progressive source measures a
  sleeping block** [RTP § "Session B — the deinterlacer, measured for the first time"; § "And half
  of that sweep was asked on a sleeping block"; BEH § "What the removal cost…"].
- **Motion-adaptive versus bob** (one register: `MAPDT_VT_SEL_PRGV` forced to 1 gives bob):

  | content | motion-adaptive | bob | change |
  |---|---|---|---|
  | still menu, vertical detail | 1.091 | 0.958 | −12% |
  | still menu, frame-to-frame difference | 1.069 | 1.806 | nearly double |
  | moving game, vertical detail | 1.203 | 0.871 | **−27.6%** |
  | moving game, V/H ratio | 1.061 | 0.607 | −42.8% |
  | moving game, top of vertical spectrum | 0.630 | 0.835 | +32.6% |

  In bob, HUD text is visibly sheared [RTP § "Motion Adaptive against Bob…"; § "The deinterlacer
  itself, finally measured in motion"].
- **The fields the two functions write.** `disableMotionAdaptDeinterlace()` writes
  `MAPDT_VT_SEL_PRGV`=1, `DEINT_00`=0xFF, `RFF_FETCH_NUM`=1, `WFF_ENABLE`=0, `RFF_ENABLE`=0,
  `WFF_FF_STA_INV`=1, `MADPT_Y_MI_OFFSET`=0x7F and `MADPT_Y_MI_DET_BYPS`=1. The enable function
  restores `DEINT_00` 0x19 and, unconditionally, **`RFF_FETCH_NUM` 0x80 and `RFF_WFF_OFFSET`
  0x100**: preset-shaped memory-path constants. A timing apply switched an interlaced source to bob
  and left it there until the sync watcher re-enabled the deinterlacer
  [T&B § "The first apply turns the deinterlacer off…"; § "2026-09-02 — found: it is the
  deinterlacer…"].
- **Deinterlacer plus missing FIFO flags tears the picture.** Re-enabling the deinterlacer on a
  computed mode gave **90 px of edge spread and 408 of about 972 rows displaced**. The cause was the
  **missing FIFO line flags**, which left the read FIFO marginally served, with the deinterlacer as
  the extra load. With the flags written last: 0–1 px and 0 rows, 4/4. Deriving `RFF_WFF_OFFSET`
  turned out to be unnecessary [T&B § "2026-09-02 — fixed: the boot mode wrote four registers…";
  FWD § "What the motion-adaptive deinterlacer writes…"].
- **Field inversion.** `VDS_FIELD_FLIP` is **inert**: it is the *output* field index, and the
  output is progressive. The RetroTINK's Field Inversion (reversing the fields of a 480i input) has
  no register equivalent here [RTP § "Field Inversion is not `VDS_FIELD_FLIP`"].
- **Linear/Blend has no counterpart.** `MADPT_VIIR_BYPS` and `MADPT_VIIR_COEF` belong to the
  vertical **downscaler** ("V-IIR filter in vertical scaling down"), and this path upscales.
  - Engaging it changed vertical detail −2.25% and horizontal −3.80%.
  - COEF 0–64 made no difference; **127 destroyed the picture** (a green smear).
  - `MADPT_SEL_PHASE_INI` ("vertical scaling down initial phase", proposed as a bob-phase control)
    was inert within 2.1%.

  [RTP § "Linear and Blend: real, tiny, and not what the name suggests"; § "Why: the datasheet
  says it belongs to the downscaler"]
- **Interlaced output: the candidate mechanism is inert.** `VDS_DIF_FR_SEL_EN`=1, `VDS_FR_SELECT`=1
  and `VDS_VSYN_SIZE1`=`SIZE2`=1113 were already set. Changing any of them left the output at 1111
  lines and 59.937 Hz, with a bit-identical vsync period (2 669 382 ticks).
  - **Not tried:** `VDS_DFIELD_EN`. It is critical; the datasheet says "field double mode, 50→100
    / 60→120, horizontal count halved, … output timing is interlaced". It changes the meaning of
    the totals.

  [RTP § "B2, interlaced output: the candidate mechanism is inert"; SEN § "The totals are
  total-minus-one…"]
- **Inverse telecine hardware exists but is switched off.** `MADPT_EN_PULLDOWN32` and
  `MADPT_EN_PULLDOWN22` read 0, `STATUS_DEINT_PULLDN` reads 0, and there are ID, offset, lock-reset
  and 2:2 threshold controls. It was never exercised, for lack of film material. The test design:
  enable a detector, watch `STATUS_DEINT_PULLDN`, and expect a large *rise* in vertical detail
  [RTP § "C1, and what the chip shows without it"].
- **Motion-sensitivity knobs.** `enableMotionAdaptDeinterlace()` writes `MADPT_Y_MI_OFFSET` = 0,
  and the threshold is `MADPT_MI_THRESHOLD`. Do not create a second owner for these registers [BEH §
  "What was looked at and left alone"].
- **The scanline effect and the deinterlacer share the same block.** Scanlines can therefore run
  only while the deinterlacer is idle (§5.1).

---

## 5. Picture processing

### 5.1 Scanlines: how this chip does them

- **There is no scanline generator.** The motion-adaptive line mixer is made to emit every source
  line twice, the second copy's luma "pulled down by an offset". At the same time the white level
  is lifted (`VDS_WLEV_GAIN`, `VDS_W_LEV_BYPS`).
  - The recipe sets `MAPDT_VT_SEL_PRGV` = 0 (the progressive source is treated as interlaced),
    `RFF_LINE_FLIP` = 1 and the three RAM passes on.
  - It uses **17 fields**: the `MADPT_*` and `RFF_*` group, `DIAG_BOB_PLDY_RAM_BYPS`,
    `MAPDT_VT_SEL_PRGV`, `VDS_WLEV_GAIN` and `VDS_W_LEV_BYPS`. Capture-and-restore covers 19
    registers.

  [BEH § "The effect — `scanlines`"; § "The four replacement blocks"; RTP § "B3, the last
  ride-along…"]
- **Gate.** NTSC/PAL standard, `VPERIOD_IF` odd (progressive) and deinterlacer idle. On 480i it
  does nothing, since the deinterlacer needs the block [BEH § "The effect — `scanlines`"].
- **Strength** runs 0..127 and is written as `MADPT_Y_MI_OFFSET = 0x7F − strength` (0x7F is off).
  The default 79 equals the old 0x30 [BEH § "Strength — `scanlineStrength`"].
- **Measured on PS1 240p at 1080p** (one source line is 4.5 output rows):

  | state | mean luma | amplitude at 4.5 rows |
  |---|---|---|
  | off | 153.3 | 0.35 |
  | strength 20 | 148.2 | 9.24 |
  | strength 79 | 112.4 | 36.05 |
  | strength 127 | 82.8 | 51.21 |

  [BEH § "Measured (PlayStation 1 in 240p…)"]
- **Restore what the program chose, not what you last wrote.** Capture the 19 registers at the end
  of every program load and write them back on toggle-off. Ten toggles returned every register
  exactly [BEH § "Capture-and-restore…"].
- **Pseudo-interlace is impossible.** The dark line does not alternate (phase step 0.000 rad on all
  23 frame pairs; amplitude 2.54 against 0.03 with the effect off).
  - Making it alternate would need `RFF_LINE_FLIP` toggled every output frame by the ESP at 59.8
    Hz with no jitter. A `loop()` pass is **89 ms at rest and has reached 707 ms**.
  - Black-frame insertion fails on the same arithmetic.

  [RTP § "B3, the last ride-along — the scanline effect does not alternate…"; § "Why this closes
  the item…"]

### 5.2 Sharpness, peaking, step response, filters

- **Peaking (`VDS_PK_*`)** raised median overshoot from 1.81% to **18.62%** at full strength and
  widened chroma bleed, so it stays **off** (`VDS_PK_Y_H_BYPS` = 1) [CAL § "What none of this
  fixed"; T&B § "2026-09-19, later…"].
  - Defaults seen: `VDS_PK_LB_GAIN` 22 [T&B § "2026-08-30 — user overrides live in their own
    file"].
  - `VDS_PK_LH_GAIN` 0x18 (identity 1) against 0x0A (identity 5) [T&B § "Four branches followed
    the identity across"].
- **Step response (`VDS_UV_STEP_BYPS`).** On, it narrowed chroma transitions from 4.25 to 3.31 px
  and raised real-frame chroma detail by 2–4%, at a 10% loss at the 1-pixel frequency. The final
  default was 1 (bypassed), "the operator's reference capture" reversing the earlier measurement on
  judgement [CAL § "`wantStepResponse`…"; T&B § "2026-09-19, later…"].
- **Output line buffer (`VDS_D_RAM_BYPS`).**
  - In the band where the source's lines live (0.20–0.30 cycles/px, i.e. one source line per 4.5
    rows), bypassing it (=1) brought vertical energy from half of the reference up to the reference
    (0.0133 → 0.0267 against DuckStation's 0.0260).
  - At the output raster's Nyquist (2.0-row period) it adds alternation: 0.1 → 4.8, overshooting
    the reference by 60%. At 4× magnification the frames showed "line structure, stepped edges and
    horizontal streaking".
  - They kept 1, as upstream does. Whether the Nyquist overshoot is *visible* is open.

  [RTP § "`VDS_D_RAM_BYPS` — large, and the wrong direction"; T&B § "What the same three frames say
  about the rest of the picture"; CAL § "`VDS_D_RAM_BYPS` — 0 becomes 1…"]
- **Softening controls (RetroTINK Anti-Alias LPF / Horizontal Blur): "should not be built".**
  `IF_HS_SEL_LPF` changes only 0.39% of horizontal detail on game content, and the line-buffer
  bypass degrades rather than softens [RTP § "Verdict" (softening)].
- **Chroma bandwidth is 0.81 of luma.** The chroma transition was pinned at **4.12–4.31 px** across
  seven filter combinations and the four `MADPT` chroma fields: "a chain limit" [CAL § "What none
  of this fixed"].
- **There is no colour bleed.** Chroma reached its full value within about 9 output px (two source
  px), while the luma edge moved 44 levels in one pixel [T&B § "What the same frame says about
  colour"].

### 5.3 Noise reduction: it wrecks the picture

- **The test** was the 240p Test Suite's Drop Shadow, which alternates every frame, so any temporal
  blend would collapse the alternation.
- **`VDS_NR_MIG_USER_EN` alone tears the picture apart.**

  | | shipped (bypassed) | with `VDS_NR_MIG_USER_EN` = 1 |
  |---|---|---|
  | level | 72.12 | 66.48 |
  | contrast | 67.53 | 51.22 |
  | alternation | 1.800 | 26.275 |

  The frame showed displaced streaks with the sprite gone. The effect is fully reversible, and the
  field was reclassified as critical.
- **Nothing else did anything measurable,** on 240p or on 480i with the deinterlacer engaged:
  `VDS_NR_Y_BYPS`=0, `MADPT_Y_NRD_ENABLE`, `MADPT_UV_NRD_ENABLE`, `MADPT_GM_NOISE_VALUE` and
  `MADPT_STILL_NOISE_VALUE` at 0–255, `MADPT_NOISE_EST_EN` and `MADPT_NRD_SEL`. All fell inside
  the noise.
- **Proposed mechanism.** The NR motion index comes from the MADI block. With no index it does
  nothing; forced to manual, it acts on a meaningless value.
- **Verdict: "noise reduction is not a feature that can be built here."** One caveat: most of these
  fields carry the "datasheet locus never exercised" mark.

[RTP § "The noise reducer, on the Drop Shadow test…" and its subsections]

### 5.4 Brightness, contrast, colour

- **Output-stage controls** [BEH § "Brightness memory — `pic.brightness`"; § "Contrast memory —
  `pic.contrast`"; § "Verified on hardware"]:
  - **Brightness** is **`VDS_Y_OFST`** (s3_3A, read as a signed offset; default 0).
  - **Contrast** is **`VDS_Y_GAIN`** (s3_35; default 128).
  - The OSD steps by 8 (three steps: 0→24 and 128→152).
  - Contrast used to move the *ADC* gain, which fought auto-gain. Keep input gain and output gain
    separate.
- **These are lost on every program load** unless re-applied. Changing the output resolution once
  "silently dropped every stored picture setting". Re-apply after the load, once the picture has
  been stable for 3 passes [BEH § "How a change takes effect"].
- **Colour matrix:** `VDS_UCOS_GAIN`/`VDS_VCOS_GAIN`/`VDS_USIN_GAIN`/`VDS_VSIN_GAIN` =
  **28/41/0/0** is already right. SMPTE bars land within 2.8 levels of nominal (mean error 1.12)
  [CAL § "What none of this fixed"].
- **Other stored picture controls** (each with a "keep the preset's value" sentinel 0xFF):
  peaking (LB/LH gain, coring, bypass), black-level expansion (level, gain, bypass), U and V gain,
  and "phosphor". Phosphor "drives eight fields as a recipe", including `VDS_C1_GAIN` (selective
  corrector) [BEH § "The other ten `pic.*`"].
- **Other untested picture blocks** in the register map: skin-tone, blue stretch, non-linear
  saturation, SVM, black/white expansion, correctors C1/C2 and PIP [REG group headings]. Whether
  `VDS_C1_U_LOW` and its neighbours are signed is unresolved [DSR § "One field is signed…"].
- **Grey tint.** A monochrome test pattern showed black exactly neutral, but **red falling behind
  as brightness rises**: R−G as a fraction of G was −6.31% (Y 60–80), −6.93% (95–115), −9.03%
  (140–180) and −9.47% (200–255); blue was high by under 2.4%. ADC gains were balanced (208 ×3), so
  this is "a gain shape", attributed to uncalibrated white balance [T&B § "What the same frame says
  about colour"].
- **Tone against an emulator** (median Y / pixels at Y≤2):

  | build | median Y | Y≤2 |
  |---|---|---|
  | fork, before | 9.9 | 10.84% |
  | upstream | 23.7 | 5.61% |
  | fork, after preset gain 0x7B | 36.0 | 0.00% |
  | DuckStation | 36.2 | 0.00% |

  The difference was the auto-gain ramp [T&B § "What the same three frames say…"; § "What the three
  symptoms measure now"].
- **Capture trap.** The HD60 encodes BT.601 but ffmpeg assumes BT.709. Greys stay right while
  saturated colour drifts, which produced a fake "34-level green compression". Decode with
  `in_color_matrix=bt601` [CAL § "The bench"; RTP § "The standing rules"].

### 5.5 The masking/border colour investigation (`VDS_UV_BLK_VAL`, s3_3F, signed)

- **It tints the picture more than the margin.**

  | sweep direction | margin | picture |
  |---|---|---|
  | 0x00→0x7F (positive) | goes magenta (left B +49.4, R +45.9; right B +71.7) | goes green (B 56.45→5.58, R 67.81→14.30); at 0x30 the whole title is green |
  | negative half | barely moves | G falls 60.40→21.00, about 40× more than the margin |

- **It cannot reach the pillarbox bars.** The bars inside the display window stay black; only the
  strip *outside* the display window is tinted. There is no luma counterpart (no
  `VDS_Y_BLK_VAL`), so it could never make a bright border. **Verdict: a masking colour is
  impossible.**
- **Hypothesis (not tested).** The MS9288A white-balances against the blanking interval, which
  would explain the opposite movement of margin and picture.
- **One sweep run lost the picture,** recoverable only by restart. The later reading: a downstream
  lock loss, not this register (§8).

[RTP § "A4 — the masking colour…" and subsections]

### 5.6 The MS9288A's own processing and clamping

- **It is a second front end, not a cable.** It is a "10-bit triple ADC plus HDMI transmitter with
  its own MCU and firmware". It does its **own auto phase, gain and offset**, has an enhancement
  block (black/white stretch, colour transition, dynamic peaking,
  brightness/saturation/contrast/hue) running at Macrosilicon defaults, and does "Auto Position
  (Blank) Detection" [DSR § "What the MS9288A datasheet does and does not settle"; § "What the
  MS9282 brief adds…"; SEN § "The HDMI converter is not on the bus"].
- **Black sits at 6 and white ceilings at 218.5,** whatever is done on the TV5725 side (luma
  offset, luma gain, ADC offset). "Both clamps are downstream of the GBS"; DSR calls this
  "consistent with a clamp" in the MS9288A [CAL § "What none of this fixed"; DSR (same)].
- **It wants explicit H and V sync.** The `useHdmiSyncFix` workaround patches the program on the
  way in: s0_44 bit 0 cleared and s0_49 bit 2 set. Per REG these are `DAC_RGBS_PWDNZ` (DAC off) and
  `PAD_SYNC_OUT_ENZ` (sync out disabled). It then drives `PAD_SYNC_OUT_ENZ` low after the path
  settles, so the converter sees sync appear cleanly. Analogue component output (YUV with 0.25 Vpp
  sync) was removed; output is fixed RGB/VGA [SEN § "The HDMI converter is not on the bus"; DSR;
  REG § "Pads and DAC"; BEH § "What the removal cost…"].

### 5.7 Pause and freeze

- **Freeze = `CAPTURE_ENABLE` = 0** (s4_21). The frame stays lit, showing the last captured frame
  [BEH § "Pause — `video.pause`"].
- **The sync watcher freezes and unfreezes at eight points,** and the post-load unfreezes, so a
  user pause must lock out unfreeze [BEH § "Pause — `video.pause`"].
- **Freezing alone keeps a corrupted last frame** on the DAC when the source disappears. Blank the
  DAC *and* stop sync after two agreeing "no source" probes, but **do not** reset the parameters:
  that loses `videoStandardInput` and the SOG tuning [FWD § "Source removal"].

---

## 6. Bypass modes

**Evidence is thin. The bench never had an HD, PC or RGBHV source**, so nothing here was measured
[FWD § "What is left of the waits, measured 2026-09-15"].

- **What exists:**
  - 720p, 1080i and 1080p are "passed through" (HD bypass; standards 5–7).
  - VGA to SXGA is passed through (13).
  - RGB/HV is either scaled (14, when `isValidForScalingRGBHV`, with `GBS_OPTION_SCALING_RGBHV`
    set) or passed through (15).
  - `GBS_PRESET_ID` reads 0x21 and 0x22 on the two bypass paths.

  [FWD § "The video standard, by name"; § "The preset identity the base program still carries"]
- **The register-map group "HD bypass output path"** has 27 fields at s1_30… (e.g.
  `HD_MATRIX_BYPS` bypasses YUV→RGB, `HD_DYN_BYPS` bypasses dynamic range). There is a reset bit
  `SFTRST_HDBYPS_RSTZ`. The output-side bypass switches are `DAC_RGBS_BYPS2DAC`, `OUT_SYNC_SEL`,
  `VDS_HSCALE_BYPS` and `VDS_VSCALE_BYPS` [REG § "HD bypass output path"; BEH § "The four
  replacement blocks"; T&B § "2026-08-30 — the datasheet on the reset controls…"].
- **In HD bypass the VDS registers are stale.** The video goes around the scaler, so the output
  registers describe the last preset; one panel showed 1602 × 1126 at 107.968 MHz while following a
  2345 × 259 source. Report "passthrough". The sync processor still measures the source
  [SEN § "Passthrough was reporting stale numbers"].
- **The bypass entry paths are separate code.** The HD-bypass path returns early inside the
  post-load preparation and enables the DAC itself, with its own synchronous sequence. The RGBHV
  switch (`bypassModeSwitch_RGBHV`) returns without the post-load. Upstream's only `PLL_MS::write(2)`
  calls (memory clock from FBCLK) are on bypass paths [FWD § "Clamp position and the dark hold"; §
  "The post-load waits are a machine"; BEH § "How a change takes effect"; T&B § "Raising the clock
  has to happen inside the apply"].
- **The RGB/HV branch still blocks.** It has 320/300/100/40 ms waits between PLL and SP writes, and
  three 30 × 10 ms line-count checks. Upstream's RGBHV `needPostAdjust` block hard-codes
  `IF_HB_ST2`, `IF_HB_SP2`, `IF_HBIN_SP`, `IF_VB_SP` 8 and `IF_VB_ST` 6, which collided with the
  fork's computed capture window [FWD § "What is left of the waits…"; T&B § "Two things were
  overriding the calculation"].
- **Recovering from upscale mode.** After a source reset, the measurements upscale mode was built
  on no longer hold, giving "a black screen, a wrongly coloured picture, or an output timing the
  display refuses". Drop back to plain bypass and re-classify from scratch; `applyPresets(15)`
  re-probes the sync type [FWD § "Upscale-mode recovery"].
- **When bypass is needed** (their docs only partly say this): HD inputs are passed through rather
  than scaled. On SD, upstream's 1080p array bypasses the horizontal scaler (`VDS_HSCALE_BYPS` = 1)
  to keep 1:1 sampling, which is the best horizontal detail they measured [T&B § "2026-09-19 — the
  three halves…"].
- **Not a bypass mode:** bypassing the IF line doubler (`IF_LD_RAM_BYPS` = 1) is worse for 240p
  (§2.8).

---

## 7. RetroTINK-4K parity conclusions

| RT4K function | verdict on this hardware | evidence | source |
|---|---|---|---|
| Deinterlacer: Motion Adaptive vs Bob | **exists, one register** (`MAPDT_VT_SEL_PRGV`) | −27.6% vertical detail in bob on moving content | RTP § "Motion Adaptive against Bob…"; § "The deinterlacer itself…" |
| Field Inversion | **no equivalent** | `VDS_FIELD_FLIP` is the output field index; inert on progressive output | RTP § "Field Inversion is not `VDS_FIELD_FLIP`" |
| Deinterlace Linear / Blend | **no counterpart** | V-IIR is on the vertical *downscaler*; −2.25%/−3.80%; coef 127 destroys | RTP § "Linear and Blend…"; § "Why: the datasheet says…" |
| Bob phase | **inert** | `MADPT_SEL_PHASE_INI` within 2.1% | RTP § "Why: the datasheet says…" |
| Interlaced output (B2) | **mechanism inert**; `VDS_DFIELD_EN` untried | `VDS_FR_SELECT`/`VSYN_SIZE*` change nothing | RTP § "B2, interlaced output…" |
| Pseudo Interlace | **impossible** | the scanline phase never moves; the ESP cannot toggle a bit at 59.8 Hz without jitter (loop 89–707 ms) | RTP § "B3, the last ride-along…" |
| Black Frame Insertion | **impossible** (same arithmetic) | — | RTP § "Why this closes the item…" |
| Smooth I/P switch (B3) | **exists, measured** | 0 blank frames in 6600 across 240p↔480i | RTP § "B3 — the 240p/480i switch costs nothing…" |
| Scanlines | **exists** (MADI line mixer) | amplitudes in §5.1; progressive SD only | BEH § "Scanlines" |
| Masking colour (A4) | **impossible** | `VDS_UV_BLK_VAL` tints the picture and never the bars; no luma field | RTP § "A4 — the masking colour…" |
| Anti-Alias LPF / Horizontal Blur IIR | **should not be built** | `IF_HS_SEL_LPF` −0.39%; line-buffer bypass degrades | RTP § "The softening control was measured before it was built…" |
| Noise reduction | **cannot be built** | only `VDS_NR_MIG_USER_EN` acts, and it destroys the picture | RTP § "The noise reducer…" |
| Inverse telecine (C1) | **not evaluated**; hardware present, disabled | `MADPT_EN_PULLDOWN32/22` = 0 | RTP § "C1, and what the chip shows without it" |
| Custom input modes | **exists, one slot** (not a CSV table) | the user-defined MD slot matches a learned V/H pair | RTP § "Custom input modes…"; BEH § "Custom input mode" |
| Per-console sampling (A1) | **impossible** to auto-detect | 256/320/640 are bit-identical in all 1536 bytes | T&B § "2026-09-06 — A1…" |
| Gen Lock / 50-60 auto (A. Buffer) | **yes**: 0.07–0.09 ppm; output = source rate | PAL 49.99919 against 49.99913 Hz | RTP § "A5…"; T&B § "What holds" |
| Buffer Length / latency (A5) | **relative only**: stability matched, absolute unmeasured | the phase is arbitrary per boot | RTP § "A5…"; § "What this number is not" |
| Vertical ceiling (A3) | 1200 measured, guard later 1600; "the real ceiling is the clock" | §3.3 | T&B § "2026-09-02 — the vertical ceiling…"; § "2026-09-14 — the series above…" |
| 720p120 / 1080p24 (A2) | **not evaluated** (the capture card is fixed at 1080p) | computed at 131.75 MHz / 54.5 MHz | RTP § "Not evaluated, for want of hardware or material" |
| Integer scaling | **exists**: window + scale only, N ≤ 2 inside 1080 for 480-line sources | §3.4 | T&B § "2026-08-29 — integer scale and stretch…" |
| Phosphor | closest thing is the `pic.phosphor` corrector recipe | — | BEH § "The other ten `pic.*`" |
| IR remote (E1) | **possible** on GPIO2, at the cost of the status LED; boot-window hazard | GPIO16 has no interrupt; GPIO15 must be low at boot; heap 16.0 KB against a 15 KB floor | RTP § "E1 — the pin inventory…" |

The 54.5 MHz figure for 1080p24 would sit below the 104.5 MHz sink floor found later, a point
their docs do not connect.

Their meta-lesson: **"an item is 'possible' only until someone measures it, and the measurement
is cheaper than the feature."** [RTP § "Verdict" (softening)]

---

## 8. Black-screen and no-picture failure modes

### 8.1 Black or no picture

| # | failure | trigger | what the chip shows | recovery / prevention | source |
|---|---|---|---|---|---|
| 1 | output starved | output pixel clock < ~1.13–1.16 × input pixel clock | output scans; sink shows black or "no frame" | widen htotal; warn below 1.20× | T&B § "The rule…" |
| 2 | per-apply coin toss | single-pass apply of a computed timing (cause unknown) | every config byte identical to a good state; `STATUS_VDS_VERT_COUNT` scanning | second pass after ~4 s; `VDS_VSYNC_RST` to another value and back rescues a settled black, but not as prevention | T&B § "The coin toss was per-apply…"; § "Bisecting the second pass" |
| 3 | geometry overwritten | post-load running after the timing | wrong vscale | order: program → post-load → timing | T&B § "`doPostPresetLoadSteps()` was rewriting…" |
| 4 | sink floor | pixel clock ≤ ~104 MHz on their MS9288A + HD60 chain | chip coherent; capture black | widen htotal past 104.9 MHz; floor is bench-specific | T&B § "It is the pixel clock, and nothing else" |
| 5 | converter lock loss | display clock dwelling on the internal PLL (0xA5) for a long time (t=1.6–19 s in the recipe: watcher off, apply, unconfirmed 20 s revert, watcher on); also seen during the `VDS_UV_BLK_VAL` sweep, after the B2 register restore, and once with nothing written | chip healthy; 14 output registers byte-identical; capture shows its own "No Video" placard; FIFO flags inconclusive | **re-present the output**: DAC power down + `PAD_SYNC_OUT_ENZ` disabled, held 3 s, DAC back, sync back 2 s later (3/3, ~5 s outage, occasionally needs a second call); none of 12 chip-reset attempts helped; restart also works; give the converter ~20 s to re-acquire; watchdog after a 10 s dwell | RTP § "A third black screen…"; § "2026-09-04 — reproduced on demand…"; § "The trigger, and a recovery…"; § "The recovery is in the firmware now…" |
| 6 | chained changes lock out the downstream link | 3 window changes in a row, or 3 applies in a row | chip healthy; converter and capture locked out (ffmpeg got no frame for 90 s) | restart; one change from a settled state | T&B § "One thing to know before using it"; § "Three applies and the capture card stops locking" |
| 7 | display clock never set | baked array carrying `PLL648_CONTROL_01` = 0x75 | no picture; Si5351 left at the 81 MHz default | two-stage bootstrap: table clock first, then 0x75 plus the computed frequency | T&B § "Why: the clock has no way to bootstrap itself" |
| 8 | baked array black after restart | same array, every restart after the first post-flash boot | HDMI link up, 70 frames black; 8 benign byte diffs; correct clock | unsolved: something the previous firmware left in the chip | T&B § "2026-08-30 — the two-stage clock bootstrap…" |
| 9 | no picture with blocks held in reset | `RESET_CONTROL_0x46` = 0x00 across the program write | no picture (8 boots) | do not hold the blocks off across the write | T&B § "Holding the blocks in reset…" |
| 10 | segment-0-only rewrite / clock re-init on a running board | rewriting segment 0 alone; re-running `externalClockGenDetectAndInitialize()` | no picture | — | T&B § "…bisecting the program write by segment…"; § "2026-08-30 — bisecting the bring-up…" |
| 11 | forcing 0x75 at the wrong time | writing `PLL648_CONTROL_01` = 0x75 on a running picture; adopting mid-apply or in the deferred pass; single-pass apply | black, or torn | adopt only at a settled moment (the stable-counter window) | T&B § "2026-09-02, later still — three routes to 0x75…" |
| 12 | full post-load from a torn state | `doPostPresetLoadSteps` as a recovery step | black | restart | T&B § "Nothing short of a restart recovers it" |
| 13 | mode the card refuses | 1280x960 at 85 MHz; 1280x1440 | chip coherent | — (card limit) | T&B § "2026-08-30 — the aspect rule reached only the boot mode"; § "2026-09-14 — the series above…" |
| 14 | wrong front-end clock after a format change | program loaded before the ADC/PLLs are prepared for the new format | "fading patterns": a decaying frame buffer | prepare 5_00, 0_40 bit 2, 0_43 bit 5 first, then wait out the settling | FWD § "Format-change classification" |
| 15 | grey fade-in | clamp never locked (`SP_NO_CLAMP_REG` forced 1) | black drifts up | always run the clamp-position step; hold the output dark until it locks | FWD § "Clamp position and the dark hold" |
| 16 | garbage left on screen | source switched off; `freezeVideo()` only stops capture | last, often corrupted frame held | after 2 agreeing probes, blank the DAC and stop sync | FWD § "Source removal" |

### 8.2 Board crashes, resets and network loss (video-adjacent)

| failure | trigger | outcome | prevention | source |
|---|---|---|---|---|
| clock runaway | 50 Hz output requested from a 60 Hz source; unclamped lock ratio 1.197/pass | clock past 162 MHz; board off the network; power pull | clamp the clock to the DAC range; refuse unreachable locks | T&B § "2026-09-15…" |
| vtotal 1250 @ 155.5 MHz | apply (2026-09-02) | HTTP and ping stop; self-reset after ~40 s with low heap | not reproduced 2026-09-14 (held 70 s) | T&B § "What 1250 does"; § "2026-09-14 — the series above…" |
| early "crash" reports (vtotal 741, 1024) | contaminated-instrument era | one crash took the `/p` filesystem directory and its saved profiles | later ~160 applies, no crash | T&B § "`vtotal = 1024`…"; § "No crash"; § "The board no longer crashes in this region" |
| "board dead" for ~2 min | revert ramp of 529 steps | slow, not dead | wait minutes; read the breadcrumb | SEN § "What the revert cost…" |
| heap exhaustion | 6 raw chip dumps in a row; constant polling | `HEAP_LOW` at 5 090 B, restarts every 40–80 s; OTA fails at ~15 KB idle heap (upstream 22 KB) | pace reads; flash over serial | T&B § "Two things the bench said…" |
| network loss on sleep | output powered down by the (removed) sleep timer | board left WiFi (22/26 polls failed); cause in the WiFi stack, unknown | not a video-path fault (manual DAC/sync power-down alone: 0/70 failures) | BEH § "The sleep timer was removed…" |

### 8.3 Picture corruption (not black, but the same family)

| failure | trigger | source |
|---|---|---|
| per-line horizontal jitter or displacement | FIFO line flags not (re)written after a path reset; deinterlacer load makes it worse (408 rows) | T&B § "2026-08-29 — the FIFO line flags…"; § "2026-09-02 — fixed…" |
| tearing (4–18 px edge spread) | `VDS_HSCALE` one count off (truncation) | T&B § "2026-09-02 — the tearing was integer truncation…" |
| wrapped band at the left edge | `VDS_HB_ST` after the window end (start+4), or `VDS_HB_SP` 80 units late | T&B § "…the left artefacts were the horizontal blanking start"; § "2026-09-19, on the board…" |
| every line torn | `VDS_HB_SP` not matching the captured span (zoom/trim implementations) | BEH § "Zoom…"; § "Trims…" |
| green bar / green noise | capture starting before active video (Y=U=V=0 shows as green); display window opened past the capture window | T&B § "What it was"; § "Two readings that were wrong on the way…" |
| frame torn in half, green bands | `IF_HSYNC_RST` 1279 on a mode sampled at 2345 | T&B § "But 1279 tears the frame…" |
| picture destroyed | `VDS_NR_MIG_USER_EN` = 1; `MADPT_VIIR_COEF` 127; `VDS_UV_BLK_VAL` ≥ ~0x30 | RTP §5.3/§4/§5.5 sections |
| band introduced | `PB_CAP_OFFSET` raised to 260/264 | T&B § "Six things in that sequence…" |
| 17% fast output (70.1 Hz) with a clean still image | display PLL left on 0xA5 (adoption race; watcher off during apply) | T&B § "2026-09-02 — the applied output runs 17% fast…" |
| shimmer | non-integer vertical ratio (1085 from 480) | T&B § "2026-08-29, later — 1920x1080 reverted…" |
| uninitialised memory band | shrinking the picture (zoom < 100%) | BEH § "Zoom — `geo.zoom`" |
| PAL bottom cut | `VDS_VSCALE` 455 on a 576-line source | T&B § "2026-09-24 — …" |
| unexplained stripes | PAL captures had 3 orange/teal stripes at columns 125–215, fixed while the picture moves (not diagnosed) | T&B § "A left-edge artefact, seen and not chased" |

**Detection limits.** "No register distinguishes a dead output from a live one." That held for
configuration bytes, status bytes and the FIFO flags. Two candidate self-detectors failed:
`CRC_REGOUT_RFF` (s0_22) and `STATUS_MEM_FF_CAP_FIFO_FULL`, the latter correlated on one bad boot
only [T&B § "A hypothesis the instrument killed quickly"; § "A detector bit that was not one"; RTP
§ "2026-09-04 — reproduced on demand…"; SEN § "Memory FIFOs"].

---

## 9. Measured numbers

| quantity | value | conditions | source |
|---|---|---|---|
| Input pixel clock, 240p NTSC | 39.7 MHz (2558 × 259 × 59.96) | old-preset sampling; line 15.53 kHz | SEN § "Source rates, derived" |
| Input pixel clock, 480p | 73.8 MHz (2345 × 524 × 60.043) | — | T&B § "The rule…" |
| Clock-margin boundary | 1.13–1.16 × input clock; warn < 1.20 | 480p input, 1280x724 active | T&B § "The rule…" |
| First good output clock against a 480p input | htotal 1900 → 85.6 MHz | vtotal 750 | T&B § "The rule…" |
| Sink floor | black ≤ 104.0 MHz, picture ≥ 104.9 MHz; floor constant 104.5 MHz | MS9288A + HD60; 1111-line rasters at ~59.85 Hz | T&B § "It is the pixel clock…" |
| Highest clock held | 160.8 MHz | vtotal 1111 | T&B § "2026-09-14 — the series above…" |
| DAC maximum | 162 MHz | DS-5725-3.2 Table 19 | DSR |
| MS9288A maximum sample/TMDS rate | 165 MHz | datasheet §2 | DSR |
| Vertical total held | up to 1600 (ht 1439) | ~138 MHz, 1280-wide window | T&B § "2026-09-14 — the series above…" |
| 1250 / 155.5 MHz | reset ×2 (2026-09-02); held 70 s (2026-09-14) | 1920 wide | T&B (same sections) |
| Memory clock shipped | 129.6 MHz (`PLL_MS` 7); 32-bit bus | overclock to 162/216 MHz made no difference | T&B § "The overclock exists"; § "The result" |
| Boot raster (1080p array) | 1602 × 1126 @ 107.968 MHz → 67.396 kHz, 59.854 Hz | NTSC source | SEN § "What the display is locking to" |
| CVT-RB 1080p (computed) | 2080 × 1111 @ 138.5 MHz (60 Hz); 2080 × 1106 @ 115.0 MHz (50 Hz) | — | SEN § "The timing engine gets the geometry right…" |
| PAL output on the 1080p array | 1602 × 1126 @ 49.997 Hz = 90.19 MHz; picture shown (bottom cut) | PS2 OPL 576i, 2026-09-24 | T&B § "2026-09-24 — …" |
| Total−1 error before the fix | output 0.14–0.26% slow | 8 computed modes | SEN § "The off-by-one is now fixed on both sides" |
| Derived vs measured refresh after the fix | 0.0033–0.0035% | 7 presets | SEN § "Swept across every supported resolution" |
| Frame lock (ratio, Si5351) | 0.07–0.09 ppm; 49–70 h per frame of slip | 59.8251 Hz in and out | RTP § "Locked…" |
| PAL lock | 49.99919 out, 49.99913 source; 20.0003 ms both | PS2 PAL | T&B § "What holds" |
| Internal-PLL free-run | 70.1066 Hz (162 MHz / 2080 × 1111); beat 10.28 Hz | generator disabled or not adopted | RTP § "Unlocked…" |
| Old phase-loop wander | 59.820–59.973 Hz (0.256%); freezes 2.10–2.38 s | lock on, generator present | FWD § "The frame time lock hunts…" |
| Phase-loop step limit | 0.06% per 1.67 s; gain 0.0038 | `MAX_CORRECTION` 0.0006 | FWD § "Why it hunts, in the algorithm" |
| MS9288A tolerance seen | follows +83 kHz ramps (1 kHz steps) and +1/+2 kHz steps | 138 MHz clock | FWD § "Neither ingredient reproduces it…" |
| Display-sync loss without a generator | Dell U2312HM loses sync at a 0.1% change | method 1 clamp is 0.06% | BEH § "Lock method…" |
| Buffer phase | 12.13 ms (805/1111 lines) vs 3.25 ms (216) across a restart; PAL 5.905 ms σ 0.000 | progressive | RTP § "The absolute phase…"; T&B § "What holds" |
| Interlaced phase scatter | 225–1540 µs | PS2 480i | RTP § "The latency instrument tells…" |
| Phase sample cost | ~20–25 ms (100 ms deadline; was 619 ms spin) | — | SEN § "Buffer phase…"; FWD § "What the phase measurement's…" |
| ESP tick | 6.25 ns (2 ticks = 12.5 ns) | vsync timing on D6 | FWD § "What the phase measurement's…" |
| Clock ramp in apply | 250 kHz steps; 529 steps for 64 → 108 MHz (~2 min) | — | T&B § "…boot pipeline…"; SEN § "What the revert cost…" |
| Smooth ramp | 1 kHz increments if < 750 kHz, else one jump | — | T&B § "2026-09-15…" |
| Adoption outage | Si5351 output off for 800 µs | `adoptExternalDisplayClock()` | FWD § "The external display clock is switched…" |
| Adoption window | stable counter 35–45; observed 36 | every clean restart | FWD § "The display-clock watchdog…" |
| Seeded clock | 138 514 144 Hz for a 138 514 147 Hz target | 2080 × 1111 × 59.94 | T&B § "…two-stage clock bootstrap…" |
| Si5351 default after init | 81 MHz | — | T&B § "Why: the clock has no way to bootstrap itself" |
| Apply passes | 1 pass 7/15; 2 passes 15/15; apply ~4 s → ~12 s | 18 s wait | T&B § "The comparison that decides"; § "The fix, and the detail…" |
| Edge-spread regimes | boot 0.00–0.21; first apply 1.07–2.61; accumulated 8.00–18.08 px | std-dev statistic; threshold 2.0 px; a person saw ~15 px | T&B § "The tear threshold is not calibrated…" |
| Deinterlacer + missing flags | 90 px spread, 408 of ~972 rows displaced → 0 | — | T&B § "2026-09-02 — fixed…" |
| FIFO line flags (1080p) | PB 45/0, RFF 36/52 | ntsc_1920x1080 (found independently twice) | T&B § "It confirms the FIFO line flags" |
| `PB_MAST_FLAG_REG` across presets | 24 (NTSC, vtotal 264–1067); 45 ntsc_1080; 34/38/40/43 PAL; 24 pal_1080 | — | T&B § "And there is no rule to recompute them with" |
| Centring sensitivity | `IF_HB_SP2` 1 unit = 2 px at hscale 512; `IF_VB_SP` ≈ 2.25 px/unit | 1080 output | T&B § "What the registers actually do"; § "The vertical, found the same way" |
| Sync widths | HLOW 216 (Mega Drive), 170 (PlayStation) | 240p | T&B § "The chip already measures the cause" |
| Centre fit | centre = 735 − 0.437 × HLOW (slope real, offset not) | old array | T&B § "The chip already measures the cause" |
| HDMI relock drift | centring zero 165.8 vs 149.4 (32 px) | same console, two sessions | T&B § "And here is what this bench cannot tell you" |
| Sink centring offset (CVT-RB) | +40 units (half of 120 vs 40 blanking) | hsync 8..40, window 120..2040 | T&B § "Where the window was" |
| `VDS_HB_ST` clean range | 1740–1810 (band at ≥1830 and ≤1700) | old 2080 raster, window 230..1850 | T&B § "…the left artefacts…" |
| Capture-card framing offsets | ~109 px L, 127 px R, 31–32 lines top/bottom | HD60 | BEH § "Trims…" |
| Integer vscale | N=2 512; N=3 341 (+0.098%); N=4 256 | `VDS_VSCALE = 1024/N` | T&B § "The arithmetic it rests on" |
| 240p in 960 lines | ×4.0000 (window 86..1046) | doubler ×2 then scaler ×2 | T&B § "2026-08-29 — the source is 240p…" |
| Vertical scale per standard | 455 NTSC, 546 PAL (456 → only 481 lines) | 1080-line window | T&B § "Measured on both standards…" |
| PAL ×2 | 1152 lines > 1126 vtotal (needs ~1160) | — | T&B § "PAL cannot have this…" |
| Aspect error of the three-halves constant | horizontal ×1.07554 vs upstream (7.46%); vertical ×1.00090 | Sobel profile fit, emulator reference | T&B § "2026-09-19 — the three halves…" |
| Output unit size | 1920 px over 1397 units = 1.374 px/unit | 1602 raster | T&B § "The first fix was a constant…" |
| Zoom pitch | 455/425/396/506 → 72.00/77.00/82.50/64.75 px | pitch = 32768/vscale | BEH § "Zoom…" |
| Mega Drive 224 vs 240 | 1010 vs 1078 output lines; cell pitch 72.00 in both | identical registers | BEH § "Zoom…" |
| Scanline amplitude | 0.35 / 9.24 / 36.05 / 51.21 (off / 20 / 79 / 127) | PS1 240p @1080p | BEH § "Measured (PlayStation 1…)" |
| MADI vs bob | −12% (still), −27.6% (motion) vertical detail | PS2 480i | RTP § "Motion Adaptive against Bob…"; § "The deinterlacer itself…" |
| NR destruction | alternation 1.800 → 26.275; contrast 67.53 → 51.22 | `VDS_NR_MIG_USER_EN` | RTP § "The isolation…" |
| Line-buffer bypass | vertical spectrum ratio +83.55%; 2-row bin 0.1 → 4.8 | 1080 from 240 | RTP § "`VDS_D_RAM_BYPS`…" |
| `IF_HS_SEL_LPF` | −0.39% horizontal detail (game); 84.06% vs 87.14% luma modulation (stripes) | — | RTP § "`IF_HS_SEL_LPF`…"; CAL § "`IF_HS_SEL_LPF`…" |
| Peaking overshoot | 1.81% → 18.62% | full strength | CAL § "What none of this fixed" |
| Step response | chroma transition 4.25 → 3.31 px | 1080p | CAL § "`wantStepResponse`…" |
| Chroma / luma bandwidth | 0.81; transition 4.12–4.31 px | 7 filter combinations | CAL § "What none of this fixed" |
| Colour matrix | 28/41/0/0 → SMPTE within 2.8 levels (mean 1.12) | — | CAL § "What none of this fixed" |
| Black / white at the HDMI capture | 6 / 218.5 (cannot be moved from the TV5725) | — | CAL § "What none of this fixed" |
| ADC phase | `PA_ADC_S` 74.84% vs best 74.88% of 16 | — | CAL § "What none of this fixed" |
| ADC gain | preset 0x7B (123); lower number = more gain; auto-gain ramp 0x48 → 0x94 (+100 s) → 0xA6 (+48 s) | cold boot | CAL § "What the cold boot does" |
| White-balance trim (one PS1) | gains 157/186/208 → grey spread 26.1 → 0.0 (MD: 1.0 with equal gains) | — | CAL § "What August measured…" |
| Red deficit | R−G −6.31% … −9.47% of G from Y 60 to 255 | monochrome pattern | T&B § "What the same frame says about colour" |
| Masking field effect | picture B 56.45 → 5.58, R 67.81 → 14.30 at 0x7F | `VDS_UV_BLK_VAL` | RTP § "It tints the picture harder…" |
| 240p↔480i switch | 0 blank frames of 6600 | 240p Test Suite | RTP § "B3 — the 240p/480i switch…" |
| User mode match | V 32 (31–32), H 173–174 (172–174) | MD 480i, `VPERIOD_IF` 524 | RTP § "Measured on the bench"; BEH § "Measured" |
| Console horizontal modes | srcHTotal 2345, HPERIOD_IF 428, HLOW 170, 15.554 kHz for 256/320/640 | PS1 | T&B § "2026-09-06 — A1…" |
| Mega Drive raster | `VPERIOD_IF` 525, srcVTotal 260, 15.584 kHz, 36 543 628 Hz | 224/240p | BEH § "Zoom…" |
| PAL source | `STATUS_00` 0xA7, `VPERIOD_IF` 624, 310 lines/field, 15.499 kHz | PS2 | T&B § "What holds" |
| 240p → 480p readings | VTOTAL 259 → 524; HPERIOD_IF 430 → 214; HTOTAL 2558 → 2345; VPERIOD_IF 523 → 524 | same preset family | T&B § "The input changed on its own…" |
| Program write | 432 bytes, 16-byte chunks, ~0.5 s of I2C | — | T&B § "Why the program write…"; FWD § "The post-load waits…" |
| Post-load waits | sync ≤ 2002 ms; mode ≤ 1505 ms; SOG settle 300 ms; clamp 25 × 20 ms | — | FWD § "The post-load waits are a machine" |
| Loop pass | 89 ms at rest; 707 ms worst (watcher 702–706 ms) | old firmware | RTP § "Every pin of the D1 mini…"; FWD § "What is left of the waits…" |
| Heap | idle ~15–16 KB (floor 15 KB); `HEAP_LOW` at 5 090 B | — | RTP § "What is still owed…"; T&B § "Two things the bench said…" |
| Supply rail | 3.47 V (ESP 3.3 V rail) | — | SEN § "The supply rail…" |
| Preset/compare statistics | 31 non-VDS fields differ between presets; 508/40/6/66 field split; 74/432 bytes NTSC vs PAL | — | T&B § "The design conclusion"; § "What to do next"; § "What the two arrays actually disagreed about" |

---

## 10. Open questions they left

**Timing engine and clocks**

1. **Why does a second apply pass cure a black screen?** It is known to work (15/15), and a
   settled black is rescued by toggling `VDS_VSYNC_RST`, yet nothing readable differs. The likely
   shape is a counter that reloads only when the value changes, on a frame edge, but that is
   unconfirmed [T&B § "Where this leaves things"].
2. **What made vtotal 1250 / 155.5 MHz reset the board on 2026-09-02** but not on 2026-09-14? The
   lead was "what `timingResetVideoPath()` touches that a 17 MHz jump invalidates" [T&B § "What this
   leaves"; § "2026-09-14 — the series above…"].
3. **Why did 1280x1024 fail (1 of 3, 1 of 4)** with the same 30–31 lines of blanking as 1080p?
   Possibly the sink floor (91 MHz), but never re-tested [T&B § "The clock margin does not explain
   the 1280x1024"].
4. **What is the sink floor really a property of** (MS9288A or HD60), and does it depend on line
   rate? The PAL 1602 × 1126 @ 50 Hz output at 90.19 MHz *did* show a picture on 2026-09-24,
   although it is below the 104.5 MHz "floor" measured on 60 Hz rasters [T&B § "It is the pixel
   clock…"; § "What this costs"; § "2026-09-24 — …"].
5. **What does the MS9288A really accept above 1080 lines?** DSR concludes 1080 is "the practical
   ceiling of this chain", but 1920x1200 and 1600x1200 carried pictures, and 1280x1440 went black
   with "board or card … not separated" [DSR § "What the MS9282 brief adds…"; T&B § "2026-09-14 —
   …"].
6. **What slew or excursion does the MS9288A tolerate before it drops lock?** This would be needed
   to restore phase-locked (chosen-latency) frame lock [FWD § "The phase loop no longer runs…"].
7. **The display-clock adoption race**, i.e. the second loop branch clearing the flag without
   adopting, remains a "live suspect", now mitigated only by the watchdog [T&B § "2026-09-04 …";
   § "What is and is not claimed"].
8. **Should the frame-lock call sit inside the `enableFrameTimeLock` test?** The ratio lock
   currently always runs [T&B § "2026-09-14 — the rate you ask for…"].
9. **The path without a generator** (`runVsync`/FrameSync methods) is unmeasured. Audio dropouts
   during the old lock freezes were never measured [FWD § "The fix holds…"].
10. **Why does a baked array give a picture on the first boot after flashing** and black on every
    restart, with the register state matching to 8 benign bytes? [T&B § "2026-08-30 — the two-stage
    clock bootstrap…"]

**Geometry and PAL**

11. **PAL coverage is still thin.** The 18 array-only registers were never checked for PAL. The
    576p capture window and PAL 288p were never measured. The 541-permille fallback is unexercised,
    and whether the sync-width fit holds at 50 Hz is unknown. Note that T&B's 2026-09-24 entry
    calls itself "the first PAL source", although D1 (2026-09-06) measured one [T&B § "2026-08-30 —
    what is left open"; § "What this does not touch"].
12. **Is the 1085-line window against a 1080-line target** in upstream's 1080p array intentional?
    [T&B § "The fix, and why it is anchored…"]
13. **Horizontal detail is still about ¾ of upstream's** after the 1:1 array change [T&B § "What
    the three symptoms measure now"].
14. **The centring model is a well-motivated hypothesis**, not a rule: the sync-width slope is
    from two consoles, with one in-session validation [T&B § "And here is what this bench cannot
    tell you"].
15. **The PAL left-edge stripes** (columns 125–215) were not diagnosed [T&B § "A left-edge
    artefact…"].
16. **Which physical jack maps to `ADC_INPUT_SEL` 0/1/2?** Their docs never say.

**Features and picture**

17. **Interlaced output via `VDS_DFIELD_EN`** was never tried [RTP § "B2…"].
18. **Inverse telecine** was never exercised [RTP § "C1…"].
19. **Absolute end-to-end latency** was never measured [RTP § "Not evaluated…"].
20. **Is the line-buffer bypass's Nyquist overshoot visible?** [T&B § "What the same three frames
    say…" (2026-09-24 note)]
21. **The auto-gain loop needs redesigning** (lower as well as raise; start from the preset value)
    before it can be on [CAL § "What the cold boot does"].
22. **Signedness of `VDS_C1_U_LOW` and its neighbours** is unresolved [DSR § "One field is
    signed…"].
23. **Is the MS9288A white-balancing against the blanking interval?** That would explain A4 [RTP §
    "A hypothesis this raises…"].
24. **Why does the input PLL lock bit dither?** [SEN § "Locks"]
25. **The tear threshold was never calibrated against a human eye** (the one sighting was ~15 px)
    [T&B § "The tear threshold is not calibrated…"].
26. **What is the cause of the WiFi drop when the output sleeps?** [BEH § "The sleep timer was
    removed…"]
27. **Do 240p↔480i changes black the output?** RTP's B3 found 0 blank frames with no program
    reload. FWD's frame-lock control recordings saw 0.23 s / 0.93 s dark blocks per change,
    attributed to "the preset reload", on a different source. Their detector cannot tell black
    output from black content, so this is unresolved [RTP § "B3…"; FWD § "The fix holds…"].
28. **HD bypass, RGBHV and PC modes were never put on the bench** [FWD § "What is left of the
    waits…"].

---

## Appendix A: measurement-method lessons (what cost them weeks)

1. **Check that the effect's precondition still holds after measuring.** The apply was queued and
   auto-reverted after 20 s, so captures photographed the *preset* and counted it as success. "No
   success rate published before this fix holds." [T&B § "The instrument error that invalidated all
   the earlier measurement"; § "Method: the pattern that produced eight errors"]
2. **Measure the rate before looking for a pattern.** With a ⅓ success rate, three good results in
   a row come up once in thirteen. One run of an intermittent system is an anecdote [T&B §
   "Method…"; § "What this cost, and the rule it earns"].
3. **"A mechanism that explains the measurement is not the same as a remedy that works."** The
   `VDS_VSYNC_RST` toggle rescued 3/3 but prevented nothing (2/5) [T&B § "Method…"].
4. **Suspect the check script before the firmware.** A script encoding the wrong rule produces
   intermittency indistinguishable from a defect [T&B § "Method…"].
5. **Summary statistics lie.**
   - A mean/standard-deviation "BARS" verdict passed shimmer, tearing and a frame torn in half.
   - A lit bounding box measured artefacts and game content.
   - A column average reads zero on a checkerboard.

   **Look at the frame.** [T&B § "But 1279 tears the frame…"; § "The instrument said BARS"; §
   "2026-09-06 — A1…"; RTP § "The standing rules"]
6. **Every number is only meaningful for the input it was taken on.** That applies to geometry,
   refresh and HTOTAL alike. Record the source with every measurement; a stale refresh (59.823
   against 59.937 Hz, 0.19%) invalidated a whole campaign [T&B § "2026-09-01, later — it was never a
   null test"].
7. **Writes can fail silently.** Critical fields answered 409 and the helper swallowed it. Print
   the read-back beside every write [T&B § "Two instrument errors…"; § "PLLAD_MD does nothing on
   its own"].
8. **Sweeps of FIFO and buffer registers need a path reset between points,** or they measure
   nothing [T&B § "And a method note that invalidates earlier probing"].
9. **Use the branch point as the control.** Flashing the last good commit separates "this does not
   work" from "my change broke this" [T&B § "What this cost, and the rule it earns"].
10. **An "ESP restart" is not a power cycle.** The TV5725, Si5351 and SDRAM keep their state across
    `ESP.restart()` [T&B § "2026-08-30 — every "boot" in this file was an ESP restart…"].
11. **Capture-card details.**
    - Sampling at 2 fps cannot see a 2-frame period; capture at the full rate.
    - A 60 fps capture against a 59.94 Hz output adds about 0.4 repeats per 64 frames on its own.
    - ffmpeg takes about 1.4 s to open the device before frame 0.

    [RTP § "The trap this measurement was built to avoid…"; T&B § "What this bench had never done
    until now"; RTP § "Two ways this measurement nearly went wrong"]
12. **Keep the bench from becoming the fault.** Turning the sync watcher off during an apply
    (which every bench script did, and no user does) produced the 17% free-run. Constant polling
    and raw dumps exhausted the heap [T&B § "2026-09-02 — the 17% rate error is a bench artefact";
    § "Two things the bench said that belong in the record"].
