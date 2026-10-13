---
name: investigate-register
description: Studies a TV5725 register field or chip behaviour safely — datasheet page, known measurements, current value on the board, a reversible experiment and a visual verdict. Use when debugging the video path, when a setting does not seem to act, or before exposing a chip capability as a new setting.
---

1. **Locate it.** Find the field in `schema/registers/tv5725.yaml` (segment, address, bits, access)
   and the register in `docs/reference/register-map.md`, which gives the RD-5725-1.1 page. Read that
   page (`docs/hardware/pdf/`, fetch with `uv run tools/datasheets.py`). The text extraction
   misaligns bit descriptions: match by meaning, not position.
2. **Check what is already known.** Search `docs/research/gbs-control-lessons-videopath.md` and
   `docs/research/gbs-control-mechanics.md` for the field name and its block. Note who writes it
   automatically (detector, loader, watcher…): a value set by hand may be overwritten by them.
3. **Read it on the board** with the console: `expert on`, then `reg get <FIELD>`. Record the input
   signature (`status`).
4. **Experiment reversibly.** Write one value, observe, write the original back. Change one thing
   at a time from a settled state; FIFO and memory fields act only after a video-path reset, so say
   whether one happened. Never write `S0_00..S0_2F` (read-only status) — the chip ignores it.
5. **Judge visually** with the `bench-verify` skill; a register that reads back correctly proves
   nothing about the picture.
6. **Write down the result** in `docs/bench/` (field, values, signature, verdict, capture paths),
   and link it from any design doc or setting that relies on it.
