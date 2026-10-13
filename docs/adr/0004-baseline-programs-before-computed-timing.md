# 0004. Fixed baseline programs first; computed timings later and optional

- Status: accepted
- Date: 2026-10-13

## Context

Two approaches exist for output modes. The upstream firmware loads one fixed program per output
resolution and tunes it; it runs on thousands of boards. The reference fork replaced this with a
single base program plus computed (CVT-RB) timings, which needed a two-pass apply whose cause was
never found, a pixel-clock floor relative to the input clock, and hand-tuned FIFO flags
([video-path lessons §3](../research/gbs-control-lessons-videopath.md)).

## Decision

Milestone M1 offers output modes from the 12 baseline programs (NTSC/PAL × 960, 1024, 720,
480/576, 1080, 15 kHz) plus HD and RGBHV bypass, with the proven post-load tuning. Computed or
custom timings are an M2 feature behind an "experimental" setting, built on what M1's bench
measures.

## Consequences

The first release reaches parity with the mature firmware's picture quickly. Custom resolutions and
integer-ratio modes beyond the programs wait for M2 and for measurements on this bench.
