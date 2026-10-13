# Firmware

ESP8266 (ESP-12F module, 4 MB flash, 80 KB RAM), Arduino core 3.1.2, GCC 10.3, C++20.
PlatformIO project: run commands from this directory.

## Commands

```sh
pio test -e native                    # all host tests
pio test -e native -f test_chip       # one suite (directories under test/)
pio run -e gbsc                       # build for the board; writes .pio/build/gbsc/size.json
pio run -e gbsc -t upload             # flash over USB — bench action, see the flash-board skill
pio device monitor                    # serial console, 115200 baud
```

## Layout

`src/platform` (flash shims) → `src/hal` (I2C, Si5351, encoder, board profile) → `src/chip`
(TV5725 access, generated fields and programs) → `src/video` (machines) → `src/core` (scheduler,
settings, status, commands) → `src/net`, `src/ui` (web, Wi-Fi, console, OLED) → `src/main.cpp`.
Dependencies point down only; front ends reach the video path through commands and the status
model. The map and the reasons: `docs/design/architecture.md`, `docs/design/video-engine.md`.

## Things that will bite

- Only the main loop touches I2C and LittleFS (ADR 0003). The web server is synchronous
  (`ESP8266WebServer`, ADR 0008), so handlers run in the loop too — keep them short anyway and
  route chip work through commands.
- Use `tv5725::Chip` with `chip/fields.gen.h`; a read-only field cannot be written (it does not
  compile). Registers `S0_00..S0_2F` are status, read-only by the vendor's definition.
- The chip answers at most 16 bytes per read; `Chip` splits longer reads.
- An ESP reset does not reset the TV5725, the Si5351 or the SDRAM: initialise from any state.
- The encoder button is GPIO0, the boot strap: never require holding it at power-on.
- `const` data is copied to RAM unless it is `PROGMEM`; use `platform/progmem.h` accessors.
- Host tests compile a subset (`build_src_filter` in `platformio.ini`): new hardware-independent
  modules must be added there to be tested.
- Memory budgets (`docs/design/memory-budget.md`) are checked in CI; report static RAM and idle heap
  changes in the PR.
