# Reference lessons: platform, architecture, interfaces, instruments, bench

> **Provenance.** Knowledge extracted on 2026-10-13 from the documentation of the gbs-control fork
> this project builds its knowledge on: a local branch of
> [JuergenLeber/gbs-control](https://github.com/JuergenLeber/gbs-control) with extensive bench notes
> (docs dated up to 2026-09-24), itself descended from
> [ramapcsx2/gbs-control](https://github.com/ramapcsx2/gbs-control). Citations point into that
> fork's `docs/` and source files. Every measurement here is *theirs*: treat it as a hypothesis for
> this project's bench until reproduced (see [the research index](README.md)).


Knowledge extracted from the reference `gbs-control` fork (read-only; docs dated up to 2026-09-24,
read 2026-10-13). It covers facts, measurements, limits, pitfalls and rationale, and no code. Video-path
physics (output timing, blanking, deinterlacer, scaling, colour) is out of scope except where it
limits the platform. That material is in `docs/timing-and-blanking.md`, `docs/retrotink-parity.md`,
`docs/behaviour-controls.md` and `docs/calibration-defaults.md`.

## Citation legend

Citations are `(TAG §"section" Lnn-mm)`. Line numbers are as read on 2026-10-13.

| Tag | File |
|---|---|
| CL | `CLAUDE.md` (repo root) |
| RM | `README.md` (repo root) |
| FD | `docs/firmware-design.md` |
| AR | `docs/architecture.md` |
| ME | `docs/memory.md` |
| WI | `docs/web-interface.md` |
| OM | `docs/oled-menu.md` |
| MT | `docs/menu-taxonomy.md` |
| IN | `docs/instruments.md` |
| TO | `docs/tools.md` |
| FP | `docs/first-picture.md` |
| GL | `docs/glossary.md` |
| DS | `docs/datasheets/README.md` |
| SE | `docs/sensors.md`, read only for hardware facts the listed docs lack |
| TB | `docs/timing-and-blanking.md`, selected sections only (method errors, OTA, reset behaviour) |
| CK | `tools/checks/README.md` |
| `file:line` | a source or config file in the repo, cited for facts the docs do not state (pins, bus speed) |
| `git <hash>` | a commit message in the repo history |

Two markers are used. **Digest note:** means my own inference or verification, not a statement in
their docs. **[conflict]** means their docs disagree with each other or with the code. Appendix A
lists all of the conflicts.

---

## 0. The fifteen lessons that matter most

1. **The async web server runs handlers in the lwIP context. A `yield()`, a `delay()`, long work or
   filesystem activity there drops the board.** Handlers must validate, queue, and answer `202` or
   `409`. The loop does the work. Ignoring this cost board restarts after every timing apply, lost
   preference writes, orphaned capture files, and an exception-9 reset that looked like a "black
   frame" for days. (CL §"The async context" L111-115; FD L16-48, L468-479)
2. **The I²C bus layer must never yield in the middle of one register access.** On this core,
   `Wire.available()` yields, so a web handler could switch the chip segment between the read and the
   write of a read-modify-write. That happened 4 times a minute while the page was open. (FD §"Bus
   layer" L576-597)
3. **Heap, not static RAM, is the binding constraint.** The board idles at about 15 KB free and stops
   serving its page at ≤ 10 000. By 2026-09-19, HTTP OTA no longer worked at all ("about 15 KB
   against the upstream firmware's 22 KB") and flashing needed USB. (WI L260-301; TB L4687-4691)
4. **Long sequences must be state machines with `tick()` and `busy()`, with one `chipBusy()` gate
   that the API answers with 409.** Retrofitting this cost a whole phase (F2). (FD L50-411)
5. **One command table is shared by HTTP, the serial console and the OLED.** Each row carries a name,
   a risk class and an argument count. Critical operations need `ack=1` in the call itself.
   (FD §"The command layer" L1425-1459)
6. **Every register write is journaled at the bus hook, with an origin set by a scope object.** That
   answers "was it me or the firmware?". (FD L561-574; IN L96-121)
7. **Two single sources of truth generate everything else:** the register map plus its curation, and
   the HTTP contract. A `--check` mode fails CI when a generated file is stale. The UI bundle and the
   firmware carry the same hash, and the UI refuses to operate when they differ. (CL L83-89; FD
   L1467-1488)
8. **Preferences are stored by name hash, never by position.** The firmware never writes them at
   boot, writes them deferred and coalesced, and writes them in one buffered write. (FD §"Preferences"
   L646-727)
9. **Anything that can blank the output arms a 20 s automatic revert.** The revert restores fields
   rather than raw bytes, puts the clock back after the registers, then resets the video path. (FD
   §"Timer revert" L1961-1988)
10. **A coherent register set is not a picture on screen.** The verdict has to come from a capture
    card against a known pattern. (CL L172-175; TO L339-384)
11. **A host build with a fake chip seeded from real captures runs in CI.** So does a mock board for
    the UI. Neither can reach the async server, real I²C timing, the OLED or ISRs. (FD §"The host
    build" L1280-1344)
12. **Derived values must be re-derived from the source, not carried forward from the last result.**
    One such value held the wrong vertical scale for PAL "for as long as the project has existed".
    (FD L1192-1217)
13. **A field the board rewrites on its own may not be a default control.** A CI check enforces this.
    (CL L163-168; TO L111-136)
14. **Suspect the script before the firmware.** Their bench history lists eight method errors, and
    each one looked like a firmware defect. (TB §"Method" L877-946; TO L262-268)
15. **Their docs drift even with a prose-number checker.** Appendix A lists twelve contradictions
    found while reading. (Digest note)

---

## 1. Hardware facts

### 1.1 Board and chips

The board is the RetroScaler "GBSC" variant of the GBS-8200. It carries the TV5725 and its SDRAM
from the stock design, plus an ESP8266 module, a Si5351 clock generator, an SSD1306 OLED, a rotary
encoder, an MS9288A VGA-to-HDMI converter and an LM1881 sync separator. The stock MTV230MV
microcontroller is still fitted but is not on the ESP8266's I²C bus. (DS L7-11) The ESP8266 is a
Wemos D1 mini. (CL L5-6)

| Part | Bus / address | Facts that constrain firmware | Source |
|---|---|---|---|
| Tvia TV5725 (Trueview5725) scaler | I²C `0x17` | Six segments of 256 bytes, so a 1536-byte register space. The segment is selected by writing `0xF0`, and the address pointer auto-increments. | `tv5725.h:6`; SE L584; IN L279-283; FD L1290-1292 |
| | | It answers **at most 16 bytes per read transaction**. A 24-byte burst is refused as an *empty* read, not a short one. | FD §"Registry" L537-543 |
| | | RD-5725-1.1 describes 558 registers: 48 read-only (`s0_00`–`s0_2F`, contiguous) and 510 R/W. Writes to read-only registers are silently discarded. | DS L54-58; CL L135-141 |
| | | Exactly one signed field, `VDS_UV_BLK_VAL` (`s3_3F`). | DS L77-91 |
| | | DAC maximum is **162 MHz** (DS-5725-3.2 Table 19), lower than the HDMI converter's 165 MHz. | DS L149-151 |
| | | **Three** analog inputs, not four. `ADC_INPUT_SEL` 00 = R0/G0/B0/SOG0, 01 = R1/G1/B1/SOG1, 10 = R2/G2/B2, 11 reserved. Input 2 has no sync-on-green. | OM L50-58 |
| | | The register "program" is 432 bytes across segments 0/1/3/4/5 (segment 2 excluded). Writing it costs about 0.5–0.7 s of I²C. | TO L1116-1127; FD L347-348, L401-402 |
| | | The chip has its own power and does not reset with the MCU. Its FIFOs, SDRAM and PLLs hold state that no register shows. | IN L207-208; TB L2523-2537; CL L172-175 |
| SDRAM | — | Silkscreen says "2MB SD RAM" (16 Mbit, 1M×16). The part is unidentified and there is no datasheet. | DS L207-212 |
| Si5351 clock generator | I²C `0x60` | Generates the output pixel clock on TV5725 pin 40. | `src/si5351mcu.h:58`; SE L587; DS L118 |
| | | The display uses it only while `PLL648_CONTROL_01` = `0x75`. Every other code is a fixed internal rate: `0x25` 40.5 MHz … `0x95` 129.6 MHz, `0xA5` 162 MHz. | TB L4053-4058; FD L1543-1548 |
| | | It sits at **81 MHz** after detect-and-initialise. | TB L2739-2742 |
| | | Adoption disables its output for 800 µs. | FD L1933-1935 |
| | | The "smooth" ramp steps 1 kHz at a time for changes under 750 kHz. | FD L1725-1729 |
| | | Once adopted, the output tracks the source to **0.07 ppm**. On the internal PLL the output free-runs about 17% fast: 70.1067 Hz against a 59.9392 Hz source. | FD L1672-1674; TO L419-421 |
| SSD1306 OLED, 128×64 | I²C `0x3c`, SDA = D2, SCL = D1 | ThingPulse driver. A redraw costs about **41 ms** of I²C at a 50 ms refresh interval. The Sensors page costs about 35 I²C reads per redraw. | `gbs-control.ino:48`; FD L175-178; SE L564-565 |
| MS9288A VGA/YPbPr-to-HDMI | **not on the bus** (no answer in either direction) | Its own MCU runs it (`MCUSEL` floating). The I²C slave pins exist, but no register map or address is published. | SE L596-599; DS L153-156, L171-175 |
| | | A 10-bit triple ADC re-digitises the TV5725's analogue RGB, with its own auto phase/gain/offset and enhancement block. The chain therefore has two ADCs in series. | DS L145-148, L164-169 |
| | | 165 MHz maximum. The family brief states "input resolution up to 1080P", so 1080 lines is the practical ceiling of the chain. | DS L149-151, L195-199 |
| | | It wants explicit HSIN/VSIN, which is what `useHdmiSyncFix` (driving `PAD_SYNC_OUT_ENZ`) serves. "Auto Position (Blank) Detection" places the picture. | DS L157-160, L201-203; SE L615 |
| | | Re-acquiring its own lock takes it time. | TO L835-838 |
| LM1881 | not on the bus | Analogue sync separator for the RGBS jack. Not programmable. | DS L121 |
| EEPROM (AT24C02-like) | I²C `0x50` | Reads `0xFF` everywhere. It is the stock controller's settings memory and is unused. It is "free space" in principle, but establish whose it is first (the MS9288A document mentions an HDCP EEPROM). | SE L589-592; DS L122, L161-163 |
| CH340 | USB | Serial console and USB flashing. | DS L123 |

### 1.2 The I²C bus

- **There is one bus.** `0x17`, `0x3c`, `0x50` and `0x60` all answer in both directions. The scanner
  probes 0x08–0x77 with both a write and a read, because a read-only device is invisible to the
  usual scanner. (SE §"The I2C bus" L574-588)
- **It runs at 400 kHz.** `startWire()` calls `Wire.begin()`, sets SCL and SDA to
  `OUTPUT_OPEN_DRAIN`, then calls `Wire.setClock(400000)`. (`videopath.cpp:660-668`)
- **Digest note: the OLED driver tries to change the bus speed.** The ThingPulse `SSD1306Wire`
  constructor defaults to `frequency = 700000`, and `display.init()` applies it to the shared bus.
  (`.pio/libdeps/d1_mini/.../SSD1306Wire.h:73-100`) The bus returns to 400 kHz only because
  `startWire()` runs after `display.init()` in `setup()` (`gbs-control.ino:89,136,174`). Pass an
  explicit frequency in a rewrite.
- **A NACK looks like success.** `Wire.requestFrom()` on the ESP8266 returns 0 and discards the data
  when the slave NACKs early. The caller's buffer keeps whatever junk it held: the first state sweep
  returned 563 bytes of `0x3fff` heap addresses that looked like registers. (FD L539-543)
- **Clock stretching yields.** The I²C driver's clock-stretch wait yields after 5 ms. The TV5725
  does not stretch. (FD L596-597)
- **Boot-time bus recovery (code, undocumented).** If the TV5725 does not answer, `setup()` cycles up
  to 40 times through start, read, SCL held low for 12 µs, stop, until SDA reads high.
  (`gbs-control.ino:198-231`)
- **Scaler power check (code).** The firmware writes `0x6a` to `ADC_UNUSED_69` and reads it back.
  (`videopath.cpp:670-680`) The `power-check` tick runs this at `noSyncCounter` 61/62.
  `power-recover` waits for SCL and SDA to both read high for 50 ms, then waits 350 ms more and
  restarts Wire. (`gbs-control.ino:552-593`; FD L122-124)

### 1.3 GPIO

| Function | Pin | Configuration | Source |
|---|---|---|---|
| Encoder CLK | `pin_clk = 14` (GPIO14, D5) | `INPUT_PULLUP`, interrupt on FALLING | `board.h:20`; `gbs-control.ino:92,96` |
| Encoder DATA | `pin_data = 13` (GPIO13, D7) | `INPUT_PULLUP` | `board.h:21`; `gbs-control.ino:93` |
| Encoder push | `pin_switch = 0` (GPIO0, D3) | `INPUT_PULLUP`, interrupt on FALLING | `board.h:22`; `gbs-control.ino:94,97` |
| TV5725 debug/test-bus output | `DEBUG_IN_PIN = D6` (GPIO12) | input; vsync period and phase timed by `ICACHE_RAM_ATTR` ISRs | `board.h:24`; `framesync.cpp:22,33`; SE L354-356 |
| Status LED | `LED_BUILTIN`, active-low | lit during `setup()` | `board.h:26-31`; `gbs-control.ino:152-154,269` |
| I²C | SDA D2 (GPIO4), SCL D1 (GPIO5) | shared bus | `gbs-control.ino:48` |
| A0 | unusable | `ADC_MODE(ADC_VCC)` routes the ADC to the internal supply divider | `gbs-control.ino:72`; SE L529-533 |

- A discrete-button input path (`#if HAVE_BUTTONS`) was kept "by explicit product decision" but is
  not built. (MT L138)
- **Digest note:** the D-to-GPIO mapping is the standard D1 mini one, and `LED_BUILTIN` is GPIO2 (D4)
  on that board. GPIO0, the encoder push, is the ESP8266 boot-mode strap: holding the button through
  a reset would enter the UART bootloader. Their docs do not mention this.

### 1.4 MCU, clocks, flash, toolchain

- **CPU:** 160 MHz (`platformio.ini:17`, `board_build.f_cpu = 160000000L`). One ESP tick is 6.25 ns
  ("2 ticks (12.5 ns)", FD L1863).
- **Serial:** console at 115200 (`platformio.ini`; FP L43-44). USB upload at 460800.
- **Flash:** 4 MB with `eagle.flash.4m1m.ld` (`platformio.ini:18`). The toolchain's linker-script
  header gives the split:

  | Region | Size |
  |---|---|
  | sketch | ≤ 1 044 464 B (~1019 KB) |
  | empty, used as OTA staging | ~2052 KB |
  | LittleFS | 1 024 000 B (block `0x2000`, page `0x100`) |
  | EEPROM | 4 KB |
  | RF calibration | 4 KB |
  | WiFi configuration | 12 KB |

  Source: `~/.platformio/packages/framework-arduinoespressif8266/tools/sdk/ld/eagle.flash.4m1m.ld`
  L1-7, L16-19. **[conflict]** with ME L7, which says "the rest given to LittleFS".
- **Firmware size:** about 55% of the sketch area (RM L190). Static RAM is 47 940 bytes of 80 KB.
  (CL L130-131)
- **Platform and libraries** (`platformio.ini`):

  | Package | Version |
  |---|---|
  | `espressif8266` platform | 4.2.1 |
  | `esp32async/ESPAsyncTCP` | 2.0.0 |
  | `esp32async/ESPAsyncWebServer` | 3.12.0 |
  | ThingPulse SSD1306 | 4.6.2 |

  `PersWiFiManager` is a modified local copy (`git 38eba4f`), and the Si5351 driver lives in `src/`.
- **RTC user memory:** 512 bytes. The breadcrumb uses 20 of them: magic `MIGA` = `0x4D494741` at
  offset 0. (IN L37-41; `breadcrumb.h`, `breadcrumb.cpp`)
- **WiFi:**
  - Access point `gbscontrol`, password `qqqqqqqq`; mDNS name `gbscontrol.local`. (RM L177; FP L35-41)
  - Hostname lives in `/hostname.txt`, 1–32 characters, alphanumeric or `-`. (`network.cpp:228-246`)
  - Sleep mode is `WIFI_NONE_SLEEP` and TX power is 16.0 dBm. (`gbs-control.ino:143-144`, code)
  - Flashing firmware does not erase the filesystem or the WiFi credentials; only `pio run -t erase`
    does. (RM L169-171; FP L40-41)
- **LittleFS contents:**

  | Path | What | Source |
  |---|---|---|
  | `/app.html.gz` | the UI | WI L413-414 |
  | `/prefs.bin` | preferences | FD L657-659 |
  | `/user.cfg` | up to 64 register overrides | FD L720 |
  | `/hostname.txt` | hostname | FD L1177-1178 |
  | `/c/<n>.cap`, `/c/<n>.met` | chip captures, 4 slots | IN L283-284 |
  | `/p/<n>.cfg`, `/p/assign.bin` | profiles, 8 slots | RM L158; FD L1377-1378 |

### 1.5 Power quirks

- **Supply:** 5 V input. "Fussier about a weak supply than it looks: a thin phone cable into a laptop
  port is the most common cause of a board that half-works." (FP L9-10)
- **The VCC sensor reads the ESP module's 3.3 V rail,** not the 5 V input and not the TV5725's
  supply. It read 3.47 V on the bench. (SE L535-538)
- **The scaler can be unpowered or unresponsive while the ESP runs.** The Now panel's last rung is
  "The video chip is not answering … power or wiring on the board itself". (FP L99-103)
- **`ESP.restart()` does not reset the TV5725, the Si5351 or the SDRAM.** "Every one of the
  thirty-odd boots recorded … was an ESP restart with the scaler never losing power." A real power
  cycle later landed in the same bad state. (TB L2521-2560)
- **RTC memory** survives `ESP.restart()`, a watchdog reset and the reset button. It does **not**
  survive power loss, and it is **wiped by an OTA firmware upload**, measured 2026-09-14. (IN
  L37-47, L201-203)
- **Inherited power notes** (never re-measured):
  - Turning both phase controls off saves power: 506 mA → 493 mA.
  - ADC power saving: -10%.
  - Memory clock lowered to 108 MHz: -5%.
  - `VDS_TAP6_BYPS`: -2%.

  (FD §"Power notes, inherited" L2019-2029)

---

## 2. ESP8266 platform pitfalls: what each cost, and the rule adopted

### P1. The async handler context (lwIP)

- **Fact.** `ESPAsyncWebServer` runs handlers from the lwIP network-stack context. There, `yield()`,
  `delay()` and long blocking work "do not merely make things slow, they drop the board". (FD
  L16-21) The server also runs callbacks whenever the sketch yields: every `delay()`, every `yield()`,
  and between passes of `loop()`. The sketch's "hundred-odd `delay()` calls inside chip sequences"
  were therefore moments when a handler could run. (FD L87-90)
- **Evidence.**
  - The first timing apply ran inside the handler. "The board restarted six seconds after every
    apply, with an exception."
  - Writing `prefs.bin` inside `/api/prefs/set` dropped the board.
  - `registryFind()` must not yield because it runs in handlers. (FD L23-29)
  - `/api/capture/save` reset the connection "every time" and left orphaned `.cap` files without
    `.met`. (FD L31-38)
  - The capture case was a carried-over assumption: "only six 256-byte segment reads", based on an
    older 563-byte snapshot that never yielded. "A measurement belongs to the operation it was taken
    on." (FD L31-48)
- **Rule.** Handlers validate, queue and answer `202 {"queued":true}`. `loop()` does the work. The
  client polls a list or sequence number until the result registers. Writes arriving mid-sequence get
  `409 {"busy":true}`. (FD L40-43, L78-85)

### P2. No filesystem access from handlers, not even a directory walk

- **Evidence.**
  - `resetInfo` read `Fatal exception:9 (LoadStoreAlignmentCause) epc1:0x40106258
    excvaddr:0x00000003`.
  - The trigger was `/api/board/files`, which walked LittleFS and called `delay(1)` per entry inside
    the async callback. It fired "every second or third run" of the UI smoke test.
  - Three more handlers also touched the filesystem: `/api/fs/remove`, `/api/capture/remove`, and
    `/api/version`, which asked LittleFS for its totals.
  - "The three 'black frames' of 5 and 6 September were this crash, every time." (FD L468-479)
- **Rule.** `network.cpp` keeps the file list and the filesystem totals in a cache that
  `networkTick()` rebuilds when a module calls `networkFilesChanged()`. Removals are queued, and
  handlers answer from memory. "No handler touches the filesystem in the network's context — not to
  write, not to list, not to remove, not to ask for totals." (AR §"Files on the flash" L127-134)
- **Digest note.** The code still does LittleFS work inside async callbacks:
  - the upload chunk callback does remove, open "w" and write;
  - the download handler calls `exists`, then `send(LittleFS…)`;
  - `/` calls `exists`, then streams `app.html.gz`.

  (`network.cpp:162-193, 250-265`) Streaming a file through the library's response objects is
  evidently tolerated. The fatal case combined a directory walk with `delay(1)`. A rewrite should
  write down exactly which filesystem operations are allowed in async context instead of stating
  "none".

### P3. WiFi reconfiguration from a callback

- **Fact.** `WiFi.begin()` called from an lwIP callback invokes `WiFi.mode(WIFI_STA)`, which tears
  down the AP while the TCP connection is still alive, and the board crashes.
- **Rule.** Store the credentials and apply them on the next loop pass with `WiFi.persistent(true)`,
  `WiFi.mode(WIFI_STA)` and `WiFi.begin()`, in the pass *before* honouring the restart. The hostname
  follows the same route: validate, store, write `/hostname.txt` from the loop, restart. (FD
  L1161-1178)
- **Related defect.** For a while the stored credentials were never consumed, because the consumer
  was deleted along with the single-character command tables. The Connect button simply restarted
  the board on the old network. (FD L1167-1174)

### P4. The bus layer yielded inside a register access

- **Mechanism.**
  - `rawRead()` drained with `while (Wire.available())`.
  - `TwoWire::available()` calls `optimistic_yield()` when the buffer is empty, so every read ended
    with a yield.
  - A handler running in that gap selected another segment.
  - The result: "select segment 0, read, *handler selects segment 3*, write". The read-modify-write
    landed at the same offset in the wrong segment.
- **Evidence.** `VDS_WLEV_GAIN` (3:0x58) kept reading the values a watcher writes at 0:0x58. Under
  back-to-back API reads this happened "four times a minute; the web page polls, so it was happening
  whenever the page was open". (FD L576-592; FD L882-892)
- **Rule.** Read exactly the count `requestFrom()` returns, with no `available()` and no yield.
  Result: "zero in two runs of 120 s, 3654 and 3936 reads, zero strays".
- **The narrower rule.** "A handler may read and write fields, because every field access selects its
  own segment through the cache, but the bus layer itself must never yield between the pieces of one
  access." (FD L594-597)

### P5. Segment cache visibility

- **Fact.** The current-segment variable must live outside `setSeg()` so an observer of bus writes
  knows the segment.
- **Evidence.** When it was a function static, the journal assigned the wrong segment to every write
  not preceded by a segment change, and "a whole investigation chased writes to `s5_18` and `s0_58`
  that were on segment 3". (FD L570-574)
- **Rule.** Never select a segment from outside the bus layer. It leaves the cache stale, and every
  later compiled read goes to the wrong segment. (FD L551-555)

### P6. Route matching is prefix-plus-slash

- **Fact.** A plain `server.on("/api/prefs", …)` builds an `AsyncURIMatcher` of type
  `BackwardCompatible` with the rule `^{uri}(/.*)?$`. So `/api/prefs` registered first also answers
  `/api/prefs/set`, with "a 200 and the wrong body and no error anywhere". It does not match
  `/api/prefsx`. (CL L117-123; FD L512-528)
- **Evidence.** It happened twice. Preference writes vanished and returned the whole list. The
  journal's clear button returned the journal. (FD L525-528)
- **Rule.** Register specific routes before general ones. `tools/check_routes.py` runs in CI, reads
  routes in registration order across `api.cpp`, `network.cpp`, `PersWiFiManager.cpp` and the sketch,
  and requires the slash. (TO L16-30; AR L141) Name routes defensively: the status route is
  `/api/board/wifi/status`, not `/api/board/wifi`, which would have swallowed
  `/api/board/wifi/list`. (FD L1494-1497)

### P7. `beginResponse(code, type, pointer, size)` reads freed RAM

- **Fact.** That overload builds an `AsyncProgmemResponse`. It keeps the pointer and reads it later
  with `memcpy_P`, after the handler has freed the buffer.
- **Evidence.** The client received "563 bytes of already-freed heap with pieces of HTTP header
  inside, looking like plausible registers".
- **Rule.** Use a response stream, which copies immediately. (FD L1507-1511)

### P8. Request bodies arrive sliced

- **Fact.** A 6-byte batch record can straddle two network slices.
- **Rule.** Reassemble the whole body before applying, which also makes the batch atomic. The cap is
  128 records = 768 bytes, "what can be allocated transiently without threatening the heap margin".
  (FD L1513-1518)

### P9. Heap is the binding constraint

- **Thresholds.**
  - The page is refused at ≤ 10 000 bytes free: a 503 page from PROGMEM with `Retry-After: 5`
    (`network.cpp:250-256`; WI L265-271, L296-301).
  - The UI's danger line is 10 000 and its alert line is 12 000 (`public/src/main.ts:218-219`).
  - The event log records `HEAP_LOW` as a warning under 12 000 and a fault under 10 000. (IN L71-74)
- **Idle-heap history.**
  - Before the August trim, 13 192 bytes; after it, 23 664. (ME L221-226)
  - Removing the websocket took heap from 20 904 to 22 392. (`git 38eba4f`)
  - After F0, 18.9–19.8 KB. (ME L49-51)
  - On 2026-09-07, 14 424–15 736 at idle, and 11 720 once right after a restart. (ME L239-247)
  - No leak: 16 536 at 10 s of uptime, settling to ~15 900 within a minute. (ME L264-270)
  - On 2026-09-14, 15 040 at idle. (WI L277-286)
- **Load measurements, 2026-09-14** (minimum free heap, WI L273-294):

  | Load | Minimum free heap |
  |---|---|
  | 1 polling client | 14 680 |
  | 2 polling clients | 14 224 |
  | 3 polling clients | 12 000 |
  | 4 polling clients | 10 752, and one request failed |
  | serving the page once (80 758 bytes from LittleFS, about 3.3 KB of heap) | 11 536 |
  | two page loads at once | 8 272, both served |
  | a third concurrent page load | refused |

- **Other heap costs.**
  - Registering the same handler twice costs a second `AsyncCallbackWebHandler` with its own
    `std::function` pair. Ten old-path aliases cost **1.8 KB**: 18.2–19.2 KB free without them,
    16.5–17.4 KB with them. They were removed, and scripts fall back to the old path on a 404 instead.
    (FD L1499-1505)
  - Failed HTTP flash attempts leak heap: 20 680 fell to 13 240 over four failures. "Restart the
    board before retrying." (TO L1158-1165)
  - Bench scripts can exhaust the heap. The board "restarted every 40 to 80 seconds while being
    polled", with `HEAP_LOW` at 5 090 bytes, and "six `/api/dump` calls in a row are enough to kill
    it. Pace the reads." (TB L4693-4696)
  - A JSON of 958 fields is "the shortest path to a lockup". The schema ships in the UI bundle and
    the board serves only binary values. (TO L1039-1042; WI L16-20)
- **Rules.** Prefer static rings to heap objects, because "a buffer that lives for the whole program
  has no reason to take part in heap fragmentation". (ME L117-120) Keep transient buffers (563, 256,
  768 bytes) to handler lifetime; "handlers never overlap". (ME L208-210) Every new feature measures
  heap before and after, because "the working band sits on the 15 KB floor". (ME L254-258)

### P10. Static RAM: `.rodata` lives in RAM

- **Fact.** On the ESP8266, string literals and `const` tables without `PROGMEM` are copied to RAM at
  boot, and "a `const char *` array is not enough". Use `PROGMEM`, `F("…")` and the `_P` functions.
  (ME L89-91, L204-206)
- **Budget rule.** No change may grow static RAM by more than 512 bytes without paying it back. The
  current baseline is **47 940 bytes (58.5%)** of 80 KB. Measure it from the ELF with
  `xtensa-lx106-elf-size -A`, adding `.data` + `.rodata` + `.bss`. (CL L130-133; AR L142)
- **Largest static users.**

  | Symbol | Bytes | What |
  |---|---|---|
  | `itemPool` | 2704 | OLED menu pool, 52 × 52 |
  | `g_ic` | 1704 | WiFi SDK, not theirs |
  | telemetry ring | 960 | 96 × 10 |
  | journal ring | 768 | 96 × 8 |
  | `wvfState` | 512 | WiFi SDK |
  | `dns_table` | 432 | lwIP |
  | `fieldLabels` | 384 | OLED Tuning labels |
  | probe ring | 288 | 48 × 6 |

  The firmware's own share is about 7.8 KB, and "there is no fat". (ME L103-112, L272-292)
- **What was won back.**
  - The OLED item struct went from 96 to 52 bytes and the pool from 96 to 60 slots, saving 6196
    bytes. (ME L134-150)
  - Moving the Tuning field names into PROGMEM blobs freed about 2400 bytes of `.rodata` against 384
    of `.bss`. (ME L155-167)
  - The splash XBM had sat in `.data` without `PROGMEM`: 1024 bytes. (ME L169-176)
  - Deleting a harmful frame-lock call returned 576 bytes. (FD L1788-1790)
- **Digest note.** CI runs `numbers.py` before the firmware build, and `numbers.py` skips the RAM
  check when no ELF exists ("static RAM not checked: build the firmware first",
  `tools/checks/numbers.py:184-191`; `.github/workflows/build.yaml`). So the 512-byte rule is not
  enforced in CI.

### P11. OTA: three mechanisms, all problematic

- **ArduinoOTA.** The loop called `ArduinoOTA.handle()` on an instance that was never initialised.
  Initialising it "crashes the video: the clock drops from 162 to 81 MHz and the screen goes dark".
  `espota` also needs the ESP to connect back to a port on the PC, which the firewall blocked. Both
  were removed. (`git 0e12453`; RM L76-79)
- **Digest note: which clock?** The docs do not say which clock dropped. 162 MHz is the TV5725's
  internal display-PLL rate (`0xA5`) and 81 MHz is the Si5351 default after initialisation (TB
  L2739-2742, L4057). The CPU is configured at 160 MHz. "ArduinoOTA dropped the CPU clock" is not
  supported by the evidence; treat it as unverified.
- **HTTP upload (`/api/board/firmware`, `Update.runAsync`).**
  - At `--limit-rate 25k` the upload "died four times in a row, each time after about 77 KB". 15k
    "flashed first time in 35 seconds". (TO L1158-1162)
  - **By 2026-09-19:** "OTA does not carry a firmware onto this board any more. Six attempts … at 15k
    and at 10k … all closed the connection part way. The heap this firmware idles at, about 15 KB
    against the upstream firmware's 22 KB, is not enough for the async upload." USB writes it in
    fifteen seconds. (TB §"2026-09-19, on the board" L4687-4691)
  - **[conflict]** WI L418-419 says network flashing works "with the sync watcher off and at
    25 KB/s".
- **Flash-layout claim.** "With 733 KB written into a 1044 KB region, a new image would have to fit
  in 311 KB for OTA to work." (FD L2010-2013) **Digest note:** that arithmetic assumes the filesystem
  starts at 1 MB. With the current 4m1m script there are about 2052 KB of staging space (§1.4). The
  binding constraint today is heap, not flash.
- **Corollary.** "Each probe past the ceiling costs a stall and a recovery over serial, because OTA
  needs a network the stalled board is no longer on." (TB L4365-4367)

### P12. The watchdog, light sleep and bounded spins

- **The watchdog window.** The software watchdog's window is about **3.2 s**. `ESP.wdtDisable()`
  around a vsync spin "protects against nothing": the worst no-edge spin is 619 ms, twice per
  measurement, 1.24 s in total. (FD L1869-1873)
- **Light sleep inside a spin "buys nothing and costs something".** The SDK suspends only inside
  `delay()` or `yield()`. (FD L1875-1878)
- **`delay(7)` before sampling bought nothing measurable:** 1.5 ppm against a 600 ppm threshold. (FD
  L1880-1882)
- **Rule: bound waits by deadlines, not iteration counts.** The 3-million-iteration bound sat "twenty
  to sixty times above the need". It was replaced by a 100 ms deadline, measured at "100.1 ms against
  the 619 ms it used to spin". The good path went from 22.3 / 25.1 / 51.5 ms to 16.8 / 19.3 / 34.3 ms
  (min / median / max). (FD L1887-1916)
- **Open defect.** On the timeout path the ISR is never detached: `_risingEdgeISR_prepare` stays on
  `DEBUG_IN_PIN` until the next `start()`. (FD L1926-1929)

### P13. Unexplained resets and exceptions need numbers, not suspicion

- **The mechanism.** "Black frames with a healthy chip" were the board resetting by itself in the
  middle of an apply. The reboot rewrote the program while HDMI was up, and the capture card dropped
  the link and never re-locked. (FD L455-460)
- **Rule.** `/api/sensors` carries `resetReason`, and `/api/version` carries `resetInfo`, which
  includes `epc1` to feed `xtensa-lx106-elf-addr2line`.

  | `resetReason` | Meaning |
  |---|---|
  | 0 | power on |
  | 1 | hardware watchdog |
  | 2 | exception |
  | 3 | software watchdog |
  | 4 | restart |
  | 5 | deep sleep |
  | 6 | external |

  (SE L519; FD L460-466)
- **Recovery.** After a watchdog or exception reset, the first clock adoption queues one output
  re-presentation. Since a format change also drops the HDMI link, *every* default-mode apply now
  ends with one queued re-presentation. (FD L460-487)
- **After a flash, verify the new firmware is the one running.** "Read something only the new
  firmware would say before trusting a measurement to it." An F3 flash had silently failed to build
  and left F2 running; the journal origins gave it away (`firmware` where F3 says `watch`). (FD
  L446-453)

### P14. LittleFS behaviours

- **Opening `"w"` over an existing file fails silently.** "Uploading a new file always worked;
  re-uploading the same one never did." Remove the file before opening it. (FD L1348-1351)
- **`request->send(LittleFS, path)` with a missing path never answers.** The request falls through to
  HTTP 501 "Handler did not handle the request". A backup script stored "two hundred and eighty files
  of thirty-four bytes containing that message". Check existence first. (FD L1353-1356)
- **A full filesystem looks like a dropped connection.** A 51 KB transfer failed after a 160 KB one,
  and the upload handler does not check whether the write fit. Report `fsTotal` and `fsUsed` in
  `/api/version`. (FD L1371-1373)
- **Call `LittleFS.begin()` once,** and load preferences and overrides *before* the web server
  starts. Otherwise the first `/api/prefs` of a boot answered with defaults. (FD L1366-1369)
- **No recursive listing.** Backup builds the file list from each module's naming convention.
  Uploading into a subdirectory works by sending `filename=c/0.cap`. (TO L1150-1153)

### P15. Writing preferences

- **Never write the preferences file at boot when it is missing.** "If the save drops the board, the
  file is still absent on the next boot and the same save is attempted again". That bootloop was hit
  on the bench and needed an external reflash. (FD L676-681; RM L174-176)
- **Never save on every change.** Eight preference writes, each followed by a heavy effect and a
  LittleFS transaction, left `prefs.bin` corrupted: the watchdog caught a write mid-way. Defer the
  save: `prefs.bin` is written 3 s after the last change (`prefs.cpp:261,267`; FD L683-686, L1364).
- **Write once, from a stack buffer.** It used to be 54 `write()` calls. A `static_assert` caps the
  buffer at ≤ 512 bytes. (FD L670-674; `prefs.cpp:115`)
- **Queue effects as a bit mask, not a single slot.** With one slot, two preference writes between
  loop passes lost the first effect. A 32-bit mask covers the preferences. (FD L688-693)

### P16. The radio's share of the loop

- **Measurement** on an idle board. (FD L157-178)

  | Tick | Menu awake | Screensaver on |
  |---|---|---|
  | `oled` | 43 ms | 18 ms |
  | `net-pending` (mostly the SDK's work landing in the yield after it) | 38 ms | 38 ms |
  | `net` | 32 ms | 30 ms |
  | all fourteen subsystem ticks together | 1 ms | 1 ms |
  | **loop pass** | **76 ms** | **51 ms** |

- **Conclusion.** One frame at 59.74 Hz is 16.7 ms. "The target is not reachable while this firmware
  serves a network from the same loop." (FD L180-185)
- **"A tick that yields absorbs whatever the radio does".** Attributing that time to the function
  before the yield "would send someone to optimise an empty branch". (FD L169-173)

### P17. `fetch()` has no timeout

- **Fact.** One hung request stopped the poll loop for good, "fully drawn while the board stayed
  perfectly healthy".
- **Rule.** Every call carries an 8-second deadline. The poll awaits before it reschedules. (WI
  L193-198, L38-40; CK L92-103)

### P18. Cross-site requests against a GET-mutating API

- **Rule.** One middleware refuses `Sec-Fetch-Site: cross-site` and any `Origin` whose host is not
  the board's. Requests with neither header (curl, scripts) pass. "This is not authentication."
  (FD L489-507)
- **Cost.** Nothing in static RAM; free heap 16 344 with it against 16 136 without. (FD L509-510)

### P19. Small platform traps

- **`PersWiFiManager` scan needs two calls.** `/api/board/wifi/list` returns empty on the first call
  and the result on the second. (WI L336-340)
- **The old shared header's include guard never closed** (`#ifndef _USER_H_` was never defined).
  Headers must stand alone and include `<stdint.h>`. (FD L641-644)
- **`.local` names do not resolve on some phones.** The serial console prints the address. (FP
  L36-37)
- **`ADC_MODE(ADC_VCC)` is a compile-time decision** that disables `analogRead(A0)`. (SE L529-533)
- **`millis()` wraps** at about 49 days. (SE L540)
- **The websocket log mirror** (port 81) cost a 2142-line library, a ping every second and server
  memory, "for a client that does not exist". Removing it saved 13 314 bytes of flash and 1400 of
  static RAM, and free heap rose from 20 904 to 22 392. (FD L1140-1147; `git 38eba4f`)

---

## 3. Architecture lessons

### 3.1 Layering

- **Layers** (dependencies point down only):

  | Layer | Contents |
  |---|---|
  | 0 · access | register map and bus, registry, Si5351 driver, `board.h` |
  | 1 · video path | primitives without policy |
  | 2 · subsystems | machines and memories |
  | `sched` | scheduler |
  | 2½ · commands | the command table |
  | 3 · panels | api, console, network, OLED, OSD, captive portal |
  | assembly | `gbs-control.ino`: `setup()`, `loop()`, the tick tables |

  (AR §"Layers" L7-20)
- **Layer 3 never touches layer 1.** It goes through a layer-2 function, the registry or the command
  table. (AR L22-25; CL L79-81)
- **No `extern` function declarations in a `.cpp` file.** A module's interface is its header. That
  count went from 31 to 0. (CL L128; FD L421)
- **Seven mutual `.cpp` pairs still know each other's names,** with no include cycles. (AR L144-152)
- **F3 cut the sketch** from 6 882 lines and 159 functions to about 630 lines. Inlining lost across
  module boundaries plus the journal scopes added +3.3 KB of flash. (FD L413-428)
- **Digest note.** AR L139 claims an include-cycle check script exists in `tools/checks/`. None was
  found there.

### 3.2 State ownership

- **The runtime state is four structs grouped by subject, not owner.** "Almost no field is touched by
  a single module": `videoStandardInput`, `currentLevelSOG` and `syncTypeCsync` each have five
  writers. (FD L599-622; AR L27-44)

  | Struct | Fields |
  |---|---|
  | `SourceState` | 9 |
  | `LockState` | 15 |
  | `PathState` | 9 |
  | `BoardState` | 5 |

- **The written-by tables are derived from code,** by scanning assignments through the four pointers.
  "A field with five writers is one whose value cannot be predicted from any single file, so adding a
  writer is a decision to take deliberately." (AR L41-44)
- **The split was a pure rename,** 823 sites, so the compiler catches errors. Cost: 12 bytes of static
  RAM and 668 bytes of flash. (FD L617-622)
- **Persisted options are separate from runtime state.** `userOptions` holds the five algorithm
  switches with no chip field. Runtime state is rebuilt each boot. `adcState` sits with runtime state,
  with `r_gain == 0` meaning "not measured yet". (FD L628-639)
- **Two reset lists that had drifted apart** (`setup()` and `setResetParameters()`) became
  `stateResetSource()` and `stateResetBoot()` in one file. (AR L35-38; FD L426)
- **The pause is a lock inside `unfreezeVideo()`, not eight guards at its eight callers.** The program
  load rewrites the capture bit, so the lock re-asserts it on each unfreeze. (FD L1264-1271)

### 3.3 Long sequences are state machines

- **Each sequence has `tick()` and `busy()` and runs from the tick table.**

  | Machine | States / pieces |
  |---|---|
  | post-load | `PL_WAIT_SYNC` (≤ 2 002 ms), `PL_WAIT_MODE` (≤ 1 505 ms), `PL_SOG_SETTLE` (300 ms), `PL_CLAMP` (25 tries × 20 ms), `PL_FINISH` |
  | input detection | `DT_*` states |
  | watcher | ten functions plus deadline "holds" via `watcherHold(ms, then)` |
  | timing apply | 1–2 passes |

  None of them calls the scheduler. `chipBusy()` is the OR of their busy predicates and is "the 409
  the API owes its clients". (AR §"Machines" L104-117; FD L319-378; CL L91-94)
- **Scheduler guard.**
  - A `SchedSequence` RAII guard raises a depth counter, so an early `return` lowers it.
  - A flag marks the tick list as executing.
  - `handleWiFi()` runs ticks only when both are clear. Otherwise it does only the network part and
    counts the refusal in `schedHeldCount()`.

  (FD L69-76)
- **Why the guard exists.** Nested calls from wait loops let a deadline expire *inside* a sequence:
  `applySourceAspect()` and friends ran "in the middle of the post-load's two hundred and fifty
  writes". (FD L57-67)
- **The loop is a table** of named ticks with a cadence each, e.g. watcher every 20 ms, source-detect
  every 500 ms, IRQ clear every 3 s. The cadence gates the *attempt*, not the run. The order of the
  table is the semantics. (FD L104-131)
- **Each tick is timed.** `/api/sched` reports per-tick cadence, longest run since boot and in the
  last 10 s, and run count. `/api/sensors` reports the longest pass and the slowest tick. (FD
  L133-145)
- **Measured results of F2** (2026-09-06). (FD L380-402)

  | Metric | Before | After |
  |---|---|---|
  | `handleWiFi()` called from inside a sequence | 8 places | 0 |
  | `delay()` calls | 122 | 89 |
  | longest function | 1 158 lines | 478 lines |
  | `network` tick, longest run | 1 366 ms | 287 ms |
  | `schedulerHeld` at end of boot | 555 | 97–285 |
  | loop pass at rest | 85–94 ms | 87–105 ms (frame lock + OLED) |

- **What still blocks.**
  - The 432-byte program write.
  - The RGB/HV branch's 320/300/100/40 ms waits. They stay unconverted because "there is no RGB/HV
    source on the bench", and converting blind "would compile, the tests would pass, and nothing would
    have exercised the branch".
  - Watcher worst case 706 ms; subsys 309 ms.

  (FD L263-291, L310-313)
- **Lesson: a getter must not write.** `getVideoMode()` answered queries by writing twelve
  `MD_*_CNTRL` thresholds with random skew, so "the firmware" showed up in the journal moving
  untraceable registers. It became a pure `modeRead()` plus an explicit `modeNudgeVga()` that only the
  two detectors call. (FD L223-235) The nudge overwrites hand-set values on the next borderline
  signal. A host test pins that behaviour, and the field help text warns users. It was found by the
  field-level chip diff, because "twelve thresholds each moving by exactly +1 … is a signature no
  measurement drift produces". (FD L237-261)

### 3.4 One command table for every panel

- **Each operation is a row in PROGMEM** (flash only): name, risk, number of numeric arguments,
  handler, usage line. (FD L1427-1435)
- **`commandRun(name, args, origin, reply)`:**
  - refuses a short argument list;
  - refuses a critical command that was not acknowledged;
  - opens a `JournalScope` with the panel's origin;
  - calls the handler.
- **Eight results** come back: done, accepted, busy, refused, needs-ack, unknown, bad-args, not-found.
  `commandHttpCode()` maps them to HTTP codes, and the console prints the name.
- **Access.** `POST /api/cmd/run?name=…&a0=…&text=…&ack=1` and `/api/cmd/list`. The console has
  `cmds` and `cmd <name> [args] [ack]`. OLED action items call `commandRun()` with origin `JO_OLED`.
  The older routes are thin wrappers. (FD L1443-1450)
- **Arguments are integers,** so `timing.apply` takes the refresh rate in millihertz. (FD L1439-1441)
- **Shared tables live in the contract.** The OLED's and the page's timing-mode lists had drifted in
  their labels while hand-copied. (FD L1452-1454)
- **Cost:** 224 bytes of static RAM, 4.2 KB of flash. (FD L1458-1459)
- **What it replaced.** Two single-character tables, 76 commands on serial and 44 on the web, were
  "two places to diverge". The API alphabet had 77 cases and "one free letter left". (RM L52-54; FD
  L1149-1155, L1463-1465)

### 3.5 The write journal with origin

- **Where the hook lives.** It installs on `tw::detail::rawWrite`, the hook every bus write passes
  through, so it sees the writes made by the watcher, preset reload and auto-gain via the compiled
  field path. A layer above would see only writes addressed by index. Cost: "one null comparison per
  transaction". (FD L563-568; IN L102-105)
- **Origins.** Panels set `web`, `serial`, `oled`, `batch` and `revert`. Subsystems set their own
  through `JournalScope`: `watch`, `program`, `clock`, `detect`, `tune`, `picture`, `geometry`.
  `firmware` (0) means unclaimed. The origin enum is part of the API contract. (AR L119-125; CL
  L100-102) F3 took the origin count from 6 to 13. (FD L428)
- **Sizing limits.** The ring holds 96 entries × 8 bytes and fills in **2 min 24 s** at idle; the
  interrupt clear at `s0_58` alone writes twice every 3 s. (IN L107-111) A preset load writes "more
  than 400", so the journal cannot reconstruct a load. (TB L921-922) `journalSequence()` is
  monotonic, so a client can detect missed entries. (IN L118-119)

### 3.6 Preferences, overrides and profiles

- **Preference format.**
  - Header `"GBSO" | version u8 | count u16`, then repeated `name-hash u32 | value u8`. (FD L655-660)
  - The hash is 32-bit FNV-1a (offset basis 2166136261, prime 16777619). (`prefs.cpp:20-23`)
  - A missing preference keeps its default and an unknown one is ignored, so the table can grow,
    shrink and be reordered. (FD L662-663)
  - `max` is a sanity limit, not validation: anything past it becomes the default. `0xFF` is
    `PIC_KEEP`, "no opinion". (FD L665-668)
- **What it replaced.** The old positional format shifted every later preference when one was
  removed, and its tail had four formats told apart by "counting leftover bytes". (FD L648-653; RM
  L61-64)
- **Register overrides** (`/user.cfg`): up to 64 entries of FNV-hash plus value.
  - Until 2026-09-04 a schema-hash mismatch discarded the whole file, so "each registry change silently
    emptied the overrides".
  - Each entry now resolves by name, and the unresolved count is reported. (FD L718-727)
- **Profiles.** (FD §"Profiles" L1375-1423)
  - **Contents.** A profile is a named copy of the user's overrides (≤ 64, resolved by name) plus all
    preferences. No program and no timing. Fixed ADC gains must be stored together with
    `enableAutoGain = 0`, or auto-gain "walks over them within seconds".
  - **Slots.** Eight slots, names up to 23 characters, one per file in `/p/<n>.cfg`.
  - **Loading order.** Arm the revert, replace and apply the overrides, set preferences and queue
    their effects, queue the file writes.
  - **Manual vs automatic loads.** A manual load arms the 20 s revert. An *automatic* load (a profile
    assigned to the source's standard) does not, because "the assignment was the consent". A load
    that reverted itself 15 s after every boot was the old system's defect.
  - **Standards.** One assignment per detected standard. A switch between 240p and 480i is one
    standard and reloads nothing.
  - **Bench-found bug.** "The active set stops counting as a profile the moment it is changed by
    hand", or a re-settle would not reload the profile.
  - **Cost.** Name cache 8 × 24 bytes, assignment table 16 bytes. File I/O runs in `profileTick()`.
    `profileBusy()` is part of `chipBusy()`.
- **No output presets.** "Profiles are copies of your own changes." There is one base program per
  detected standard, and nobody selects it. (RM L13-18)

### 3.7 Revert on timeout

- **What it is.** The contract a monitor offers on a mode change: apply, ask, and undo if nobody
  answers. `revertArm(seconds)` takes a 563-byte snapshot plus the pixel clock and sets a deadline.
  `revertConfirm()` cancels it. `revertNow()` restores. (FD L1961-1969)
- **Restore order matters.**
  - Walk the fields, not raw bytes, so reserved and state bits are not stepped over.
  - Restore the clock by ramp *after* the registers. Restoring registers without the clock produced an
    incoherent mode: "htotal 2704 at 85.25 MHz gives 31.5 Hz".
  - Then reset the video path, or "the screen stayed black: the FIFOs and the SDRAM were still aligned
    with the timing being undone". (FD L1970-1983)
- **Who confirms.** The OLED panel confirms its own applies 3 s later, because the revert arms only
  when the queue reaches the request. The browser does not confirm, "because whoever applies from a
  browser loses, with the picture, exactly what they would use to undo it". (FD L1985-1988; OM
  L204-219)

### 3.8 Single sources of truth and generated artifacts

- **Register registry.** `tv5725.h` (each field's locus: segment, offset, bit, width) plus
  `tools/registry/fields.json` (label, group, help, risk) generate `registry.gen.h` (PROGMEM table),
  `public/src/registry.gen.ts` and `docs/register-map.md`. (CL L83-89; TO L1028-1044)
- **API contract.** `tools/contract/api.json` covers routes, methods, parameters, response types,
  status codes, five wire enums and the binary layouts. It generates:
  - `api_contract.gen.h`, whose `static_assert`s pin enum values and `sizeof` to the firmware's;
  - `api.gen.d.ts` and `api.gen.js`;
  - `docs/api.md`.

  `tools/checks/contract.py` fails when a route, method, parameter or JSON key exists on one side
  only. (FD L1471-1482; TO L68-92)
- **Never hand-edit a `.gen.*` file.** `--check` modes fail CI when generated files are stale. (CL
  L86-88)
- **Versioning.** `/api/version` carries `schema` (a hash) and `api` (an integer that changes when an
  older client would break). The UI refuses a foreign schema hash or a higher api number. (FD
  L1484-1486; WI L22-25)
- **Vendor access flags.** `extract_access.py`, run by hand, reads the PDF's `RO`/`R/W` markings into
  a committed `vendor_access.json` with page count and SHA-256. `build_registry.py` refuses to
  generate a registry that offers a write to an `RO` register, so CI enforces it without the PDF.
  (TO L1066-1086; CL L42-44)
  - It found **eleven** fields the firmware would write and the chip discards: `HPERIOD_IF`,
    `VPERIOD_IF`, three `CHIP_ID_*` and six test-bus readouts. (DS L60-70)
  - Regex pitfall: a bare `R` also matches the `R` of `R/W`, which reports all 558 registers as
    read-only. (DS L72-75)
- **Datasheet extraction is a "candidate, not truth".**
  - Where `tv5725.h` and the extraction disagree, `tv5725.h` wins. (TO L1059-1060)
  - Use `pdftotext -layout`. Even then, bit descriptions land beside the wrong bit, so match by
    meaning. (DS L31-41)
  - For the Chinese MS9288A PDF, use PyMuPDF: `pdftotext` and `pypdf` drop the Chinese. (DS L177-180)
- **One-time generators.** The addendum generator is pasted in by hand once, then reviewed. The
  curation seed runs once, and `fields.json` is hand-edited afterwards. (TO L1088-1105)
- **Painful.** Curation by default produced 739 fields marked "safe" that nobody had tried, and the UI
  told readers the opposite until 2026-09-15. (CL L147-152)

### 3.9 Risk classes and safety gates

- **Risk counts.** 958 fields: 782 safe, 175 critical, 1 needing a reset. Writing a critical field or
  running a critical command needs `ack=1` in the call (`/api/set` still also takes `force=1`). The
  field stays visible, marked; the gate exists so "a write capable of blanking the output never
  happens on a stray click". (RM L29-32; CL L143-145)
- **What "critical" was measured against.** `probe_field_risk.py` has one criterion: the write
  knocked the board off the bus. It never looks at a frame. `VDS_NR_MIG_USER_EN` wrecked the picture
  while classified "safe" (2026-09-07). (CL L154-158) The safety probe alone classified "216 of 216
  fields as safe, a result that discriminates nothing", so `probe_picture.py` was added. (TO
  L991-998)
- **Coverage.** The sweeps reached 219 of 958 fields. The schema carries `m` for measured fields, and
  the UI badges the rest "never probed". (CL L147-152)
- **The vendor decides what is writable; the bench decides what is wise.** (CL L135-141; DS
  L104-107)

### 3.10 Ownership rules learned the hard way

- **"A rule that has to be stated twice will eventually be stated differently."** A step-response rule
  lived in two modules, and the result depended on which ran last. (FD L711-716)
- **Brightness and contrast had three writers.** The OSD, the picture base capture and the scanline
  base capture all wrote them, so an OSD adjustment silently vanished on the next scanline toggle or
  reload. (FD L752-766)
- **"A one-bit preference over a one-bit field is a second owner as surely as a struct wrapper is."**
  The rule that followed: a control is one chip field, or it is one of five firmware algorithms with
  no field to point at. (MT L90-91; RM L16-18)
- **Silent correction loops hide defects.** "A loop that corrects silently hides exactly the class of
  defect this investigation found." The once-a-second "gap-filler" sweep was removed. (FD L695-703)
- **Derived values must come from the world.** The vertical scale carried the previous ratio forward
  and held `VDS_VSCALE` 456 for NTSC and PAL alike. "A derived value that carries its own previous
  result forward cannot be tested by looking at it: it agrees with itself on every read." (FD
  L1192-1217) `applySourceAspect()` re-derived from its own truncated output, and the scale walked down
  one count per pass. One count was enough to tear the picture. (FD L1623-1641)
- **Dead guards look like thoroughness.** "A guard repeated inside the branch it already guards is
  always dead, and always looks like thoroughness." (FD L827-841)
- **Per-mode chip registers are rewritten by every program load.** Re-apply stored values at the end
  of every load: a reload once silently restored a half-finished experiment's values. (FD L1225-1230)

### 3.11 The host build with a fake chip

- **Real modules, fake world.** 25 real modules compile unchanged for the host (CL L104-107; AR
  L154-161). **[conflict]** FD L1282 still says "Twenty-three … eighteen tests". The shims, under
  `test/host/include/`:
  - `Arduino.h`: String, Print, `millis`, PROGMEM macros, `ESP`;
  - `Wire.h`: the other end is a TV5725 register file, six 256-byte segments, segment select at
    `0xF0`, auto-incrementing pointer;
  - `LittleFS.h`: files in a map;
  - a static `FrameSync`.

  The eight "outside world" modules and the OLED are stubbed with call counters. (FD L1286-1295)
- **The chip is seeded from real 1536-byte captures** (PS1 240p at boot, PS2 480i). A test's diff
  against the seed is the same instrument as the bench gate. (FD L1297-1303)
- **Time is a variable.** The test advances `millis()`, and `delay()` advances it too. (FD L1305-1307)
- **Two build tricks.** Define the real framesync header's guard on the command line and force-include
  a host version. Wrap force-included C++ in `__cplusplus`, because PlatformIO's Unity glue is C. (FD
  L1309-1314; `platformio.ini` `[env:native]`)
- **Trap.** On a healthy seeded chip, every status reads "settled", so machines "race to the end in
  two ticks". Tests must set the status bits the machines wait on (`STATUS_16` bit 1). (FD L1337-1341)
- **Machine tests assert termination.** Each machine terminates inside its own timeouts, actually
  visits its states, reports completion exactly once, and holds then releases `chipBusy()`. (FD
  L1326-1335)
- **Cannot reach:** the async server, real I²C timing, the OLED, the ISR-driven frame lock. "The bench
  gate stays." (FD L1343-1344)
- **Run with** `pio test -e native`, a single Unity binary of 25 cases that runs in seconds. (CL
  L59-63)

### 3.12 Docs as the rationale store

- **"The code carries no comments."** Measurements, bugs and limits live in `docs/`. (CL L125-126; RM
  L114-116) `numbers.py` checks figures in the prose against their sources: test count, sketch length,
  field and risk counts, Essential counts, `program.cpp` branch values, and static RAM within 512
  bytes. (`tools/checks/numbers.py`)
- **Digest note.** The result is very large docs (`firmware-design.md` 138 KB,
  `timing-and-blanking.md` 261 KB) and measurable drift: Appendix A lists twelve contradictions. Their
  own records show it repeatedly: "repeated in eight places across `docs/` without anyone finding the
  code behind it" (WI L268-270), and a copy "still giving identity 1's numbers five days later with
  every check passing" (`numbers.py:143-146`).

### 3.13 Scope decisions

- **WiFi is not optional.** Upstream added a build without WiFi (`ENABLE_WIFI`); it was deliberately
  not ported, because "a board without WiFi would be a board with the serial console and the OLED
  panel, which is a different product". (FD L1180-1190)
- **No invented curated "looks".** A one-click "CRT look" was dropped because "inventing [values]
  without hardware access would be presenting a guess as a considered default". (MT L108-130)
- **No speculative event counters.** Counters for PLL lock losses and mode changes were not built:
  without bench study of the 25+ assignment sites, a counter "looks authoritative and is actually
  noise". (MT L169-180)

---

## 4. Web interface lessons

### 4.1 Delivery and size

- **The UI lives on LittleFS, not in the firmware.** Generated TypeScript is bundled into one page and
  stored as `/app.html.gz`. Moving it out of PROGMEM gave back 171 KB of flash and made updating the
  interface without reflashing possible. (WI L411-419; FD L2010-2013) The name must be exactly
  `<page>.html.gz`. (WI L413-414)
- **Bundle size over time:**

  | When | Size | Source |
  |---|---|---|
  | early | 69.7 KB | ME L197-198 |
  | after stripping shipped source comments | 62.1 KB | ME L197-198 |
  | README figure | 66 KB | RM L35 |
  | 2026-09-07 | "grew from 66 to 72 KB" | ME L251-252 |
  | (no date) | 71 KB gzip, "either way", minified or not | WI L404-406 |
  | 2026-09-14 | 80 758 bytes sent from LittleFS | WI L288 |

- **The bundle is not minified,** so the served page stays readable in a browser debugger. (WI
  L404-406)
- **Build steps, each failing the build:** `tsc --strict`, ESLint (recommended TS rules plus
  `prefer-const`, `eqeqeq`, unused-variable errors), the esbuild IIFE bundle, then pack. (WI L399-406)
  Seventeen ES modules: "mutable state stays with its writers", and the generated schema is imported
  rather than injected. (WI L375-397)
- **Emergency page.** A ~1 KB PROGMEM page served at `/` when `app.html.gz` is missing. It "depends on
  nothing external, and knows how to upload the missing file". (FD L2008-2017; RM L180-181)
- **Busy page.** When heap ≤ 10 000, `/` returns a small 503 page that explains the board is busy
  sending the interface to someone else, with `Retry-After: 5` and a meta refresh. The retry "succeeds
  on the first attempt five seconds later", and refusing costs no file read. (WI L296-301)

### 4.2 Binary state transport; the schema stays on the client

- **The board sends values only.** State is 18 windows covering 563 bytes. The schema stays on the
  client and the schema hash is checked before any write. (RM L40-42; WI L16-25)
- **Other binary blobs:**

  | Blob | Size | Source |
  |---|---|---|
  | telemetry | 960 bytes, against "about five kilobytes of JSON" that "do not pass through this board's heap margin" | IN L153-156 |
  | chip capture | raw 1536 bytes | IN L279-284 |
  | event log | 12-byte entries; "100 bytes over the wire for the eight events a boot produces" | IN L27-35 |

- **Decode on the client.** Diffs and decoding happen in the browser: "raw bytes on the wire, meaning
  on the client". (IN L316-319)
- **One API path in the client.** All calls go through `apiJson<T>`, `apiBytes` and `apiFetch`, typed
  by the generated contract. "Before 2026-09-04 every response was `any`." The refactor found a button
  that had answered 400 since it was written (it sent `hostname` where the firmware read `h`). (WI
  L27-51) The client sends `POST` for routes the contract marks as mutations, otherwise `GET`.
  Firmware accepts both. (WI L38-40; FD L1486-1488)

### 4.3 Polling, not push

- **No push.** The websocket was removed (§P19).
- **Poll cadence.** `poll()` runs every **1.5 s** and awaits completion before rescheduling. Every
  fetch has an 8 s deadline. (WI L188-198)
- **Missed reads are normal.** "The board drops a connection now and then by design — it serves one
  request at a time and the sync watcher blocks inside `loop()` while it probes the input." A missed
  read "does not deserve alarm". (WI L196-201)
- **Refresh values without rebuilding the DOM.** Rebuilding loses focus and slider drags. The
  Behaviour panel skips the rebuild when the JSON is byte-identical, because rebuilding a `<select>`
  closes it. (WI L203-207)
- **Unknown section ids fall back** rather than throwing. A stale id from the URL hash once blanked the
  whole page. (WI L209-212; TO L47)

### 4.4 Tiers: Essential and Advanced

- **Two tiers, not three.** The old "Raw" tier duplicated Advanced. (WI L66-71)
- **What Essential actually contains.** It is 66 fields: **two** with a measured picture effect
  (`VDS_Y_GAIN`, `VDS_Y_OFST`), fifteen measured as moving nothing on a still frame, and 49 never put
  in front of a camera. (CL L158-161) The 2026-09-14 review of the 86 fields then in the tier found
  2, 15 and 69 in those classes, and 14 fields whose locus came only from the datasheet. (WI L73-77)
- **Rule** (CI: `tools/checks/essential.py`): "A field the board rewrites on its own may not be a
  default control."
  - The check greps six modules (watch, tune, detect, measure, mode, framesync) for `GBS::<FIELD>::write`.
  - Twenty fields failed, including every ADC gain and offset and the whole sync-processor group.
  - The fix is almost always to drop `"ess"`, not to stop the firmware writing.

  (WI L79-94; TO L111-136; CL L163-168)
- **The Advanced section shows everything with no selector.** "A filter there would hide the only
  thing it is for." Stored tier choices are validated, and legacy ids map forward. (WI L150-158)
- **Bypass bits are shown inverted,** in one place (`isBypass()` keyed off the name), so a switch
  shows the feature rather than the bypass. The `x` flag opts a field out. (WI L160-169)
- **Gated fields show "inert"** with a button that turns the gating block on. Gate plus dependents fold
  into an accordion. "Writing a gain with its block bypassed does nothing and raises no error. It is
  the trap that cost the most time in the whole characterisation of the chip." (WI L171-186; FD
  L557-559)
- **The Input panel uses the `input.select` command, not the register.** The register is rewritten by
  detection. The third input's tooltip says it has no SOG. The control is presented as a nudge. (WI
  L133-148)
- **Limits come from the board, not page constants.** The integer-stretch ceiling moves with the
  source, so the page asks `/api/timing/integer` once (`srcWidth`, `frameWidth`). (WI L303-312)

### 4.5 The "Now" panel sentences

- **A pure function.** `core/verdict.ts` maps one `/api/sensors` reading to a tone, a headline and a
  paragraph. Seven of the eight rungs are "a way of not having a picture"; the eighth says what the
  board is sending. (WI L96-100)
- **The eight headlines, in priority order** (FP L46-103):

  | # | Headline | Meaning |
  |---|---|---|
  | 1 | **Sending …** | output size and rate; "does not mean a picture arrived at your display" |
  | 2 | **No signal on …** | no HSync |
  | 3 | **Sync is arriving, and it is not clean** | SOG bad |
  | 4 | **Signal found, settling** | |
  | 5 | **Sync is steady, the source is not measured yet** | |
  | 6 | **No output program loaded yet** | |
  | 7 | **Output is paused** | |
  | 8 | **The video chip is not answering** | |

- **It never claims a picture.** The board cannot see its own output. The healthy rung says what it is
  generating and that a dark screen means a downstream fault. (WI L104-107)
- **It never reads the PLL lock bits.** `STATUS_MISC_PLLAD_LOCK` read `8 7 8 8 5 0 8 8 8 0` (n/8)
  across ten reads on a board with a good picture. `tools/checks/verdict.js` fails if either PLL lock
  name appears. (WI L109-113; SE L419-444)
- **The rung order is the contract.** A missing signal must beat every downstream complaint, and the
  check drives "everything wrong at once". (WI L115-118)
- **Every headline must appear in `first-picture.md`;** the check enforces this. (WI L129-131)
- **Refresh cost.** The panel refreshes every second poll: free heap bottomed at 13 944 with the panel
  open against 14 384 on a panel that fetches neither. It renders nothing until the register block has
  arrived. (WI L120-127)

### 4.6 Mock board, smoke tests, static UI checks

- **Mock board.** `tools/mock/record.py` fetches every non-mutating route from a live board (40
  responses with content type and `X-` headers). `tools/mock_board.py` serves them on port 8266,
  acknowledges every mutation and serves the built page. `npm run smoke` opens every section in jsdom.
  CI runs this. The fixtures are "a photograph of one board on one day": re-record after a contract
  change, and expect smoke to fail on keys the photograph lacks — "that is the check working". (FD
  L1316-1324; CL L29-32)
- **jsdom smoke.** `webui_run.js` opens each section plus a bogus one, waits 4.5 s, and fails on an
  empty section or a console error. It "does not click anything". (TO L94-109) Stubbing mistakes
  produce harness bugs that look like page bugs: `/api/journal` is an object, `/api/capture/list` has
  `null` slots. (TO L57-59)
- **The never-answering request test.** A stub that hangs from the third call. The loop must stall
  for the deadline, then resume. The control (abort ignored) must show the calls stopping dead.
  "Without the control the test cannot fail." (CK L92-103)
- **`webui.py` (CI)** catches string-literal drift `tsc` cannot see: undefined `var(--x)`, classes
  with no rule, unknown section ids, theme attribute names, leftover Portuguese in string literals and
  UPPERCASE constants. "Every check exists because the matching bug shipped." (TO L32-51)
- **Template tag checker.** It tokenises the template and keeps a stack. A quote opens an attribute
  value only right after `=`. The content of `script`, `style` and `textarea` is skipped. One orphan
  `</div>` once blanked the last two tabs, far from the edit. (WI L351-371)
- **Theme.** Dark by default (operated next to a TV in low light). "No colour has its only definition
  inside an `@media`." (WI L342-349)
- **Literal labels.** `VDS_HSYNC_RST` and `VDS_VSYNC_RST` are totals including blanking, so cards are
  labelled literally rather than promising "the resolution". (WI L214-220)

---

## 5. OLED menu lessons

### 5.1 Tree and use frequency

- **Root order is "what you do standing in front of the device, from most frequent to rarest".**
  (OM L20-48; ME L301-302) This is a design judgement; there is no usage telemetry.

  | # | Root item |
  |---|---|
  | 1 | Input (cycles the three ADC inputs) |
  | 2 | Timing (eight computed modes, "the only way to change the output on the panel") |
  | 3 | Profiles |
  | 4 | Tuning |
  | 5 | Pause the picture |
  | 6 | OSD |
  | 7 | Current state |
  | 8 | Sensors |
  | 9 | Network |
  | 10 | Reset and restore |

- **Recover the output** sits under Reset and restore. It runs the same DAC/sync-pad output
  re-presentation as the web button. It lives on the panel because "it is the one control still
  readable when that fault is happening" (HDMI dark, chip healthy). "Pressing it again is the intended
  remedy." (OM L72-80)
- **Tuning holds 88 curated fields.** Every field is risk-safe, writable and unsigned, because "the
  panel has no room for a confirm dialog". (OM L82-87; MT L151)
- **Fields are resolved by name** via `registryFind()` on group entry: a linear scan over 958 PROGMEM
  entries, acceptable on navigation. The `REG_F_OLED` flag stayed unused. (MT L141-159)
- **Image items carry `apiName`** so `/api/oled/state` can name them. (OM L65-68)
- **The state screen draws the real output totals,** not a preset name. (OM L60-63)

### 5.2 Memory model

- **A fixed pool.** `itemPool` is a fixed `.bss` array with no heap allocation.
  - `OLED_MENU_MAX_ITEMS_NUM` = 52, `OLED_MENU_MAX_SUBITEMS_NUM` = 12, `OLED_MENU_MAX_DEPTH` = 8.
    (`OLEDMenuConfig.h`)
  - The item struct went from 96 to 52 bytes: children are `uint8_t` pool indices instead of 12
    pointers, and enums and coordinates are `uint8_t`. (OM L301-318)
- **Free on exit.** Dynamic branches are freed when left. A handler that calls `clearSubItems()` marks
  its item "rebuilt", and `popItem()` frees that item's children, so "rebuilt on entry" and "freed on
  exit" are one statement. Without this, visiting nine Picture groups accumulated 75 items. (OM
  L278-299, L350-352)
- **Sized by counting and measuring, not guessing.** The pool size was checked with a menu walk over
  `/api/oled/nav` against `oledItems`, which `/api/version` reports live. (OM L229-272)
  - 2026-09-07: 35 items at rest, peak 43, so a margin of 9.
  - The walk measures a *lower bound*: visiting several groups in one sitting peaked at 45. (OM
    L354-374; ME L299-320)
- **Overflow guards.** The sub-item limit was "checked nowhere" and is now enforced in both
  `registerItem` and `addSubItem`. Pool exhaustion calls `panicAndDisable()`, so "the failure is a
  dead menu rather than a corrupted one". (OM L264-268, L372-374)
- **Draw-it-yourself screens.** A frozen handler that draws its own screens costs 0 extra slots: the
  Sensors page pages through six screens with no item churn. (OM L257-259; SE L558-562)
- **Labels from PROGMEM.** Field-name lists are PROGMEM NUL-separated blobs copied into a shared
  `fieldLabels[12][32]` on group entry. This saved about 2.4 KB of RAM, and is safe because only one
  group is open at a time. (OM L97-99; ME L155-167)
- **Bounded formatting.** `snprintf` into bounded buffers, not `sprintf`. Floats are formatted with
  integer arithmetic, avoiding `%f` and its weight. (OM L325-328; SE L567-568)

### 5.3 Encoder ISR handling

- **ISRs.** Both are `IRAM_ATTR`, on the FALLING edge of CLK (GPIO14) and of the switch (GPIO0). The
  ISR sets a `volatile oledNav` and increments `volatile rotaryIsrID`. The loop clears the nav after a
  tick only if `rotaryIsrID` did not change meanwhile, so an event arriving during the tick is not
  lost. (`OLEDMenuImplementation.cpp:31-32,881-933`; `gbs-control.ino:348-352`)
- **Debounce rule.** "Ignore a direction that just flipped, to filter a bounce mid-detent — but do not
  forget what the last good direction was." Resetting `lastNav` to IDLE made a fast spin read as
  advance-then-retreat. (FD §"Rotary encoder debounce" L1132-1138) The window in code is 60 ms.
  (`OLEDMenuImplementation.cpp:895`)
- **Reversal.** `REVERSE_ROTARY_ENCODER_FOR_OLED_MENU` and `…_FOR_OSD` are both 1, flipped together.
  The OSD and field-edit handlers undo the menu's reversal before applying their own. (OM L196-202)
- **Remote navigation.** `/api/oled/nav?n=up|down|enter` writes `oledNav` exactly as the ISR does,
  including the ID increment, so the same code path is exercised. (OM L391-393)

### 5.4 Ticks, the handler contract, freeze timeouts

- **Ticks.** Refresh every 50 ms. Screensaver after 180 s idle, redrawing every 5 s. (`OLEDMenuConfig.h`;
  OM L122-138) A redraw costs about 41 ms of I²C, "four fifths of its own interval". The screensaver
  dropped the loop's recent peak from 76 to 51 ms. (FD L175-178)
- **Handler contract.** A handler is `bool handler(manager, item, nav, isFirstTime)`. It can build
  children, take the screen with `freeze()` (it then owns input until `unfreeze()`), or refuse. The
  manager delivers `IDLE` even for ENTER. (OM L153-172)
- **Freeze timeouts and why:**

  | Handler | Timeout | Why |
  |---|---|---|
  | input | 1 500 ms | |
  | reset | 2 000 ms | then `ESP.reset()` |
  | current state | 2 000 ms | without a source |
  | timing apply | **12 000 ms** | the two-pass apply takes about 12 s; releasing sooner leaves the person navigating mid-change |
  | field edit | 6 000 ms idle | |
  | OSD | 8 000 ms idle (`OSD_TIMEOUT`), counted down on screen | |
  | Sensors | 60 000 ms idle | |
  | profile load | 2.5 s | |

  (OM L174-186, L401-406; SE L546-547)
- **Self-confirming revert.** The panel confirms its own revert after 3 s (`revertConfirmAt`), because
  the revert arms only when the queue reaches the request. The profile menu once reverted silently 15 s
  after use because nobody confirmed. (OM L204-219)
- **Driving the menu from scripts** (ME L322-335; OM L395-397):
  - The cursor never reads 255 at the root; 255 means the status bar is selected, in submenus only.
  - Frozen handlers leave on `enter`, not by cursor.
  - The panel returns on its own timeouts.
  - Read state and inject navigation in separate calls, because the injection is consumed on the next
    loop pass. "Measuring the other way cost a wrong conclusion about a handler that was correct."

---

## 6. Diagnostics and instruments

Each instrument exists "because a specific question could not be answered", and was "built during an
investigation that was stuck without it". (IN L3-15)

| Instrument | Question | Cost / storage | What it actually caught |
|---|---|---|---|
| **Write journal** | "Was it me or the firmware?" | 96 × 8 B ring, 768 B; hook on raw bus write | Revealed that F3 had not actually been flashed (origins read `firmware` where F3 says `watch`) (FD L446-449). Too small for a preset load (> 400 writes) (TB L921-922). |
| **Telemetry** | "Still, oscillating or drifting?" | 6 lanes, 0.5 s sampling, 96 samples = 48 s, 960 B; binary | Lane design: six small cards rather than one multi-axis graph, which "is the most reliable way to make two series look like they cross when they do not"; `TELE_VSACT` means "vertical being measured" (200–1300 line window), not the chip's discrete-VSYNC bit, which SOG sources never raise (IN L132-162) |
| **Video-path probe** | "What was the path doing during the failure?" | 48 × 6 B = 288 B, one sample per loop pass after an apply: `vcount`, sync/blank signals, 4 FIFO full/empty pairs | Distinguishes "output stopped" from "FIFO overflow" from "no data" (IN L168-188). The "HDMI dark" FIFO pattern turned out **not diagnostic**: a live picture after apply-and-revert shows it too (SE L473-479). |
| **Crash breadcrumb** | "How far did the code get?" | RTC memory, 20 B | Steps numbered with slack (0, 10, 12, 20, 22, 24–26, 30, 32, 40, 50, 60) so new steps fit; names served from the firmware. Located the timing-apply lockup step (TB L4352-4353). "Reset with the button, not the socket" (IN L205). The `bcLine` race hypothesis measured no effect, 0 of 3 (IN L244-247). |
| **Chip capture + diff** | "What differs between the working and the failing state?" | 1536 raw bytes per slot, 4 slots; `.met` separate | Found the mode-nudge writes (FD L254-256); "no configuration byte differs" between picture and black (IN L268-271) |
| **Event log** (sixth) | "What did the board do?" without a serial cable | 32 × 12 B = 384 B; codes only, words in the UI | 9 codes: boot+reason, source found/lost, standard changed, program loaded, timing queued/reverted, clock adopted, heap low (IN L19-74) |
| **Sensors + scheduler instrumentation** | "What is it doing now?" | `resetReason`, `resetInfo`, uptime, heap, VCC, RSSI, CPU MHz, `chipBusy`, `schedulerHeld`, loop pass, slowest tick, PLL locks as n/8 | `resetInfo` found exception 9 (FD L468-479). Locks as counts showed the input-PLL bit is noise, not state (SE L419-444). |
| **Display-clock counters** | "Did the watchdog ever act?" | `doneStageBranch/Counter/AgeMs/Pending`, `watchdogAdopts/Represents/AgeMs` | Makes "never on a healthy board" "a number rather than a hope" (FD L1552-1574) |

### 6.1 Details worth keeping

- **Journal.** `journalSequence()` is monotonic, so readers detect gaps. The origin falls back to
  `firmware`, "the honest default". (IN L113-119)
- **Event log design.** "The board records numbers and carries none of the words." Codes come from the
  contract's `EventCode` enum. Translating the log means translating one table in the bundle.
  - Repeats with the same code and operands collapse.
  - Zero events in 60 s of steady operation; events cluster at boot and on changes.
  - Persistence is undecided. RTC was rejected because OTA wipes it. LittleFS needs a rate-limit
    policy for a marginal cable alternating `sourceFound`/`sourceLost`, and collapsing catches only
    identical repeats, "and none is taken here". (IN L27-59)
- **A borrowed clock.** The board has none. `/api/clock?epoch=<s>` lets the browser lend wall time
  on page load. Uptime is subtracted, and `epoch` reads **0** until set ("not an invented time, not
  1970"). It is not persisted across restarts: "a wrong clock is worse than an absent one". Cost: 8
  bytes of state, one route, +16 bytes of static RAM. NTP was rejected because it costs heap and needs
  internet. (IN L76-94)
- **Breadcrumb units come from measurement.** A `uint16` in kHz saturated at 65 535 kHz during a
  162 → 85 MHz ramp of 307 steps, so the unit became 10 kHz. (IN L237-242) Writing breadcrumbs into
  the TV5725 instead was rejected: it adds I²C writes inside the very stretch under study. (IN
  L207-210)
- **Capture file safety.**
  - Format: `.cap` holds raw 1536 bytes; `.met` holds `"GCAP"`, version, `when`, bytes, label[24],
    epoch.
  - `.met` is written **last** and removed **first**, so a half-done save reads empty.
  - The capture is saved one 256-byte segment at a time (peak 256 bytes), from the loop, queued, 202.
  - Metadata is a separate file so the `.cap` can be streamed straight from LittleFS.
  - Version 1 metadata is still read (a short read is accepted, `epoch = 0`). Bumping the version
    without that "would have silently emptied four slots".
  - No compression or encoding, on purpose.

  (IN L279-312)
- **The diff names fields, not bytes.**
  - It reports fields whose *value* changed, with label, group and risk.
  - Rule for byte-wide catch-all fields: omit one when every moved bit is already named by a narrower
    field in the list. When it survives, name the unnamed bits.
  - Status registers, counters, the test bus and the firmware's scratch group `system.state` are
    separated out as "state".

  (IN L321-362)
- **Noise floor.**
  - Two idle captures seconds apart differ in **6 of 1536** bytes (`s0_09`–`s0_12`, `s0_2E`, `s0_2F`).
  - Capture against live agrees on **1529 of 1536**.
  - The ADC offset and gains drift on their own (the chip's auto-offset and the firmware's auto-gain),
    so diffs minutes apart "expected to name the ADC".
  - The old byte-range gate ("segments 1 to 4 are the configuration") missed a real 106 → 170 ADC
    gain drift in segment 5.

  (IN L381-397; TO L187-190)
- **Export diagnostic report.** One button assembles `/api/version`, the journal, the capture list,
  the breadcrumb and the probe into a single JSON, client-side, "without asking the reporter to run
  five `curl` commands". (IN L401-405)
- **Intrusive measurements are opt-in.** Rate measurements retune the test bus and toggle the debug
  pin, so they run only when asked and are queued to the loop. Polling must not disturb the video
  path, and that was verified: eight `/api/sensors` reads left `TEST_BUS_SEL` and `PAD_BOUT_EN`
  untouched. (SE L23-32)
- **Latency phase is per boot.** It is modulo one input period and lands arbitrarily: 805 lines on one
  boot, 216 on the next. It is good for "is it locked", not for absolute lag. (SE L372-379)

### 6.2 What did not pay off

- **The 563-byte snapshot.** It held a third of the evidence and could restore nothing; replaced by
  the full capture. (IN L265-275)
- **A one-shot boolean PLL lock bit,** and a majority-of-five vote: both lie. Report n/8 instead. (SE
  L419-431)
- **Event counters for lock losses and mode changes:** not built, because they would be noise. (MT
  L169-180)

---

## 7. Tooling and bench methodology

### 7.1 Directory roles

| Directory | Needs | What it decides |
|---|---|---|
| `tools/` | nothing | static checks, run in CI |
| `tools/checks/` | board + signal | whether a control moves the chip |
| `tools/bench/` | board + capture card | whether a picture comes out |
| `tools/registry/` | nothing | the generators |

`GBS_HOST` selects the board, default `10.0.0.119`. (TO L3-12)

### 7.2 What runs in CI

From `.github/workflows/build.yaml`, in order:

1. clang-format check
2. `check_routes.py`
3. `webui.py`
4. `numbers.py` (static RAM part skipped, no ELF yet)
5. `build_registry.py --check`
6. `essential.py`
7. `build_contract.py --check` and `contract.py`
8. `platformio test -e native`
9. `npm ci && npm run build`, then the mock board and `npm run smoke`
10. `chip_diff.js --selftest` (fails if the signed-field count is zero)
11. `verdict.js`
12. `platformio run -e d1_mini`, then upload the artifact. Tags publish
    `GBSC-d1_mini-<tag>.bin`.

### 7.3 On-board checks (`tools/checks/`)

- **They verify effects on the chip, not reading the code.** "Twice an interface control accepted the
  write, returned 200 and did nothing": `pic.*` never called `pictureApply`, and `/api/journal/clear`
  was swallowed by route matching. (CK L6-10)
- **Every script checks the schema hash first** and exits 2 on mismatch. Each restores what it moved.
  Exit codes are 0 pass, 1 fail. (TO L192-199)
- **`fields.py`.** One writable safe field per group: write, read back, restore. Gated, critical and
  state fields are left out. It also checks the API's safety gates. (TO L201-217)
- **`prefs.py`.** Read the register a preference should move, flip the preference, read again.
  Survival is checked across a board *restart*, the only trigger left that re-runs the whole load.
  (TO L219-252)

### 7.4 The capture-card rig (NZXT Signal HD60 via ffmpeg DirectShow)

- **Invocation.** ffmpeg via `imageio_ffmpeg`, `-f dshow -rtbufsize 256M|512M -pixel_format yuyv422
  -video_size 1920x1080 -framerate 60 -i "video=NZXT Signal HD60 Video"`.
  (`tools/bench/colour.py:28,49-51`; `framelock.py:131-133`)
- **The pixel format changes per mode.** The card is "stuck at 1920×1080" and presents `yuyv422` on
  some modes and `nv12` on others. Naming the wrong one returns zero frames, "indistinguishable from a
  black picture". `shot.py` tries auto, then `yuyv422`, then `nv12`. (TO L380-384, L904-908;
  `shot.py:1-8,18-29`)
- **Separate "card delivered no frames" from "black frame".** `shot.py` exits 0 with a measurement,
  1 for genuinely black, 2 for no frames, which is a rig fault. Read `/api/sensors`
  (`outRefreshHz`, `stableCounter`, `pll648Locked`) and a chip diff before blaming the board. (TO
  L275-278, L298-317)
- **HDMI lock loss.** After an apply or reset, only a board restart relocked the card ("four times
  that day, reliably"). Wait for three consecutive frames plus 10 s. (TO L280-285) "Idle is not the
  same as ready": wait for idle **and** 16 s more. (TO L287-293) The MS9288A takes its own time to
  re-acquire, so recovery checks wait 20 s. A 4 s wait "turns a success into a false negative". (TO
  L835-838)
- **DirectShow limits.** The device "locks up after half a dozen opens", and each failed open leaves
  an orphaned ffmpeg holding the device. Record one session and slice it by time window. (TO
  L1000-1004)
- **Colour decoding.** The card encodes **BT.601** but ffmpeg assumes BT.709 at 1080. A "34-level
  green compression" that does not exist was measured this way. Decode in a module that encodes the
  rule. (TO L487-497)
- **Timing artefacts.** ffmpeg takes about 1.4 s to open the device before frame 0, so report against
  each clock's own origin. The card at 60 fps against a 59.94 Hz output adds about 0.4 repeats per
  64-frame burst on its own. (FD L1694-1695; TO L513-515, L808-812)
- **Low-rate captures alias.** 2 fps sampling reads a 60 Hz alternation as still, which is "the exact
  false negative". Use a full-rate capture. (TO L599-601, L669)
- **"No Video" placeholder.** It is white text on black, so detect it explicitly rather than by mean
  brightness. (TO L808-810)
- **Disk use.** 90 s at 60 fps is 5 400 PNGs, about 3 GB. (TO L815-816)

### 7.5 Picture verdicts

- **Pattern.** `verify_picture.py` works against the 240pTestSuite SMPTE bars. (TO L376-378)
- **The `TORN` verdict.**
  - Measure: the 5th-to-95th percentile range of row-to-row edge spread, threshold 3.0.
  - Clean frames measure 0.00, a wrapped line 1.00, torn frames 17–90.
  - Standard deviation could not tell one wrapped line (sd 4.98) from a torn frame (sd 6.55).
  - The tearing test runs only on the bar pattern (≥ 5 colour transitions, saturation > 40).
    Otherwise the verdict is `SOME PICTURE`: a PS2 game measured 434 and 533 px and was clean.

  (TO L343-364)

### 7.6 Method errors they documented (and the rule each produced)

1. **Measuring the reverted state.** A confirm sent before the revert armed meant captures
   photographed the restored preset, counted as success. "No success rate published before this fix
   holds." Rule: after measuring, check the cause is still there. (TB L23-46, L910-912)
2. **Too few repetitions.** "Three of three" against a ~1/3 rate comes up once in 13. Rule: measure
   the rate before looking for a pattern. (TB L906-908; TO L407-409)
3. **The script encodes the wrong rule.** It produces "intermittency indistinguishable from a defect".
   Rule: suspect the script first. "Reading the assertion against the code costs minutes; hunting a
   one-in-twenty event costs an afternoon." (TB L914-942; CK L39-58)
4. **A mechanism is not a remedy.** Four true rescue observations suggested a remedy that measured
   2 of 5 against 15 of 15. (TB L926-936)
5. **Watcher off is not the user's state.** With the watcher off the output ran 17% fast. The board
   under test was not the user's board, and a still capture cannot see a rate error. Rule: measure in
   the user's state, and print rate disagreement > 0.2% with every result. (TO L416-442)
6. **Stale constants.** `--hz 59.823` was run against a board producing 59.937. Rule: `--hz` defaults
   to the board's current rate and is refused if more than 0.05% away. (TO L444-458)
7. **A bench number belongs to its input.** The same preset loads a different geometry on 240p and
   480p. Results carry an input signature (`board.py`). Source identity uses only `VTOTAL`,
   `HPERIOD_IF` and `VPERIOD_IF`: `HTOTAL` counts at the preset's sampling rate (2345 against 2558,
   9% apart). (TO L321-337, L465-479; CL L177-179)
8. **Applies are not independent.** "The first apply after a restart is clean, the third never is."
   Rule: one restart per point. (TO L544-548)
9. **Fair comparisons.** Interleave conditions rather than blocking them; equal waits for both;
   identify the source. (TO L959-967)
10. **Null controls.** `framelock.py control` and `pseudointerlace.py` (effect must beat control 3:1;
    measured 86:1). A recording "published as a failure before the matched control existed" was not
    one. (TO L602-604, L795-799; FD L1835-1840)
11. **"Nothing moved" must prove the instrument could see movement.** A phase step of exactly 0.000
    on every pair is also what capturing one frame 24 times returns; hashing frames separated the two.
    (TO L612-616)
12. **Selecting on the property under test** makes "a guard checking its own arithmetic". Select
    pixels by magnitude only. (TO L673-677)
13. **Fix crops and bounds once.** Recomputing per frame created jitter and "a confident false positive
    at t=5.76". Fixing the crop took the noise floor from 17% to 2.3%. (TO L641-644, L499-503)
14. **Missing readings printed as success.** `(None or 0) > 2.0` printed "clean". Print `NO READING`.
    Retry every HTTP read so a crash does not discard spent restarts. (TO L555-559)
15. **`finally` is not enough.** SIGTERM and SIGPIPE skip it. Provide `--restore` and check for
    orphaned runs before believing the board is clean. (TO L679-688)
16. **Wait for the chip** before applying, or the 409 makes the sweep measure the previous mode. Run a
    known-good control between points and stop when a control fails. (TO L918-932)
17. **Parity, not magnitude.** A 240p/480i mode change is a change of parity (`VPERIOD_IF` 525 vs
    524), not of magnitude. A threshold of "> 2" called two changed runs clean. (TO L801-806)
18. **`STATUS_IF_INP_INT` lies.** It reads 1 on 240p as well. Use `VPERIOD_IF` parity. (TO L574-577)
19. **ESP restart is not a power cycle.** Every "boot" in a campaign was an ESP restart with the
    scaler powered. (TB L2521-2537)
20. **Black output vs black content.** "The detector cannot tell a black output from black content."
    (FD L1836-1838)
21. **Settle before diffing.** A diff 4 s after an upload reported 13 changed settings, all boot
    completion. `chip_diff.js live` refuses (exit 2) on `doneStagePending`, `stableCounter < 8` or
    `chipBusy`. (TO L178-185)
22. **Don't hardcode waivers.** `--allow-auto` derives 88 waived fields from the automatic modules,
    and it is a lower bound because writes reached through helpers (`PA_SP_S`) are missed. (TO
    L162-176)
23. **Don't trust the obvious ellipse fit.** The monoscope diagonals sit at 37°, not 45°. A fit that
    kept them reported "6.8% stretched when it is 1% narrow". (TO L750-755)

### 7.7 Flash and backup scripts

- `flash.sh` flashes over HTTP and `upload_ui.sh` sends `app.html.gz`. Both turn the sync watcher off.
  (TO L1155-1156) They no longer work for firmware (§P11).
- Rate limit 15k, because 25k failed. Restart before retrying. (TO L1158-1165)
- `backup.sh save|restore` carries preferences and captures, including subdirectories. (TO
  L1150-1153)
- Flash firmware and UI **together**. (CL L56-57; TO L1167-1169)
- Recovery without a network: serial, `?` opens the console, `wifi <ssid> <pass>`. (RM L183-184; FD
  L1157-1159)
- The console: `help`, `get`, `set`, `find`, `dump <segment>`, `wifi`, `version`, `exit`. It uses the
  chip's datasheet field names, because "serial matters precisely when the network is not working".
  (FD L1990-2006)

---

## 8. Open problems and unknowns they recorded

**Platform and firmware**

1. **HTTP OTA is broken.** The heap at idle (~15 KB) is too small for the async upload; upstream had
   22 KB. USB is the only reliable path. (TB L4687-4691)
2. **The heap baseline is low and the warning thresholds need revisiting.** The working band sits at
   the 15 KB floor they hold themselves to, with transients near 10 000. (ME L254-262)
3. **Event-log persistence policy is undecided.** The unsolved case is an alternating
   found/lost pair. (IN L49-55)
4. **The phase-measurement ISR is not detached on timeout.** (FD L1926-1929)
5. **The RGB/HV path's blocking waits are unconverted,** and there is no RGB/HV source on the bench.
   (FD L310-313) Holds on the sync-loss paths are not fully verified: "that test is owed". (FD
   L315-317)
6. **Seven mutual module pairs remain.** (AR L144-152) The include-cycle checker is claimed but
   absent (Appendix A).
7. **An unexplained black frame after five restarts plus three applies (F3 gate)** was later
   attributed to resets plus HDMI relock. The general rule stands: capture and look before reflashing.
   (FD L442-466)
8. **The input-PLL lock bit's dither is unexplained.** Treat it "as a property of the bit". (SE
   L442-444)
9. **`prefs.py`'s survives-a-preset-load check fails intermittently,** unexplained. (TO L238-241)

**Video chain** (summarised because it bounds the platform)

10. **Why a second timing pass cures black output is unknown.** `DEFAULT_PASSES = 2` costs about 12 s
    per apply against about 4 s; one pass gave 7 of 15 and two passes 15 of 15 (480p). (TO L934-951;
    RM L231-235)
11. **Board lockups after a timing apply** sometimes need a manual restart. The cause is not
    isolated; a 1250-line vtotal reset the board at "step 30" at every memory clock. (RM L245-247; TB
    L4350-4357, L4365-4370)
12. **What slew rate the MS9288A follows** without dropping link is unknown. The frame-lock freeze is
    "the combination or the repetition" of measure and ramp. Audio dropout and the no-generator path
    are unmeasured. (FD L1762-1766, L1797-1802, L1842-1844)
13. **Display-clock adoption is a race.** The window is a stable counter of 35–45, and adoption lands
    at 36, "inside the window by one". A miss after an OTA restart was observed once (70.1 Hz
    free-run). (FD L1543-1556; TB L4062-4088)
14. **A sticky chip-internal state** survives every soft reset and is re-rolled only by the program
    write, at about two good boots in five. (TB L2578-2587)
15. **The MS9288A cannot be observed or controlled.** No register map; `MCUSEL` floating. Its
    enhancement block runs at vendor defaults on top of the TV5725's processing. (DS L139-175,
    L184-194)
16. **Untested fields.** 739 of 958 fields have never been written on a bench, and 49 Essential fields
    have never been in front of a camera. Noise-reduction loci come from the datasheet only. The
    corrector thresholds' signedness is unknown. (CL L147-161; TO L691-693; DS L93-97)
17. **PAL is largely unmeasured.** The 576p capture window row is "as unmeasured as everything else PAL
    here". (FD L948-951)
18. **No working PIP path exists.** (MT L96)
19. **Unidentified parts:** the SDRAM part number, and whose EEPROM `0x50` is. (DS L161-163, L207-212)
20. **A sleep timer took the board off the network.** Eight hypotheses were eliminated without
    finding the cause, so the feature was removed. (FD L1273-1276)

---

## 9. Carrying it over

### 9.1 Rules worth carrying over (each earned by a measured failure)

**Concurrency and platform**

1. **Network callbacks never block, never yield, and never do filesystem walks.** Handlers validate,
   enqueue, and answer `202` (accepted), `409` (busy) or `200` from in-memory caches. The loop does
   the work, and clients poll a sequence number or list. (P1, P2)
2. **The I²C bus layer never yields between the pieces of one access.** Read exactly the byte count
   `requestFrom()` returns. Burst at most 16 bytes, check the return value, fall back to byte-by-byte.
   (P4, P5; §1.2)
3. **WiFi mode changes, credential changes and restarts run from the loop,** never from a callback.
   (P3)
4. **Every multi-step chip sequence is a state machine** with `tick()` and `busy()`, deadlines
   instead of `delay()`, and one `chipBusy()` gate that the API answers with 409. A depth guard
   refuses re-entrant scheduling. (§3.3)
5. **The main loop is a table of named ticks with cadences,** each timed. Publish the longest pass,
   the slowest tick and the held count from day one. (§3.3)
6. **Bound waits by time, not iteration count.** Never disable the watchdog to "protect" a spin.
   Detach ISRs on every exit path. (P12)

**Data and observability**

7. **One bus hook journals every register write with an origin from an RAII scope.** Origins live in
   the contract. (§3.5)
8. **One command table (name, risk, argument count, handler) is shared by every panel.** Critical
   operations need `ack` in the call itself. (§3.4)
9. **Single sources of truth with generators,** `--check` modes in CI, and `static_assert`s pinning
   wire enums. The UI and the firmware share a schema hash and an API version, and the UI refuses to
   write on a mismatch. (§3.8)
10. **Preferences are keyed by name hash and loaded tolerantly.** No write at boot; deferred, coalesced
    single-buffer writes; remove before open; metadata written last and removed first. (P14, P15;
    §3.6)
11. **Anything that can blank the output arms a timed revert** that restores fields, restores the
    clock after the registers, then resets the path. The local panel confirms; the remote browser
    does not. (§3.7)
12. **Binary values on the wire, the schema in the UI bundle, decoding on the client.** The capture
    diff runs on the client from the same code the CLI tool bundles. (§4.2, §6.1)
13. **Instruments early:** reset reason and info, uptime, heap, loop and tick timing, an event log as
    numbers, the journal, and full chip capture with a field-level diff that separates configuration
    from state. (§6)

**Memory and safety**

14. **Static RAM is budgeted per change, measured from the ELF.** `PROGMEM` and `F()` for every
    constant; static rings rather than heap objects for instruments. Measure heap at idle, under load
    and while serving the page. (P9, P10)
15. **CSRF middleware** on `Sec-Fetch-Site` and `Origin`. Emergency fallback page in PROGMEM. A busy
    page with `Retry-After` when heap is low. (P18; §4.1)

**Product and UI**

16. **A field the board rewrites on its own may not be a default control.** A control is one chip
    field or one firmware algorithm; never create a second owner. (§3.10; §4.4)
17. **Derived values are recomputed from the source each time.** Test that they change when the world
    does. (§3.10)
18. **The status sentence never claims a picture.** The rung order is a contract, and every sentence
    is documented for end users. (§4.5)
19. **Prefer "unknown" to invented data:** `epoch = 0` until the browser lends a time; PLL lock as
    n/8; "never probed" rather than "safe". (§6.1; §3.9)

**Testing and bench**

20. **A host build with a fake chip seeded from real captures,** tests that set the status bits the
    machines wait on, and a mock board for UI smoke tests in CI. (§3.11; §4.6)
21. **Bench discipline:**
    - verify the picture, not the registers;
    - record the input signature with every result;
    - use null controls, repetition and interleaving;
    - re-check preconditions after measuring;
    - suspect the script first;
    - after flashing, read something only the new firmware can say;
    - distinguish "no frames" from "black frame".

    (§7)

### 9.2 Things to do differently in a from-scratch rewrite

These are digest recommendations, inferred from their record. They are not claims made in their docs.

1. **Budget heap headroom for OTA from the start.** Their idle heap (~15 KB) made HTTP OTA impossible,
   while upstream at 22 KB worked. Make an idle-heap floor (for example ≥ 22 KB, measured) and a real
   OTA round-trip release gates. Consider an update mode that stops polling-heavy services before
   accepting an image. (P9, P11)
2. **Choose and document the flash layout deliberately.** With `4m1m` there is about 2 MB of OTA
   staging space and about 1000 KB of LittleFS. Their docs mis-describe it. (§1.4)
3. **Build state machines and the scheduler in from the first commit.** They retrofitted 122
   `delay()` calls and 8 nested scheduler calls in F2, and still could not convert the RGB/HV path
   without a source. (§3.3)
4. **Avoid route-order fragility by construction.** Use exact-match routing (or one dispatcher route
   over a contract-generated table) instead of relying on registration order; keep the CI check
   regardless. Verify which `AsyncURIMatcher` types the chosen library version offers. (P6)
5. **Make the async-context rule precise.** List exactly what may run in a callback; their rule says
   "no filesystem" while uploads and file serving still touch LittleFS there. Consider whether a
   synchronous loop-context server would remove the whole bug class: the board already "serves one
   request at a time" (WI L197). This is unmeasured; evaluate the cost of blocking the loop during the
   ~80 KB page send. (P1, P2)
6. **Put size checks after the build in CI,** so the 512-byte static-RAM rule is actually enforced.
   (P10)
7. **Set the OLED driver's I²C frequency explicitly** (its 700 kHz default otherwise lands on the
   shared bus), and cut the OLED's share of the loop: ~41 ms per 50 ms redraw. Use dirty-region or
   on-change redraws instead of a fixed 50 ms refresh. (§1.2; P16)
8. **Classify field risk against the picture as well as bus survival.** Default unprobed fields to
   "unknown", not "safe". (§3.9)
9. **Keep the rationale near the code, verified where possible.** Their no-comment policy plus 400+ KB
   of prose drifted measurably (Appendix A). Short "why" comments next to load-bearing constants, plus
   a smaller set of generated or checked docs, may age better. This is a judgement call; they chose
   the opposite deliberately.
10. **Decide event-log persistence up front,** including the alternating-pair case, and decide how
    instrument data survives OTA (RTC does not). (§6.1)
11. **Pace polling and bench reads by design.** For example, rate-limit expensive routes such as
    `/api/dump` server-side, since six back-to-back calls could reset the board. (P9)
12. **Treat GPIO0 (encoder push) as a boot strap.** Document "do not hold the knob at power-on".
    (§1.3)
13. **Assume the scaler, the clock generator and the SDRAM are not reset by an ESP reset.**
    Initialisation must not depend on power-on defaults, and a deterministic reinitialisation from
    any prior state is a requirement. (§1.5)

---

## Appendix A. Contradictions found while reading (treat single statements with care)

| # | Topic | Statement A | Statement B | What the code or toolchain says |
|---|---|---|---|---|
| 1 | UI heap warning | ME L235: status bar "turns red below 14 KB" | WI L265-271: real floor 10 000, amber at 12 000 | `public/src/main.ts:218-219`: `HEAP_DANGER = 10000`, `HEAP_LOW = 12000`. WI is current. |
| 2 | Network flash rate | WI L418-419: works "at 25 KB/s" | TO L1158-1162: 15k default "because 25k does not work" | TB L4687-4691 (2026-09-19): OTA does not work at 15k or 10k. |
| 3 | Flash layout | ME L7: "the rest given to LittleFS" | FD L2010-2013: OTA image must fit in 311 KB | `eagle.flash.4m1m.ld`: ~1000 KB LittleFS, ~2052 KB empty. Both statements fit an older layout. |
| 4 | Host build size | FD L1282-1283: "Twenty-three … eighteen tests" | CL L61-62, L104 and AR L156: twenty-five modules / cases | `platformio.ini` `[env:native]` lists 25 modules (numbers.py checks only CL and AR). |
| 5 | Include-cycle check | AR L139: a check script in `tools/checks/` walks `#include` | — | Not found in `tools/` or `tools/checks/`. |
| 6 | Documented register count | TO L1062: "569 documented registers" | DS L55: "558 registers" | Probably different extraction scopes; unresolved. |
| 7 | MS9288A input | SE L596: "parallel RGB output" | DS L145-148 and SE L607: re-digitises the analogue output | DS is right: the analogue RGB is re-sampled. |
| 8 | ArduinoOTA | RM L76-78 and `git 0e12453`: "the clock dropped from 162 to 81 MHz" | (task framing: "CPU clock") | CPU is 160 MHz; 162/81 match video clocks. Unverified which clock. |
| 9 | Static RAM figure | RM L190: 47 876 | CL L131 and AR L142: 47 940 | Snapshots at different times, within the 512-byte slack. |
| 10 | Sketch length | FD L422: 627 lines (at F3) | AR L20: 632 lines | Moves over time (numbers.py checks AR). |
| 11 | OLED pool at rest | OM L340-349: 40 / 39 | OM L360 and ME L306: 35 | Different trees over time (2026-09-07 is current). |
| 12 | Essential-tier size | WI L75: 86 fields (2026-09-14 review) | CL L158: 66 fields | The tier shrank after the review (20 moved to Advanced). |

## Appendix B. Numbers quick reference

| Quantity | Value | Source |
|---|---|---|
| User RAM / static RAM baseline | 80 KB / 47 940 B (58.5%) | CL L130-131 |
| Static RAM growth allowed per change | 512 B | CL L130 |
| Idle free heap (2026-09) | ~15 KB (14 424–15 736; 15 040) | ME L245; WI L277 |
| Page-serving floor | ≤ 10 000 B refused (503) | `network.cpp:251`; WI L269 |
| Heap cost of serving the 80 KB page | ~3.3 KB | WI L288-289 |
| Heap cost of 10 duplicate route handlers | 1.8 KB | FD L1499-1502 |
| Loop pass at rest | 51 ms (screensaver) / 76 ms (menu awake) | FD L157-163 |
| OLED redraw | ~41 ms per 50 ms refresh | FD L175-177 |
| Subsystem ticks combined | 1 ms | FD L165-167 |
| Software watchdog window | ~3.2 s | FD L1870-1871 |
| Phase sample deadline | 100 ms (measured 100.1 ms) | FD L1912-1914 |
| Program write | 432 B, ~0.5–0.7 s of I²C | TO L1116; FD L347, L401 |
| TV5725 max read burst | 16 B | FD L537-538 |
| I²C clock | 400 kHz | `videopath.cpp:667` |
| Journal ring | 96 × 8 B, fills in 2 min 24 s idle | IN L107-111 |
| Telemetry ring | 96 × 10 B, 0.5 s, 48 s | IN L143-144 |
| Probe ring | 48 × 6 B | IN L185-186 |
| Event ring | 32 × 12 B = 384 B | IN L34 |
| RTC user memory | 512 B (breadcrumb 20 B) | IN L39-40 |
| Chip capture | 1536 B, 4 slots | IN L279-280 |
| Registry state windows | 18 windows, 563 B | RM L40 |
| Fields / critical / RO registers | 958 / 175 / 48 | RM L29; DS L56 |
| OLED pool | 52 slots × 52 B; at rest 35, peak 43 | OM L229; ME L306-307 |
| Revert window | 20 s | FD L1391; RM L84-85 |
| Prefs save delay | 3 s after the last change | FD L1364; `prefs.cpp:261` |
| Poll interval / fetch deadline | 1.5 s / 8 s | WI L190-191, L38-40 |
| Timing apply (2 passes) | ~12 s | RM L234; OM L184 |
| Output tracks source (Si5351 adopted) | 0.07 ppm | FD L1673 |
| Pixel clock ceilings | DAC 162 MHz; HDMI converter 165 MHz | DS L149-151 |
| Practical vertical ceiling | 1080 lines (converter); VTOTAL_SAFE 1200 | DS L195-199; TB L4365 |
