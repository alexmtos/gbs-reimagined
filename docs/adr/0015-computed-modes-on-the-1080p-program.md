# 0015. Compute the extra output modes from standard timings on the 1080p program

- Status: accepted
- Date: 2026-10-14

## Context

[ADR 0004](0004-baseline-programs-before-computed-timing.md) left computed output timings for M2,
behind an "experimental" label and built on what the bench measures. Forces:

- **The reference's computed timings were fragile [reference bench].** The fork built every
  computed mode on the 1080p array. One apply gave a picture 7 times in 15 and two applies 15 in
  15, for a reason never found. Its FIFO flags were hand-tuned per program, and it reported a
  sink floor near 104 MHz ([videopath lessons](../research/gbs-control-lessons-videopath.md)
  §§3.1–3.3).
- **The chip and the board [vendor].** The DAC is rated at 162 MHz (RD-5725 Table 19). The
  internal display clock has fixed codes from 40.5 to 162 MHz (p.81), and there is no
  output-polarity bit: a negative sync pulse is written as ST > SP (mechanics §6.2). The
  horizontal scaler only enlarges.
- **The frame lock** ([ADR 0006](0006-frame-lock-by-clock-ratio.md)) makes the output rate equal
  the source rate. The generator's clock therefore follows the raster's total, while the line's
  duration stays the same.
- **The bench spike** ([bench M2](../bench/M2.md), "Custom timings: bench spike"). Every computed
  raster applied once, frame-locked, and showed a correct picture:
  - 1080p, 720p, 1280x960 and 1280x1024 rebuilt from their standards match the programs.
  - 1024x768, 800x600, 1600x1200 and 1920x1200 are new modes.
  - The bench TV reported 1920x1200 in its own information display: the HDMI converter passes
    such modes through.
  - The only defect was a stray column at a horizontal divisor of 448, the limit already known
    below 512.
  - The same output line drawn in more units, at a faster clock and the same duration, let 1080p
    narrow below 1:1.
- **The user decided on 2026-10-14:**
  - a fixed list of modes, no free editor;
  - the modes offered in the resolution list, marked "(exp.)";
  - the longer line used whenever a picture needs it at 1080p: PAL 4:3, and integer scale.

## Decision

1. **Four extra modes** in `output.resolution`, appended so stored indexes keep their meaning:
   1024x768 and 800x600 (VESA DMT, −/− and +/+ syncs), and 1600x1200 and 1920x1200 (CVT reduced
   blanking, +H/−V), labelled "(exp.)".
2. **Each is computed onto the 1080p program of the source's family**, after its post-load. The
   loader writes:
   - the totals (register = total − 1);
   - the sync pulses at the start of the line and frame, with ST > SP for a negative pulse;
   - `VSYN_SIZE`;
   - a geometry baseline whose window is the active area.

   Aspect, integer scale, position and size then apply as for any program, and the 1080p
   program's capture, FIFO flags and PLLAD values stay as they are.
3. **The line is drawn for an internal clock code**, so the chip runs the mode at the family's
   rate (60 Hz, or 50 Hz for PAL sources) before the generator takes over, and also on a board
   without a generator. The loader picks the fastest code that still leaves a 4:3 picture a
   horizontal divisor of at least 683. That keeps a 16:9 picture at or above 512. When no code
   reaches 683 the slowest one that still fits the capture is used; when none fits, the 1080p
   program stays as it is. The frame lock then trims the generator to the source.
4. **A longer line for 1080p** when the picture the program's line holds would be more than 2 %
   too wide:
   - PAL at 4:3 (about 10 %);
   - integer scale (12.5 % for 240p).

   The program's own layout is scaled to the same line in more units, on the lowest faster code
   that leaves the divisor at least 1 % under 1:1. NTSC 4:3 asks for 0.15 % and stays as it is. A
   change of aspect or integer scale that crosses this point reloads the program.
5. **Bench-verified only on this board, the HD60 and one TV.** The labels say "(exp.)" until
   other displays have been tried.

## Consequences

- The four modes and the longer line share one code path in the loader (`video/raster.*`, pure and
  host-tested; `Engine::applyLineShape`). Adding a mode is one row in the timing table plus a
  value in the schema.
- 1080p with PAL sources now shows an exact 4:3 picture, at 108 MHz instead of 81 MHz. With
  integer scale, NTSC runs at 129.6 MHz and PAL at 130 MHz.
- A reduced-blanking mode starts the fetch during the sync, because its back porch is shorter
  than the program's fetch lead. It showed no defect, but has not been watched for long.
- With a PAL source the modes run at 50 Hz, frame-locked. At 50 Hz they are not VESA timings, and
  the HDMI converter shows them about 7 % narrower and 2.5 % to the right; the same mode at 60 Hz
  (Force 60 Hz) is exact ([bench M2](../bench/M2.md), "Computed modes"). The user chose to keep
  50 Hz and document the remedy (2026-10-15) rather than force 60 Hz or compensate for this
  converter.
- Not measured: modes on displays other than the bench TV; the VGA output to a monitor.
- A display that rejects a mode leaves the screen blank. From the web or the console the timed
  revert brings the previous mode back; on the OLED the change confirms itself, as for every
  output change there.
