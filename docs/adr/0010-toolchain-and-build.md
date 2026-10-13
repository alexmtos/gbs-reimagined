# 0010. Toolchain: PlatformIO + Arduino core 3.1.2, C++20, doctest, explicit size budgets

- Status: accepted
- Date: 2026-10-13

## Context

The ESP8266 ecosystem is in maintenance: Arduino core 3.1.2 (2023) on GCC 10.3 is the latest
release, and `platformio/espressif8266@4.2.1` is what ESPHome and WLED still pin. GCC 10.3 compiles
`-std=gnu++20` (concepts, `std::span`, designated initialisers) but has no `<format>`. PlatformIO's
size limits do not fail an ESP8266 build ([tech stack §2.1, §2.10](../research/tech-stack.md)).

## Decision

- PlatformIO Core 6.2.0 and `espressif8266@4.2.1`, pinned exactly; board `esp12e`, 160 MHz,
  flash layout `eagle.flash.4m1m.ld` (1019 KB sketch, ~2 MB OTA staging, ~1000 KB LittleFS),
  lwIP2 low-memory variant.
- C++20 (`-std=gnu++20`) for the firmware and the host tests.
- Host tests with doctest through `pio test -e native` against fake devices; cppcheck through
  `pio check`; clang-format 22 pinned; clang-tidy on the native build.
- Python tooling runs with `uv` from the root `pyproject.toml`.
- A size-budget script parses `xtensa-lx106-elf-size -A` and fails CI when static RAM, IRAM or
  flash exceed `tools/size/budget.json`, or when a change grows static RAM by more than 512 bytes.

## Consequences

Builds are reproducible and match the wider ESP8266 community. The toolchain will not move; a
future hardware revision with an ESP32-C3 module (pin-compatible with the ESP-12F footprint,
unverified) is the upgrade path, which is why hardware access stays behind `hal/`.
