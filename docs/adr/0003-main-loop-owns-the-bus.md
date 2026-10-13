# 0003. Only the main loop touches I2C and the filesystem

- Status: accepted
- Date: 2026-10-13

## Context

The ESP8266's async web server runs handlers in the lwIP context. In the reference project,
handlers that touched the bus or the filesystem caused segment-select races ("four times a minute
while the page was open"), exception-9 resets and lost preference writes
([lessons P1–P4](../research/gbs-control-lessons-platform.md)).

## Decision

Network callbacks never call into `chip/`, `hal/` or the filesystem. They read an in-memory status
snapshot and enqueue commands; the main loop executes commands, ticks the machines and refreshes
the snapshot. The bus layer itself never yields inside one transaction (it uses the core's `twi_*`
functions, not `Wire`).

## Consequences

The segment cache is valid by construction and handler latency is bounded. Every remote action is
asynchronous: the API answers "accepted" and the client observes the result through the status
stream. The command queue and the snapshot cost a little RAM, accounted for in the memory budget.
