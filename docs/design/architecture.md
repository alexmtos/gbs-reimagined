# Architecture

For contributors. The map of the firmware and its companions; the reasons are in the
[ADRs](../adr/), the video path in [video-engine.md](video-engine.md), the interfaces in
[ux.md](ux.md).

## System context

```text
                     ┌──────────────────────────── RetroScaler GBS-C ───────────────────────────┐
 console ─ analogue ─┼─► TV5725 scaler ──► DAC ──► VGA / RGBS out ──► MS9288A ──► HDMI out ──────┼─► display
                     │      ▲   ▲ I2C            ▲ pixel clock                                   │
                     │      │   └────── ESP8266 ─┴─ Si5351          SSD1306 OLED + encoder        │
                     └──────┼──────────────▲───────────────────────────────▲─────────────────────┘
                            │              │ Wi-Fi (HTTP, polled JSON)     │ USB serial (CH340)
                            │        phone / PC browser            terminal, scripts, switchers (SVS)
```

## Repository

| Path | What | Built with |
|---|---|---|
| `firmware/` | ESP8266 firmware, host tests | PlatformIO (ADR 0010) |
| `web/` | Web UI, embedded into the firmware image | pnpm, Vite (ADR 0009) |
| `schema/` | Single sources of truth: registers, programs, settings, translations | — (ADR 0005) |
| `tools/` | Code generation, bench verification, board helpers, size budgets | uv / Python 3.12 |
| `installer/` | Browser installer site (ESP Web Tools, Improv) | static page built with Vite (ADR 0012) |
| `docs/` | Research, ADRs, design, hardware, user guide (EN + pt-BR), generated references | — |
| `.claude/` | Claude Code configuration: rules, skills, agents, hooks | — |

## Firmware layers

Dependencies point down only. A module includes headers from its own layer or below.

| Layer | Directory | Contents | Knows about |
|---|---|---|---|
| 0 · platform | `platform/` | flash constants (`PROGMEM` shims), time source, logging | nothing |
| 1 · hal | `hal/` | I2C bus, Si5351, encoder, OLED transport, vsync probe pin, board profile | platform |
| 2 · chip | `chip/` | TV5725 register access, generated fields, programs, field table | hal |
| 3 · video | `video/` | signal snapshot, tuning tables, Detector, Loader, Settler, Watcher, FrameLock | chip, hal |
| 4 · core | `core/` | scheduler, settings store, profiles, status model, commands, events, journal, storage | video and below |
| 5 · front ends | `net/`, `ui/` | web server + API, Wi-Fi/provisioning/Improv, serial console, OLED menu | core and below |
| app | `main.cpp` | composition: constructs everything, registers scheduler tasks | all |

Rules enforced in review (and, where possible, by a check script):

- **Front ends never call `video/`, `chip/` or `hal/` directly.** They read the status model and run
  commands. One command table serves the web API, the console and the OLED, so the three can never
  disagree about what an action does.
- **One owner per chip field.** A setting's apply function and an automatic algorithm never write
  the same field (ADR 0007).
- Only the main loop touches I2C or the filesystem (ADR 0003). Interrupt handlers only record
  timestamps or encoder steps into `volatile` storage.

## Concurrency and the main loop

There is one thread of control: `loop()`. It runs the scheduler, which ticks a fixed table of
tasks, each with a cadence:

| Task | Cadence | Notes |
|---|---|---|
| `net` | every pass | `server.handleClient()`, DNS for the captive portal, mDNS |
| `console` | every pass | serial bytes → Improv demultiplexer → command parser |
| `commands` | every pass | executes queued commands from all front ends, in order |
| `video` | 20 ms | the active machine's `tick()` (Detector/Loader/Settler/Watcher) |
| `framelock` | 1 s | ratio check when stable |
| `status` | 250 ms | refreshes the status model from the signal snapshot and machine state |
| `oled` | 50 ms, dirty only | redraws changed tiles only |
| `storage` | 500 ms | writes settings 3 s after the last change, one file per pass |
| `housekeeping` | 3 s | chip IRQ clear, heap and loop-time statistics |

A task's `tick()` returns quickly; anything longer is a machine that advances one step per tick
and waits on deadlines, never on `delay()`. The scheduler records each task's longest run and the
longest loop pass; both are visible in diagnostics. A "video busy" predicate (OR of the machines'
`busy()`) makes commands that would interleave answer *busy*.

## Data flow

```text
 web / console / OLED ──► command queue ──► command handler ──► settings store ──► apply()
                                                     │                                 │
                                                     └──► video machines ◄─────────────┘
 chip status registers ──► signal snapshot ──► machines ──► status model ──► web / console / OLED
```

- **Settings store:** values indexed by generated `settings::Id`, persisted by id hash in
  `/settings.bin` (header with magic, version, CRC32). Unknown hashes are ignored, missing ones take
  defaults, so the schema can evolve. Writes are deferred and coalesced; nothing is written at boot.
- **Profiles (M2):** one file per slot, same format plus a name and an input signature.
- **Status model:** a plain struct with a sequence number, rebuilt every 250 ms. `GET /api/status`
  serialises it with bounded `snprintf_P`; the OLED and the console read the same struct.
- **Events:** a ring of numeric event codes with arguments (boot reason, source found/lost,
  standard changed, program loaded, revert, clock adopted, heap low). Words live in the
  translations, not in the firmware.
- **Write journal:** every chip write passes the `WriteObserver` hook with the origin set by a
  scope object (web, serial, oled, detector, loader, watcher, framelock…), kept in a ring for
  diagnostics.

## HTTP API (sketch, contract in `schema/api.yaml` from M1)

| Route | Method | Purpose |
|---|---|---|
| `/` | GET | the embedded UI (gzip, ETag) |
| `/api/status` | GET | status model (polled every 1.5 s) |
| `/api/settings` | GET | all values + schema hash |
| `/api/settings` | POST | set one or more values (answers *accepted*; risky ones arm the revert) |
| `/api/command` | POST | run a named command (`keep`, `revert`, `calibrate`, `reboot`, `factoryReset`) |
| `/api/profiles` | GET | the profiles in use, the active one and the settled source (ADR 0014) |
| `/api/profile` | GET/POST | export one profile (`?slot=n`); load, save, rename, re-sign, delete, import |
| `/api/svs` | POST | a switcher line as the body (`SVS NEW INPUT=3`, `remote prof3`) |
| `/api/backup` | GET | settings, level calibration and every profile, streamed |
| `/api/levels` | POST | a backup's level calibration put back |
| `/api/events`, `/api/journal` | GET | diagnostics rings |
| `/api/wifi/scan`, `/api/wifi` | GET/POST | provisioning |
| `/api/update` | POST | an update file (`.ota.bin.gz`, multipart), checked before any write: gzip naming this board ([ADR 0012](../adr/0012-distribution-and-releases.md), `core/update_check.*`) |

Cross-site requests are refused (`Sec-Fetch-Site` / `Origin` check); the API is not authenticated,
like every device on a home network, and says so in the user guide.

## Safety mechanisms

- **Revert:** a risky change snapshots the previous settings and program, arms a deadline, and
  restores both unless `keep` arrives (ADR 0006 for the clock order: registers, then clock, then a
  path reset).
- **Safe mode:** holding the knob during the boot splash starts with defaults without loading the
  saved settings file.
- **Watchdog-friendly:** no loop pass may approach the ESP8266 software watchdog (~3.2 s); the
  longest pass is published.
- **Brown-out awareness:** the scaler is probed every few seconds; "scaler not answering" is the
  first status headline.

## Testing strategy

| Level | What | Where |
|---|---|---|
| Generators | schema validation, `--check` freshness | `tools/codegen`, CI |
| Host unit | chip access, Si5351 math, settings store, machines against the fake TV5725, command parser | `firmware/test`, `pio test -e native` |
| Web unit / e2e | components and API client against MSW; full pages in Playwright | `web/` |
| Bench | picture verification on the capture card per milestone gate | `tools/bench`, `bench-verify` skill |

Memory limits are in [memory-budget.md](memory-budget.md).
