---
name: flash-board
description: Backs up and flashes the RetroScaler GBS-C over USB (CH340) with esptool — full-flash backup with checksum, firmware upload, restore of a previous image, and the check that the new firmware is really running. Use when firmware must be written to the real board, when the board must be restored, or before the very first flash of a board.
---

Flashing replaces what runs on the user's hardware. Confirm with the user before writing unless
they already authorised flashing in this session, and always say which image will be written.

## 1. Find the port

`uv run --group board tools/board/board.py ports` lists serial ports and marks the CH340.
If none shows, ask the user to connect the micro-USB cable (data cable, not charge-only) and to
check the CH340 driver.

## 2. Back up (mandatory before the first flash of a board, recommended before any flash)

`uv run --group board tools/board/board.py backup --port <COMx>`

Reads the whole 4 MB flash to `backups/<date>-<mac>.flash.bin` with a `.sha256` beside it, then
reads it a second time and compares. Report the file path and hash. `backups/` is git-ignored;
remind the user to keep a copy elsewhere.

## 3. Flash

Build first (`cd firmware && pio run -e gbsc`), then:

`uv run --group board tools/board/board.py flash --port <COMx> --image firmware/.pio/build/gbsc/firmware.bin`

The firmware image starts at offset 0 and leaves the filesystem (settings) untouched. Use
`--erase-all` only when the user asks for a clean board.

## 4. Confirm the new firmware is running

`uv run --group board tools/board/board.py console --port <COMx> --send status`

The banner must show the version you built. "A measurement belongs to the firmware it was taken
on": never trust a bench result until this check passes.

## Restore

`uv run --group board tools/board/board.py restore --port <COMx> --image backups/<file>.flash.bin`
verifies the `.sha256` first, then writes the full image back.

## If the board does not enter the bootloader

The encoder push button is GPIO0: hold it pressed while plugging the USB cable (or while
pressing reset), release after esptool connects.
