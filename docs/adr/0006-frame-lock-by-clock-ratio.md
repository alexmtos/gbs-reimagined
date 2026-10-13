# 0006. Frame lock by Si5351 clock ratio, no continuous phase loop

- Status: accepted
- Date: 2026-10-13

## Context

With the Si5351 driving the display clock, the reference bench held output = input rate to
0.07–0.09 ppm by setting `clock × sourceRate / outputRate` once. A continuous phase loop on top of
it hunted (output wandering 0.256 %) and froze the picture for over 2 s on 240p/480i changes
([video-path lessons §3.7](../research/gbs-control-lessons-videopath.md)).

## Decision

Frame lock re-establishes the clock ratio when the rate relationship is missing or after a source
change, and otherwise leaves the clock alone. The clock is always clamped to 20–162 MHz. Changes
under 750 kHz ramp in 1 kHz steps; for those the Si5351 driver rewrites only the PLL fraction,
never the MultiSynth divider, so no PLL reset glitches the output.

## Consequences

Latency (buffer phase) is stable within a boot but arbitrary between boots; it is reported, not
controlled. A source whose rate drifts slowly is tracked only at re-lock events; the watcher's rate
check bounds that drift.
