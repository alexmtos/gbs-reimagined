# 0008. Synchronous web server, polled status, UI embedded in the firmware

- Status: accepted
- Date: 2026-10-13

## Context

- ESPAsyncWebServer depends on ESPAsyncTCP on the ESP8266, whose repository has been archived
  since January 2025; the async handler context caused the reference project's crash classes and
  its HTTP OTA stopped working at ~15 KB of idle heap
  ([tech stack §2.2](../research/tech-stack.md), [lessons P1–P11](../research/gbs-control-lessons-platform.md)).
- The core's `ESP8266WebServer` runs handlers inside `loop()`. It serves one client at a time and
  costs 10.5 KB less flash and 1.65 KB less static RAM in a measured skeleton. Tasmota uses it.
- Each push channel (WebSocket, SSE) pins a TCP connection and a queue per client; polling a small
  status document every 1–1.5 s held up with 1–3 clients on the reference bench.
- A UI stored separately on LittleFS can drift from the firmware API, and flashing a filesystem
  image erases user settings.

## Decision

- HTTP is served by `ESP8266WebServer`, behind a thin transport-neutral API module (one command
  table, route registration in one place) so a later move to an async server or another chip is a
  contained change.
- Handlers still follow [ADR 0003](0003-main-loop-owns-the-bus.md): they answer from the status
  snapshot and enqueue commands; long chip sequences stay in the machines.
- Live state is polled: `GET /api/status` (compact, with a sequence number) every 1.5 s, 8 s
  client deadline. No WebSocket, no SSE.
- The web UI is built into one gzip-compressed HTML file and embedded in the firmware image
  (`PROGMEM`, served with `Content-Encoding: gzip` and an ETag). A development override serves
  `/www/index.html.gz` from LittleFS when present.

## Consequences

The firmware image is the only artefact: UI and API always match, and updates never touch
settings. A slow client can hold the loop for up to the server's 5 s timeouts, so every machine
must tolerate jitter (they are deadline-based), and responses stay small. UI-only changes need a
firmware update, which OTA makes cheap.
