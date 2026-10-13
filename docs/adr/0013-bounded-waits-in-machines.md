# 0013. Long waits are machine states; short bounded waits may block

- Status: accepted
- Date: 2026-10-13
- Refines: [0003](0003-main-loop-owns-the-bus.md)

## Context

The reference project banned `delay()` in chip sequences because, with the asynchronous web server,
any `delay()` or `yield()` let a network handler run in the middle of a sequence and touch the chip
([lessons P1, P4](../research/gbs-control-lessons-platform.md)). ADR 0008 replaced that server with
the synchronous `ESP8266WebServer`, whose handlers only run when the main loop calls
`handleClient()`. A `delay()` inside a machine step still yields to the Wi-Fi stack (which the
ESP8266 needs), but no handler of ours can run there, so the reentrancy hazard is gone; what remains
is latency for the web page, the console and the OLED.

The TV5725 bring-up has many waits: detection windows of up to 6 s, a 2 s settle, 25 clamp tries
20 ms apart, a 260 ms sync-processor settle in the discrete-VSync probe, sub-millisecond latch pulses
([mechanics §2–5](../research/gbs-control-mechanics.md)).

## Decision

- Any wait of 50 ms or more is a machine state with a deadline, advanced by the 20 ms video tick.
- A single step may block for at most 50 ms in total (latch pulses, a register settling, a short
  measurement such as a vsync period with its 100 ms deadline counted as one step's budget when it
  is the only thing in that step).
- The scheduler records each task's longest run; the longest loop pass is published in diagnostics
  and must stay well under the 3.2 s software watchdog (target: under 250 ms).
- Network handlers still never call into `video/`, `chip/` or `hal/`; they enqueue commands (ADR 0003
  stands for structure and testability).

## Consequences

The bring-up code can follow the proven sequences closely without contorting every millisecond
pause into a state, while the interfaces stay responsive. Steps that exceed the budget show up in
the scheduler statistics and get split.
