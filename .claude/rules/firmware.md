---
paths:
  - "firmware/**/*.{cpp,h}"
---

# Firmware rules (ESP8266, Arduino core 3.1.x, C++17)

- Only the main loop touches I2C. Network callbacks (async web server, WiFi events) never call
  into `chip/`, `hal/` or the filesystem: they read the status snapshot and enqueue commands.
- No `delay()`, `yield()` or busy-wait inside a state machine. Long sequences are machines with
  `tick()` and `busy()`, run by the scheduler; waits are deadlines checked on the next tick.
- Every constant table and string lives in flash: `PROGMEM`, `F("...")`, `*_P` functions.
  `const` alone is copied to RAM on this chip. Include `platform/progmem.h`, not `<pgmspace.h>`.
- No heap allocation on hot paths and no `String` in `chip/`, `hal/`, `video/`. Fixed buffers,
  `snprintf` with bounds, rings instead of growing containers.
- Interrupt handlers are `IRAM_ATTR`, touch only `volatile` state, and never call Serial,
  allocate or take long.
- Register access goes through `tv5725::Chip` with generated fields (`chip/fields.gen.h`); never
  hard-code a segment/address pair in a module. Writing a read-only field must not compile.
- Short comments explain *why* when the reason is not visible in the code (a measured limit, a
  chip quirk); link the doc that holds the evidence. Do not narrate what the code does.
- A change that grows static RAM (`.data + .rodata + .bss`) by more than 512 bytes or drops idle
  heap below the budget in `docs/design/memory-budget.md` must say how it pays that back.
- New behaviour gets a host test in `firmware/test/` against the fake devices when it can run
  without the board; bench-only behaviour gets a bench check (see the bench-verify skill).
