# 0011. Device libraries: U8g2 and Improv; own drivers for the rest

- Status: accepted
- Date: 2026-10-13

## Context

The firmware needs an OLED driver with fonts that cover Portuguese, Wi-Fi provisioning that works
with a browser installer, a clock-generator driver, encoder handling and a serial console — all on
one I2C bus shared with the scaler, within a tight RAM budget
([tech stack §2.4, §2.8, §2.9](../research/tech-stack.md)).

## Decision

- **OLED:** U8g2 (BSD-2) in full-buffer mode (1 KB static) with Latin-1 `_tf` fonts and partial
  tile updates, driven through our I2C bus via a custom byte callback so it never reconfigures the
  shared bus.
- **Wi-Fi provisioning:** own module (station first, then a `GBS-XXXX` access point with captive
  DNS served by the same web UI), plus **Improv Wi-Fi Serial** using the `improv-wifi/sdk-cpp`
  parser (Apache-2.0), demultiplexed from the console on UART0.
- **Own code** for the Si5351 driver (AN619 math, host-tested), the rotary encoder (IRAM
  quadrature state machine, loop-side click/long-press), the serial console (one command table
  shared with the HTTP API and the OLED) and the settings store.
- **ArduinoJson 7.4.3** only where a document is small; the hot status endpoint is written with
  bounded `snprintf_P`.

## Consequences

Few dependencies, each pinned; third-party licences are recorded in `THIRD_PARTY.md`. Our own
drivers carry their own tests.
