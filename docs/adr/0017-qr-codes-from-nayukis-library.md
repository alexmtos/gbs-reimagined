# 0017. Draw the screen's QR codes with Nayuki's library, vendored

- Status: accepted
- Date: 2026-11-17

## Context

The UX design has the board's screen show a QR code on **Network**: one that joins the setup access
point, and one that opens the web page once the board is on the home Wi-Fi
([ux.md](../design/ux.md), *OLED + encoder*; the market research's idea 43,
[market-scalers.md](../research/market-scalers.md) §15). A phone camera then replaces typing the
network name, its password and an address.

Encoding a QR code (Reed–Solomon error correction, masking, version choice) is a well-defined but
error-prone piece of code. Project Nayuki's QR Code generator library is a widely used, MIT-licensed
implementation; its C version uses no heap and takes caller-provided buffers, which suits the
ESP8266. It is not published in the PlatformIO registry, which holds only third-party copies of an
older release (1.7.x).

The project owner chose this library over writing an encoder for the project (2026-11-17).

## Decision

- The C files `qrcodegen.c` and `qrcodegen.h` of release v1.8.0 are kept, unmodified, in
  `firmware/lib/qrcodegen/`, with the licence and a README that records the tag, the commit and the
  SHA-256 of each file. `NOTICE.md` credits it.
- The firmware encodes at most version 3 (29×29 modules), so a code fits the 64-pixel-high screen
  at two pixels per module; the address is written in upper case so that it fits version 1, drawn
  at three. It encodes on the stack (two buffers of 107 bytes) each time the screen is redrawn,
  which happens only when what it shows changes.
- The library is built with `NDEBUG` (its `library.json`): its `assert` texts would otherwise take
  about 500 bytes of RAM.
- Updates follow [ADR 0016](0016-dependencies-updated-by-hand.md): by hand, on the owner's request,
  with the README's record updated and the host tests run.

## Consequences

- The QR codes come from a tested encoder; the project's own code only builds the text (the
  `WIFI:` and `http://` strings) and draws the modules.
- The library's two constant tables (328 bytes) live in RAM, as the ESP8266 copies `const` data
  there. The screen's renderer pays for them: it keeps a 4-byte hash of the frame on screen instead
  of a 307-byte copy. Static RAM grows by 280 bytes in all, inside the 512-byte allowance per
  change, to 38,988 of the 40,960 budgeted ([memory-budget.md](../design/memory-budget.md)). The
  totals come from `tools/size/check_budget.py` on the builds of 2026-11-17 (1.1.2: 38,708 bytes);
  the tables, the frame and the `assert` texts from the symbol sizes of the firmware ELF
  (`xtensa-lx106-elf-nm -S`).
- A vendored file is not updated by any tool: a fix in a later release has to be noticed and copied.
