---
name: bench-verify
description: Verifies on the real GBS-C, through the NZXT Signal HD60 capture card, that the scaler produces the expected picture — captures frames, separates rig faults from black output, records the input signature and the board status with each result. Use for milestone gates, after any change to the video path or a picture setting, and whenever a register-level result needs a visual verdict.
---

A coherent register set is not a picture on screen; the verdict comes from the capture card.

1. **Preconditions** — check and report each:
   - The firmware running is the one under test (`flash-board` step 4).
   - The board's status headline is `scaling` and `/api/status` (or console `status`) shows the
     expected source: record port, sync type, standard, lines, H/V frequency. This is the input
     signature; a result without it is not a result.
   - Wait for the HDMI chain: after any mode change or reset, wait until status is stable **and**
     20 s more (the MS9288A and the HD60 each re-lock on their own time).
2. **Capture:**
   `uv run --group bench tools/bench/capture.py --frames 30 --label <short-name>`
   It writes `bench-runs/<timestamp>-<label>/` with PNGs, `result.json` and the board status.
   Exit codes: 0 picture, 1 black frame, 2 no frames from the card (rig fault — reconnect the
   card or close the other program holding it; do not blame the firmware).
3. **Judge:** look at the saved frame yourself (Read the PNG). For the 240p Test Suite bars, also
   run `uv run --group bench tools/bench/capture.py --check bars` once that check exists (M1).
   State what you see, not only the metrics.
4. **Record:** for milestone gates, add the run (date, firmware version, signature, verdict, PNG
   path) to `docs/bench/<milestone>.md`.

Method rules (each cost the reference project days): repeat a flaky result enough times to know
its rate; interleave the conditions you compare; after measuring, confirm the state you meant to
measure was still applied (no revert happened); suspect the script before the firmware.
