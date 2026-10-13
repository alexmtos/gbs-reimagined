# GBS Reimagined: ESP8266 firmware technology stack

Research date: **2026-10-13**. Target hardware: an ESP8266EX module (ESP-12F style, 4 MB flash, ~80 KB DRAM) with a CH340 USB-serial bridge. On I²C it drives a TV5725 at 0x17, an Si5351 at 0x60 and an SSD1306 128×64 OLED at 0x3C. Input is a rotary encoder with a push button.

## How to read this report

Each fact carries a marker:

| Marker | Meaning |
|---|---|
| **[V]** | Verified today by a command run on this machine: `gh api`, the npm registry, `pip index`, the PlatformIO registry API, a local build, `curl`, or a local compile. |
| **[D]** | Read today from upstream source code or docs (raw files fetched during the research session). |
| **[P]** | From the earlier gbs-control fork lessons digest ([gbs-control-lessons-platform.md](gbs-control-lessons-platform.md)). These are bench measurements made by that project, not re-measured here. |
| **[U]** | Unverified: my inference, background knowledge, or a claim I did not check today. Validate it before relying on it. |

Dates are GitHub release `published_at` values or registry publish dates, in UTC.

---

## 0. Summary of the recommended stack

| Layer | Choice | Why, in one line |
|---|---|---|
| Build system | PlatformIO Core **6.2.0**, `espressif8266@4.2.1` (Arduino core **3.1.2**, GCC **10.3.0**) | The only maintained, reproducible ESP8266 Arduino path. ESPHome and WLED pin the same versions [V]. |
| Language | C++20 (`-std=gnu++20`) for our code, with a C++17 fallback | Builds cleanly with the stock toolchain [V]. `<format>` is missing. |
| Memory options | lwIP2 low-memory (MSS 536); MMU "16 KB cache + 48 KB IRAM + shared 2nd heap" | Heap is the binding constraint [P]. The second heap adds IRAM-backed malloc space [D]. |
| HTTP server | **ESP8266WebServer (in core)**, synchronous, handlers run in `loop()` context | Removes the async-context crash classes the predecessor hit [P]. Tasmota uses the same server [V]. **Alternative:** ESPAsyncWebServer 3.12.1. |
| Live status | Polling a compact `/api/status` every 1–1.5 s with an 8 s client timeout; no WebSocket | Cheapest in heap and flash [P]. |
| JSON | ArduinoJson **7.4.3** (security fix), used only for small documents or streamed output | v7 documents are heap-elastic; keep them small. |
| Filesystem | LittleFS, layout **4m1m** (1019 KB sketch / ~2 MB OTA staging / 1000 KB FS), holding settings and profiles only | OTA always fits; FS OTA stays possible as an escape hatch. |
| Web UI | **Preact 11 + @preact/signals 2.11 + TypeScript**, **Vite 8**, vite-plugin-singlefile, gzip -9, **embedded in the firmware image** | 8.1 KB gzip framework baseline measured [V]. One atomic artifact, so the UI always matches the firmware API. |
| UI i18n | Typed dictionary (`en` is the source of truth, `pt-BR: Record<keyof typeof en, string>`) plus `Intl.*` | About 0 KB runtime, and a missing key is a compile error. Paraglide JS 2.27 is the heavier alternative. |
| Wi-Fi provisioning | Own module: STA, then AP fallback with captive DNS, the same SPA's Wi-Fi page, and **Improv Wi-Fi Serial** (sdk-cpp parser) | One UI and one i18n path. ESP Web Tools can provision right after flashing. |
| Browser flashing | **ESP Web Tools 10.4.0** installer on **GitHub Pages**, with binaries copied into the Pages artifact | Release assets on GitHub send no CORS headers [V]; Pages sends `ACAO: *` [V]. |
| OTA | HTTP upload to the `Updater` (gzip-compressed `.bin.gz`; eboot decompresses), browser-assisted "check for update" from `latest.json` on Pages | No TLS on the device. Signing is optional and opt-in. |
| OLED | **U8g2 2.36.18** (full buffer, `_tf` Latin-1 fonts for pt-BR, partial tile updates) | Static 1 KB buffer and Latin-1 glyphs. The alternative is ThingPulse 4.6.2. |
| Encoder | Own IRAM ISR quadrature state machine plus loop-side debounce (or `mathertel/RotaryEncoder` 1.6.0) | Small and host-testable. |
| Si5351 | Own minimal driver (fixed register plans) or `etherkit/Si5351Arduino` **2.3.0** | Etherkit just shipped its first release in two years [V]. Pin it and test it. |
| CLI | Own table-driven parser sharing one command table with HTTP and the OLED; optional `embedded-cli` for line editing | Static allocation and one source of truth. |
| Host tests | `pio test -e native` with **doctest** (or Unity), fakes for Wire, LittleFS and time | Proven pattern [P]. |
| Static checks | clang-format **22.1.8** (pinned; matches local v22), cppcheck **2.22** via `pio check`, clang-tidy 22 via pip on the native env | Current tools, pinned. |
| Size budget | Custom script over `xtensa-lx106-elf-size -A`, because PlatformIO's max-size keys do **not** fail the build [V] | Enforced in CI. |
| Web tests | Vitest **5.0.4** (happy-dom), MSW **3.0.3** device mock, Playwright **1.64.0** e2e, Biome **2.5.15** lint and format, TypeScript **7.0.2** typecheck | typescript-eslint does not support TS 7 yet [V]. |
| CI/CD | GitHub Actions (versions in §11), **release-please** v5, **Renovate** with a custom PlatformIO datasource, Conventional Commits | Neither Renovate nor Dependabot has a native PlatformIO manager [V]. |
| Future hardware | Ai-Thinker **ESP-C3-12F** (ESP32-C3) on the ESP-12F footprint [U], pioarduino `platform-espressif32` 55.03.312-1 (Arduino-ESP32 3.3.12 / IDF 5.5.5) | Keep a HAL boundary now. |

---

## 1. Version table

### 1.1 Platform and toolchain

| Component | Version | Date | Status | Source |
|---|---|---|---|---|
| Arduino core for ESP8266 (esp8266/Arduino) | **3.1.2** | 2023-03-20 | Maintenance only: no release for 3.5 years. Master's last commit was 2026-08-27 (an ENC28J60 fix); about 20 commits since 2025-05, mostly CI and tools. The `4.0.0` milestone is open (24 open / 3 closed, no due date). | [V] gh api releases, commits, milestones |
| ESP8266 NONOS SDK | v3.0.6 | 2024-06-27 | "No new features, only critical bug fixes" since Dec 2019. The core's default build is NONOS **2.2.x (190703)**; 3.0.5 can be selected by a define. | [V] repo README; [D] `tools/platformio-build.py` |
| ESP8266 RTOS SDK | v3.4 | 2021-04-08 | Master commits until 2026-02-10. No Arduino layer, so not relevant here. | [V] |
| PlatformIO Core | **6.2.0** | 2026-09-05 | Active. Python ≥ 3.9, SCons 4.11.1, PVS-Studio removed, new `pio run -p/--port`. | [V] gh + PyPI |
| platformio/espressif8266 (dev platform) | **4.2.1** | 2023-07-19 | Dormant: the `develop` branch's last commit was 2025-09-08. Still the pinned choice of ESPHome 2026.9 and WLED 16.0.1. | [V] registry API, gh; [D] their `platformio.ini` |
| framework-arduinoespressif8266 | 3.30102.0 (= core 3.1.2) | 2023-03-21 | Bundled with platform 4.2.1. | [V] `pio pkg show`, local build |
| toolchain-xtensa | 2.100300.220621 = **GCC 10.3.0** (esp-quick-toolchain 3.2.0-gcc10.3) | 2023-02-24 | Frozen. Core master also references 3.2.0-gcc10.3. | [V] `xtensa-lx106-elf-gcc --version`, core package template |
| tool-mklittlefs (PlatformIO) | 1.203.210628 (reports `mklittlefs 0.2.3`) | 2021-06-28 | Old but works. Upstream mklittlefs is 4.1.0 (2025-09-02). | [V] |
| tool-esptoolpy pinned by the platform | `~1.30000.0` (esptool 3.0) | n/a | Works for ESP8266 uploads. Latest registry package is 2.41100.260830. | [V] `platform.json` |
| Tasmota platform fork | 2026.09.00 | 2026-09-26 | Based on core **2.7.4** with backports, tuned for Tasmota. Not recommended. | [V] |
| esptool (Python) | **5.5.0** | 2026-10-08 | Active. ESP8266 is still a target (`esptool/targets/esp8266.py`). v4.12.0 (2026-07-14) is the maintenance line. | [V] gh, PyPI, repo tree |

### 1.2 Networking, web server and provisioning (firmware side)

| Component | Version | Date | Status | Source |
|---|---|---|---|---|
| ESP32Async/ESPAsyncWebServer | **3.12.1** | 2026-09-12 | Active (commit 2026-10-12). LGPL-3.0. Supports ESP8266. | [V] |
| ESP32Async/ESPAsyncTCP | 2.0.0 | 2025-01-20 | **Repository archived** (read-only since 2025-01). Still the ESP8266 dependency of ESPAsyncWebServer and of ESPHome. | [V] |
| me-no-dev/ESPAsyncWebServer, me-no-dev/ESPAsyncTCP | n/a | archived 2025-01-20 | Superseded by ESP32Async. | [V] |
| ESP8266WebServer, DNSServer, ESP8266mDNS (LEAmDNS), LittleFS, Updater, ESP8266HTTPUpdateServer | in core 3.1.2 | 2023-03 | Maintained with the core. | [D] |
| ArduinoJson | **7.4.3** | 2026-03-02 | Active. Fixes a buffer overrun in `as<T>()` (#2220), also back-ported as 7.3.2, 7.2.2 and **6.21.6**. Treat these as minimum versions. | [V] release notes |
| tzapu/WiFiManager | 2.0.17 | 2024-03-02 | Slow; master's last commit was 2026-02-25. | [V] |
| alanswx/ESPAsyncWiFiManager | n/a | last commit 2022-01-22 | Dormant. | [V] |
| khoih-prog/ESPAsync_WiFiManager | 1.15.1 | 2022-10-25 | Archived. | [V] |
| improv-wifi/sdk-cpp | v1.2.7 | 2026-08-27 | Active. Apache-2.0. Copy-in sources; used by ESPHome. | [V] |
| jnthas/Improv-WiFi-Library | 0.0.4 (PlatformIO) / v0.0.1 (GitHub release) | 2025-12-30 / 2023-03-30 | Small. Supports `CF_ESP8266`. | [V] |

### 1.3 Flashing, installer and reference projects

| Component | Version | Date | Status | Source |
|---|---|---|---|---|
| ESP Web Tools (`esp-web-tools`) | **10.4.0** | 2026-07-15 | Active (Open Home Foundation). Depends on lit, esptool-js and improv-wifi-serial-sdk. | [V] |
| esptool-js | **0.7.0** | 2026-09-21 | Active. Has an `esp8266` target. | [V] repo tree |
| improv-wifi-serial-sdk (npm) | 2.8.1 | 2026-07-16 | Active. | [V] |
| WLED | 16.0.1 | 2026-07-07 | Uses `espressif8266@4.2.1` and its own ESPAsyncWebServer fork. | [V] |
| WLED-WebInstaller | no releases; last commit 2026-08-27 | n/a | Static Pages site, `esp-web-tools@10.4.0` from unpkg. | [V] |
| Tasmota | 15.6.0 | 2026-08-25 | ESP8266 build uses ESP8266WebServer (the `TasmotaWebServer` subclass). | [V] |
| tasmota/install (web installer) | rolling, dated tags | 2026-10-13 | Binaries are committed and served from Pages. | [V] |
| ESPHome | 2026.9.1 (2026.10.0b3 beta) | 2026-09-29 | On ESP8266: ESPAsyncWebServer 3.9.6 with ESPAsyncTCP 2.0.0. | [V]/[D] |
| esphome/firmware | 26.9.2 | 2026-10-08 | Builds ready-made project firmwares and manifests. | [V] |

### 1.4 Web UI toolchain

| Component | Version | Date | Status | Source |
|---|---|---|---|---|
| preact | **11.0.1** (10.x line: 10.29.8, 2026-08-01) | 2026-10-08 | 11.0.0 shipped 2026-09-30, so it is two weeks old. 10.x is still maintained. | [V] |
| @preact/signals | 2.11.3 | 2026-09-30 | Peer `preact >=10.25 or >=11.0.0-0`. | [V] |
| @preact/preset-vite | 2.10.6 | 2026-07-18 | Peer `vite 2.x … 8.x`, `@babel/core 7.x` (Babel 8 is the npm default now, so pin `@babel/core@^7`). | [V] |
| svelte | 5.57.2 | 2026-10-06 | Active. | [V] |
| solid-js | 1.9.17 (2.0.0-rc.15 on the `next` tag) | 2026-10-07 | 2.0 is close, so expect churn. | [V] |
| lit | 3.3.3 | 2026-05-14 | Stable. | [V] |
| vanjs-core | 1.6.1 | 2026-07-16 | Stable. | [V] |
| vite | **8.3.4** | 2026-10-08 | Active (Rolldown-based). Node ^20.19 or ≥22.12. | [V] |
| vite-plugin-singlefile | 2.3.3 | 2026-04-17 | Peer `vite …^8`. **Works with Vite 8** (tested). | [V] |
| vite-plugin-compression2 | 2.5.3 | 2026-03-21 | Optional. | [V] |
| typescript | **7.0.2** (native compiler; 6.0.3 is the last JS-based) | 2026-07-08 | Active. | [V] |
| typescript-eslint | 8.71.2 | 2026-10-12 | Peer `typescript >=4.8.4 <6.1.0`, so **no TS 7 support**. | [V] |
| @biomejs/biome | 2.5.15 | 2026-09-30 | Active; independent of the TypeScript version. | [V] |
| oxlint | 1.88.0 | 2026-10-13 | Active. | [V] |
| eslint | 10.12.0 | 2026-10-02 | Active. | [V] |
| vitest | **5.0.4** | 2026-10-12 | Node ^22.12, ^24 or ≥26. | [V] |
| @playwright/test | **1.64.0** | 2026-10-07 | Active. | [V] |
| msw | **3.0.3** | 2026-10-10 | Node ≥22.12. | [V] |
| happy-dom / jsdom | 21.0.1 / 30.1.2 | 2026-10-12 / 2026-10-04 | Active. | [V] |
| @inlang/paraglide-js | 2.27.0 | 2026-10-09 | Active (compile-time i18n). | [V] |
| typesafe-i18n | 5.27.1 | 2026-02-11 | Revived after a long gap (repo moved to codingcommons). | [V] |
| @nanostores/i18n | 1.3.3 | 2026-05-10 | Active. | [V] |
| rosetta | 1.1.0 | 2020-06-29 | Stale. | [V] |
| i18next | 26.4.4 | 2026-10-12 | Active, but large for this use. | [V] |
| size-limit | 14.2.0 | 2026-10-07 | Optional bundle budget tool. | [V] |

### 1.5 Device libraries

| Component | Version | Date | Status | Source |
|---|---|---|---|---|
| U8g2 (PlatformIO `olikraus/U8g2`) | **2.36.18** (GitHub tag 2.37.1 from 2026-05-26 is not on the registry yet) | 2026-03-08 | Active. | [V] |
| ThingPulse esp8266-oled-ssd1306 | 4.6.2 | 2026-01-09 | Active. Used by the predecessor [P]. | [V] |
| Adafruit SSD1306 / Adafruit GFX | 2.5.17 / 1.12.6 | 2026-05-29 / 2026-04-09 | Active. | [V] |
| etherkit/Si5351Arduino | **2.3.0** | 2026-10-10 | Revived: first release since 2024-10. Removes heap allocation in register writes and fixes PLL re-planning. | [V] release notes |
| pavelmc/Si5351mcu | v0.6 | 2021-02-25 (last commit 2022-03-22) | Dormant. | [V] |
| mathertel/RotaryEncoder | 1.6.0 | 2026-02-21 (PlatformIO entry 2025-10-25) | Active. | [V] |
| igorantolic/ai-esp32-rotary-encoder | 1.7 | 2024-06-21 | Supports ESP8266 (README). | [V] |
| PaulStoffregen/Encoder | 1.4.4 | 2023-12-11 | Slow. | [V] |
| mathertel/OneButton | 2.6.2 | 2026-02-20 | Active. | [V] |

### 1.6 Serial CLI libraries

| Component | Version | Date | Status | Source |
|---|---|---|---|---|
| SpacehuhnTech/SimpleCLI | 1.1.4 | 2022-06-14 (last commit 2026-01-06) | Slow. Uses String and heap; its README recommends ≥ 32 KB RAM. | [V] |
| funbiscuit/embedded-cli | v0.1.4 | 2024-01-18 (last commit 2026-03-11) | Single-header C, static or dynamic allocation, history, autocompletion. | [V] |
| Helius/microrl | tag v1.5.1 | last commit 2017-09-03 | Dormant. | [V] |

### 1.7 Testing and static analysis

| Component | Version | Date | Status | Source |
|---|---|---|---|---|
| Unity | v2.7.0 on GitHub; **2.6.1** in the PlatformIO registry | 2026-07-16 / 2025-01-01 | Active. | [V] |
| doctest | v2.5.3 on GitHub; **2.4.12** in the PlatformIO registry | 2026-07-06 / 2025-04-28 | Active. | [V] |
| GoogleTest | v1.18.0 on GitHub; 1.17.0 in the PlatformIO registry | 2026-08-10 | Active. | [V] |
| Catch2 | v3.16.1 | 2026-10-08 | Active; no native PlatformIO runner. | [V] |
| cppcheck (now `cppcheck-opensource/cppcheck`) | **2.22.0**; PlatformIO `tool-cppcheck` 1.22200.0 (2026-09-23) | 2026-09-19 | Active. | [V] |
| clang-tidy (PyPI wheel) | 22.1.8; PlatformIO `tool-clangtidy` is **19.1** (2024-09-19) | n/a | Use the pip wheel. | [V] |
| clang-format (PyPI) | 23.1.3 latest; **22.1.8** recommended to match local clang-format 22 | n/a | Pin one version. | [V] |
| LLVM | 23.1.3 | 2026-10-06 | n/a | [V] |

### 1.8 Tooling, CI and dependency updates

| Component | Version | Date | Status | Source |
|---|---|---|---|---|
| pre-commit / commitizen | 4.7.0 / 4.19.2 | n/a | Active. | [V] PyPI |
| @commitlint/cli + config-conventional | 21.2.3 | 2026-09-19 | Active. | [V] |
| lefthook / husky | 2.2.2 (2026-10-13) / 9.1.7 (2024-11-18) | n/a | Active / slow. | [V] |
| release-please / release-please-action | 17.11.2 / **v5.0.0** | 2026-08-24 / 2026-04-22 | Active. | [V] |
| semantic-release | 25.0.9 (v26 in beta) | 2026-08-05 | Active. | [V] |
| Renovate | 44.155.9 | 2026-10-13 | Active; **no PlatformIO manager** (custom regex needed). | [V] manager list |
| Dependabot | core v0.400.0 | 2026-10-12 | **No PlatformIO ecosystem**. | [V] ecosystem dirs |

The GitHub Actions versions are in §11.

### 1.9 ESP32 path (future)

| Component | Version | Date | Status | Source |
|---|---|---|---|---|
| arduino-esp32 | 3.3.12 (IDF **5.5.5**); 4.0.0-RC1 (IDF 6.1) | 2026-09-18 / 2026-09-23 | Active. | [V] |
| ESP-IDF | v6.1 (v5.5.x and v6.0.x maintained) | 2026-08-27 | Active. | [V] |
| pioarduino/platform-espressif32 | **55.03.312-1** (Arduino 3.3.12) | 2026-09-22 | Community fork; the only PlatformIO route to Arduino-ESP32 3.x. | [V] |
| platformio/platform-espressif32 | 7.1.3 | 2026-09-11 | Official, but **Arduino 2.0.17 (IDF 4.4.7) only**; ESP-IDF 6.1 natively. | [V] release notes |

---

## 2. Findings and recommendations by topic

### 2.1 Arduino core, SDK, PlatformIO and compiler (topic 1)

**Facts**

- Core 3.1.2 is the newest release, from 2023 [V]. Master is alive but nearly idle. The 4.0.0 milestone has no date, so do not plan around it.
- The core's PlatformIO builder sets `-std=gnu17` and `-std=gnu++17` [D]. It defaults to NONOS SDK 2.2.x (190703) and offers several lwIP2 variants [D]:
  - `LWIP2_LOW_MEMORY` (MSS 536, the default);
  - `HIGHER_BANDWIDTH` (MSS 1460);
  - IPv6 variants.
- It also offers MMU variants [D]:
  - "16 KB cache + 48 KB IRAM";
  - the **shared 2nd heap** variant, which gives IRAM-backed `malloc` with a non-32-bit access handler, selected per scope with `HeapSelectIram`;
  - an unmanaged 16 KB block.
- GCC 10.3.0 [V], checked against a probe file on this machine:

  | Flag | Result |
  |---|---|
  | `-std=gnu++20` | Compiles concepts, `std::span`, designated initialisers, `consteval`, `<bit>` and `<ranges>`. |
  | `<format>` | Absent. |
  | `-std=gnu++2b` | Rejected. |

- A full sample project (§4) builds with `-std=gnu++20`, the MMU shared heap, U8g2, ArduinoJson 7.4.3, ESP8266WebServer, DNSServer, mDNS and LittleFS [V]. The only noise is `-Wvolatile` warnings from the core's `SPI.cpp`.

**Alternatives considered**

- Core master via `platform_packages`: unreleased, with few gains.
- The Tasmota platform fork: core 2.7.4 base, tuned to Tasmota's needs.
- The ESP8266 RTOS SDK (IDF style): no Arduino ecosystem, so OLED, Si5351 and JSON would all need ports.
- ESPHome as a framework: wrong abstraction for a register-level video controller.

None beats the pinned 4.2.1 / 3.1.2 combination.

**Recommendation**

- Pin `platform = espressif8266@4.2.1` (core 3.1.2, GCC 10.3). This is what ESPHome and WLED ship today [V].
- Use `-std=gnu++20` for designated-initialiser register tables, `std::span` and concepts. Revert to gnu++17 if any library misbehaves (none did in the test build).
- Run at 160 MHz [P].
- Keep `LWIP2_LOW_MEMORY`.
- Evaluate the MMU shared second heap on the bench. Trade-off: the flash instruction cache halves from 32 KB to 16 KB, so flash-resident code may run slower. Measure the loop time before and after [U].

### 2.2 HTTP server, push vs. poll, JSON (topic 2)

**ESPAsyncWebServer 3.12.1 [V/D]**

- Active and feature-rich: middleware, SSE, WebSocket, URI matchers.
- On ESP8266 it depends on **ESPAsyncTCP 2.0.0, which is archived**. No upstream fixes will come for the ESP8266 TCP layer.
- ESP8266 defaults [D]: `WS_MAX_QUEUED_MESSAGES 8`, `DEFAULT_MAX_WS_CLIENTS 4`, `SSE_MAX_QUEUED_MESSAGES 8`, `SSE_MAX_INFLIGH 8 KB`.
- The docs say: "You can not use yield or delay or any function that uses them inside the callbacks" [D].
- Handler-context pitfalls the predecessor paid for [P]:
  - delay or yield in a handler drops the board;
  - LittleFS listing or totals inside a handler gave `Fatal exception 9`;
  - `WiFi.begin()` from a callback tore down the AP;
  - the I²C layer yielded mid-transaction, so a handler switched TV5725 segments;
  - `beginResponse(code, type, ptr, len)` kept a pointer into freed heap;
  - request bodies arrive sliced;
  - `server.on("/x")` also matches `/x/…` (BackwardCompatible matcher).

  Their rule was: handlers validate, queue and answer 202; `loop()` does the work.
- Async HTTP OTA (`Update.runAsync`) **stopped working at about 15 KB idle heap** [P].

**ESP8266WebServer (core) [D]**

- Synchronous. `handleClient()` runs in `loop()` context, so handlers may touch I²C, LittleFS and `delay()` safely. That removes most of the pitfall list above by construction.
- Serves one client at a time. Keep-alive is dropped automatically when another client is waiting.
- `serveStatic` auto-selects a `.gz` sibling and sends `Content-Encoding`. It also offers `enableETag()`, chunked responses, and `ESP8266HTTPUpdateServer`.
- Costs:
  - a request in progress blocks the loop. `HTTP_MAX_DATA_WAIT`, `HTTP_MAX_POST_WAIT` and `HTTP_MAX_SEND_WAIT` are hard-coded at **5000 ms** (only `HTTP_UPLOAD_BUFLEN`, 2048 B, is overridable), so a misbehaving client can stall the OLED and encoder for up to about 5 s;
  - no SSE or WebSocket.
- Tasmota, the most widely deployed ESP8266 firmware, uses it [V].

**Measured static cost**

Both skeletons are the same app, with U8g2, ArduinoJson, DNS and mDNS [V]:

| Build | Flash | Static RAM (.data + .rodata + .bss) |
|---|---|---|
| ESP8266WebServer | 368,631 B | 31,136 B |
| ESPAsyncWebServer + ESPAsyncTCP + one AsyncEventSource | 379,169 B (+10.5 KB) | 32,788 B (+1.65 KB) |

Runtime heap per request or connection is not measured here. Async allocates per-request objects and `std::function` handlers [P]: ten alias routes cost 1.8 KB of heap.

**WebSocket vs. SSE vs. polling on ESP8266**

- Removing the WebSocket in the predecessor saved 13,314 B flash and 1,400 B static RAM, and raised free heap by 1.5 KB [P].
- Polling at 1.5 s with an 8 s deadline held up with 1–3 clients. Four clients reached about 10.7 KB minimum heap and one request failed [P].
- Each SSE or WebSocket client pins a TCP pcb plus a message queue for its whole lifetime.
- Choice: **poll** a compact status endpoint (≤ 512 B; binary or tight JSON; include a sequence number so the UI can skip unchanged renders).
- If push becomes essential, switch to the async server and use SSE capped at 1–2 clients, never WebSocket. SSE is one-way text with no masking or framing on the device; the browser reconnects automatically.

**ArduinoJson 7.4.3**

- Required for the #2220 fix [V]. v7's `JsonDocument` is heap-elastic: there is no `StaticJsonDocument` any more, but a custom allocator can be plugged in [U: API detail from background knowledge].
- On this heap, use small documents.
- Serialise straight to the client (chunked) instead of building a `String`.
- For the hot status endpoint, consider `snprintf_P` into a stack buffer and skip ArduinoJson entirely.
- The predecessor's lesson: a 958-field JSON was "the shortest path to a lockup". The schema stays in the UI and the device sends values [P].

**Recommendation**

- Use **ESP8266WebServer** for the ESP8266 product.
- Keep a thin transport-neutral API layer: a command table plus route registration in one module. Swapping to ESPAsyncWebServer (portable to ESP32-C3, where AsyncTCP is maintained) is then a contained change.
- If you choose async anyway, adopt the predecessor's queue-and-202 rule and route-ordering CI check from day one.

### 2.3 LittleFS, flash layouts, UI delivery (topic 3)

**Flash layouts for 4 MB parts** [D] (from `eagle.flash.4mXm.ld`):

| Layout | Sketch max | Free (OTA staging) | LittleFS | Notes |
|---|---|---|---|---|
| 4m1m | 1,044,464 B (~1019 KB) | ~2052 KB | 1,024,000 B @ 0x300000 | Any OTA fits, and an atomic FS OTA of a 1 MB image fits. **Recommended.** |
| 4m2m | ~1019 KB | ~1028 KB | 2,072,576 B @ 0x200000 | OTA fits (sketch max ≤ staging). Atomic FS OTA of a 2 MB image does not. |
| 4m3m | ~1019 KB | **4 KB** | ~3048 KB | **No OTA.** Avoid. |

All three end with EEPROM 4 KB, RF cal 4 KB and Wi-Fi 12 KB. The LittleFS block is 8 KB and the page 256 B.

**Building and uploading the FS image** [V]

- `pio run -t buildfs` produces a **full-partition image**: 1,024,000 B for 4m1m. It gzips to 11 KB when nearly empty. `uploadfs` and `uploadfsota` also exist [D].
- Flashing a 1–2 MB FS part through the web installer adds tens of seconds at 460800 baud and wipes user settings [U: timing estimate].

**Gzipped UI options**

| Option | How | Pros | Cons |
|---|---|---|---|
| A. **Embedded in firmware (recommended)** | Vite single-file HTML, gzip -9, a generated `ui_bundle.h` (`PROGMEM` array plus ETag = content hash), served with `Content-Encoding: gzip` | One atomic, versioned artifact: UI and API can never drift. OTA and web installer flash one file. Settings in LittleFS are never touched by UI updates. | UI bytes count against the 1019 KB sketch and OTA size. A UI-only change needs a firmware OTA. |
| B. LittleFS `/www/index.html.gz` | Predecessor's approach | UI can be updated by uploading one file over HTTP. Saved them 171 KB of sketch [P]. | Version skew between UI and API; needs an emergency PROGMEM page when the file is missing [P]. FS image flashing overwrites settings. |

Hybrid: ship A, and if `/www/index.html.gz` exists, serve it instead. That gives UI developers a fast loop without a reflash.

A Preact app of this scope should land around 30–60 KB gzip [U: estimate; the predecessor's vanilla TS bundle was 62–80 KB **unminified** [P]].

**LittleFS rules carried over** [P]:

- Call `begin()` once, before the server starts.
- Never write preferences at boot when they are missing; that caused a bootloop.
- Defer saves (3 s after the last change).
- Write from one stack buffer.
- Check existence before `send`; a missing file produced 501s.
- Report `fsUsed` and `fsTotal`.
- Remove a file before rewriting it (observed silent failure).

Add a magic, version and CRC32 header to each settings or profile blob, with migration functions, and write to a temporary file then `rename()` [U: LittleFS rename atomicity is a documented littlefs property; not tested on the core].

### 2.4 Wi-Fi provisioning, captive portal, mDNS (topic 4)

**Library options**

| Library | Version and state [V] | Assessment |
|---|---|---|
| tzapu/WiFiManager | 2.0.17 (2024), slow | Mature and captive. Uses its own sync ESP8266WebServer and its own HTML (not our UI or i18n). It must own port 80 while the portal runs [U]. |
| alanswx/ESPAsyncWiFiManager | dormant 2022 | Avoid. |
| khoih-prog/ESPAsync_WiFiManager | archived | Avoid. |

**Improv Wi-Fi Serial** (ESP Web Tools asks for Wi-Fi credentials right after flashing):

- `improv-wifi/sdk-cpp` v1.2.7 (Apache-2.0). It exposes a byte-wise `parse_improv_serial_byte(position, byte, buffer, …)` and `build_rpc_response(…)` [V]. That is ideal for **sharing UART0 with the CLI**: feed bytes to the Improv parser while the line buffer starts with `IMPROV`, otherwise to the CLI.
- `jnthas/Improv-WiFi-Library` 0.0.4 supports `CF_ESP8266` [V]. However, `handleSerial()` reads the `Stream` directly and calls `delay()` [V], so it would steal CLI bytes unless wrapped in a demultiplexing `Stream`.

**mDNS and captive portal**

- `ESP8266mDNS` is in the core; call `MDNS.update()` in the loop. `.local` fails on some phones [P], so always show the IP on the OLED and serial console.
- `DNSServer` is in the core for the wildcard captive DNS. Answer OS probe URLs: `/generate_204`, `/hotspot-detect.html`, `/connecttest.txt`, `/ncsi.txt` [U: list from background knowledge].
- ESP8266 has one radio. In AP+STA mode the AP follows the STA's channel, so phones on the portal drop when the STA associates. Scans also disturb the AP [U].
- Never call `WiFi.begin()` from a request callback; apply it from `loop()` [P].

**Recommendation**

- Build a small provisioning module on the main server:
  1. boot as STA with stored credentials;
  2. after N seconds or with no credentials, start the AP `GBS-XXXX` with a printed WPA2 password and captive DNS;
  3. the SPA's Wi-Fi page lists scan results and posts credentials;
  4. `loop()` applies them.
- Keep Improv Serial always on.
- The OLED shows the AP name, password and IP. A QR code is optional [U].

### 2.5 Browser flashing and installer publishing (topic 5)

**ESP Web Tools 10.4.0 [V/D]**

- Manifest schema, from `src/const.ts`:
  - `name`, `version`;
  - optional `home_assistant_domain`, `funding_url`, `new_install_prompt_erase`;
  - optional `new_install_improv_wait_time` (seconds; `0` disables Improv detection);
  - `builds[]` of `{ chipFamily, parts: [{ path, offset }], serialType? }`.
- `chipFamily: "ESP8266"` takes a single part (the core's `firmware.bin` includes eboot) at offset 0.
- The per-build `"improv": true` still found in Tasmota manifests is **no longer in the schema**.
- 10.2.0 added "manifest via URL". 10.3.0 and 10.4.0 improved the Wi-Fi picker, removed pako, and moved to TypeScript 7.
- Requires Web Serial: Chrome or Edge 89+ on desktop. Android works via the web-serial-polyfill over WebUSB, per the esptool-js README [D]. **Not Firefox or Safari** [U]. Needs HTTPS, which Pages provides.
- Boards without DTR/RTS auto-reset need manual boot mode. On this hardware the encoder button is on **GPIO0** [P]: "hold the knob pressed while plugging in USB".

**Flashers**

- esptool-js 0.7.0 has an ESP8266 target [V].
- esptool **5.5.0** still flashes ESP8266 [V]. v5 breaking changes [V]:
  - dashed commands (`write-flash`);
  - no `.py` suffix (deprecation warnings);
  - the ESP8266 `make-image` command removed;
  - Python 3.7–3.9 deprecated.
- The PlatformIO platform keeps using its bundled esptool 3.0 for uploads [V], which is fine.

**How others publish**

| Project | How it publishes |
|---|---|
| Tasmota (`tasmota/install`) [V] | Firmware binaries are **committed** and served from Pages (`tasmota.github.io/install/firmware/release/tasmota.bin`, 666,720 B, `Access-Control-Allow-Origin: *`). Per-language manifests are generated by Python scripts. Uses `new_install_prompt_erase: true` and `new_install_improv_wait_time: 10`. |
| WLED (`wled/WLED-WebInstaller`) [V] | Static page, `esp-web-tools@10.4.0` from unpkg. Reads the GitHub Releases API at page load and builds manifests as blob URLs. Fetches binaries from a **CORS-enabled mirror (`download.wled.me`)**. Deploys with the Pages actions. |
| ESPHome [V] | `esphome/firmware` builds ready-made project images and manifests. |

**CORS facts, checked today with curl [V]**

- `github.com/<o>/<r>/releases/download/...` returns 302 to `release-assets.githubusercontent.com`, and **neither response carries `Access-Control-Allow-Origin`**. Browsers cannot fetch release assets directly.
- GitHub Pages responds with `Access-Control-Allow-Origin: *`.
- `api.github.com` sends ACAO `*` but rate-limits unauthenticated calls; we got **403 "rate limit exceeded"** during this research. Do not make installers or devices depend on the API.

**Recommendation**

- On release, CI copies `firmware.bin` into the Pages artifact under `firmware/<version>/`.
- It writes `manifest.json` (stable), `manifest-beta.json` and `latest.json`, plus an `index.html` installer with a pinned `esp-web-tools@10.4.0` (vendored or jsDelivr).
- Pages-from-Actions **replaces the whole site on each deploy**. The site job must therefore re-download the last N releases' assets (`gh release download`) and rebuild every version folder.

### 2.6 OTA (topic 6)

**Updater [D]**

- `Update.begin(size, U_FLASH | U_FS)`, then `write()` and `end()`.
- MD5 is always computed; `setMD5()` can check it.
- `installSignature(hash, verify)` with `BearSSL::HashSHA256` and `BearSSL::SigningVerifier` validates **RSA-2048, SHA-256, PKCS#1 v1.5** signatures appended to the image.

**Gzip-compressed OTA [D/V]**

- eboot contains a gzip decompressor, and `Updater` auto-detects the gzip header. No application change is needed.
- Sign **after** compressing.
- Compressed images do not get their flash-mode bits patched.
- The test firmware went from 372,784 B to **269,985 B** with gzip -9 (−28%) [V].

**Signing [D]**

- `tools/signing.py --mode header` generates the public-key header; `--mode sign` signs. It uses the `openssl` CLI through subprocess.
- Automatic signing exists **only in the Arduino IDE on Linux and macOS**. PlatformIO needs a `pre:` and `post:` `extra_script` [U: script not written yet].
- Once a signed image is installed, **every later OTA must be signed with the same key**. For an open-source project that blocks community builds from web OTA; they would have to reflash over USB.
- Recommendation: do not enforce signatures in v1. Instead:
  - validate the image header (0xE9 magic or gzip);
  - embed and check a board ID string;
  - check the size;
  - publish SHA-256 sums.
  - Offer signing as a build flag for later.

**Filesystem OTA [D]**

- `U_FS` writes **directly over the live FS** unless the firmware is built with `-DATOMIC_FS_UPDATE`. With that flag the image is staged in free space and eboot copies it. This needs free space ≥ image size: works on 4m1m (1 MB), not on 4m2m.
- An FS image overwrites settings and profiles. That is another reason to embed the UI.

**Transport options**

| Option | Assessment |
|---|---|
| **HTTP upload** via ESP8266HTTPUpdateServer or own `/api/ota` on the sync server | Runs in `loop()` context with 2 KB chunks. Expected to need much less heap than the async path that failed for the predecessor [U: verify on the bench]. |
| **ESP8266httpUpdate (device pulls)** | Fetching from GitHub needs BearSSL TLS (tens of KB of heap) and redirects to release-assets. Not robust at ~15–20 KB free heap [U]. Avoid. |
| **Browser-assisted update (recommended)** | The device UI (plain HTTP) fetches `latest.json` and `firmware.bin.gz` from Pages (CORS `*` [V]), shows the changelog, verifies SHA-256 in the browser (`crypto.subtle`), then POSTs the bytes to the device. The reverse direction (a Pages page calling `http://device`) is blocked by mixed-content rules and Chrome's local-network restrictions [U for local-network details]. |
| **ArduinoOTA / espota** | Needs the PC to accept an inbound TCP connection (firewall) and uses MD5 digest auth. Predecessor removed it after video problems coincided with enabling it [P: cause unverified]. Make it a dev-only build flag at most. |

**No rollback on ESP8266** [D/U]

- eboot copies the staged image over the running one at boot, so there is no A/B slot.
- A power loss during the copy may need USB recovery [U].
- Keep the web installer path well documented, and make the bootloader entry trivial (the GPIO0 button).
- RTC memory is wiped by OTA [P].
- Verify after flashing by reading something only the new firmware reports [P].

### 2.7 Web UI for a tiny bundle (topic 7)

**Measured today [V]**: the same micro-app (counter, text binding, keyed list, 1.5 s `fetch` poll) in each framework, Vite 8.3.4 production build, ES2022:

| Framework | Min JS | gzip -9 | brotli 11 |
|---|---|---|---|
| Vanilla TS | 0.6 KB | **0.35 KB** | 0.29 KB |
| VanJS 1.6.1 | 2.7 KB | **1.34 KB** | 1.22 KB |
| Solid 1.9.17 | 11.9 KB | **4.78 KB** | 4.34 KB |
| Preact 11.0.1 (hooks) | 13.3 KB | **5.64 KB** | 5.13 KB |
| Lit 3.3.3 | 17.0 KB | **6.57 KB** | 5.93 KB |
| Preact 11.0.1 + @preact/signals 2.11.3 | 20.4 KB | **8.06 KB** | 7.32 KB |
| Svelte 5.57.2 (runes) | 33.6 KB | **13.09 KB** | 11.94 KB |

`vite-plugin-singlefile` 2.3.3 produced a single 21.7 KB HTML (8.7 KB gzip) for the Preact+signals app under Vite 8 [V]. Brotli does not apply here, because browsers only advertise `br` over HTTPS and the device serves HTTP [U]. Ship gzip -9.

**Choice: Preact 11 + signals + TypeScript**

- About 8 KB of baseline buys JSX, components, and fine-grained signal updates. Polled values update in place without re-rendering forms, which avoids the predecessor's "rebuilding the DOM loses focus and closes selects" problem [P].
- Preact 11 is two weeks old. Preact 10.29.8 (2026-08-01) is the safe fallback, with the same API for this use.
- Solid is smaller but 2.0 is at RC.15, so a migration is coming.
- VanJS or vanilla win on bytes but lose structure and typing at the size this UI will reach. They are acceptable if the team prefers them.
- Svelte 5's runtime baseline is the largest.

**Tooling**

- **TypeScript 7.0.2** for `tsc --noEmit`: native, fast. Vite does not type-check.
- typescript-eslint 8.71.2 requires `typescript <6.1.0` [V]. So use **Biome 2.5.15** (lint and format) or oxlint 1.88 with TS 7, or stay on TS 6.0.3 if you want typescript-eslint.
- Precompression: a 10-line Node `zlib.gzipSync(level 9)` step plus a header generator. No plugin is needed; vite-plugin-compression2 2.5.3 is optional.
- Tests:
  - **Vitest 5.0.4** with happy-dom for components and logic;
  - **MSW 3.0.3** handlers as the device mock, reused in dev and tests;
  - **Playwright 1.64.0** e2e against `vite preview` plus the mock.
- Also record fixtures from a real board, and set `server.proxy` to a live device for hands-on dev [P: the predecessor's mock board and smoke approach].
- Every `fetch` needs a deadline (`AbortSignal.timeout(8000)`) [P].

**i18n (English default + pt-BR)**

- `src/i18n/en.ts` is the source of truth (`as const`). `pt-BR.ts` is typed `Record<keyof typeof en, string>`, so a missing key fails the typecheck.
- A ~20-line `t(key, params)` helper does the lookup. Use `Intl.NumberFormat`, `Intl.PluralRules` and `Intl.DateTimeFormat` from the browser, at zero bytes.
- Pick the language from `navigator.language`, overridable and stored on the device.
- Bundle both locales (a few KB gzip) [U: size estimate] or lazy-load pt-BR.
- Alternatives:
  - Paraglide JS 2.27.0: compile-time, tree-shaken message functions; more tooling, nicer at scale;
  - @nanostores/i18n 1.3.3;
  - typesafe-i18n 5.27.1;
  - rosetta (stale) and i18next (too heavy) are not recommended.
- Device-side strings (OLED, CLI help) live in two `PROGMEM` tables keyed by the same IDs. A shared YAML or JSON source can generate both the TS dictionaries and the C++ tables [U: suggested design].

### 2.8 Device libraries (topic 8)

**OLED**

| Library | RAM | Latin-1 / pt-BR | Notes |
|---|---|---|---|
| **U8g2 2.36.18** | Full buffer `_F_` = 1 KB static (.bss); page buffer `_1_` = 128 B | `_tf` fonts cover 32–255; `enableUTF8Print()` + `drawUTF8()`. "Configuração" compiled [V]; on-screen rendering not verified [U]. | Huge font choice, partial updates (`updateDisplayArea`). Flash per font about 1–3 KB [U]. |
| ThingPulse 4.6.2 | **Double buffer by default** (`OLEDDISPLAY_DOUBLE_BUFFER`) → about 2 KB heap for 128×64 [D] | Bundled fonts have 224 glyphs from 0x20 (Latin-1) [D]; UTF-8 to Latin-1 lookup | Simple API. Used by the predecessor [P]. |
| Adafruit SSD1306 + GFX | 1 KB `malloc` | Built-in font is CP437-ish 7-bit; Latin-1 needs custom GFX fonts [U] | Heaviest for this use. |

- Choice: U8g2, full buffer, dirty-tile updates.
- A full 1 KB frame at 400 kHz I²C takes roughly 23 ms of bus time [U: arithmetic]. It shares the bus with the TV5725 and Si5351, so only push changed tiles. The predecessor's OLED tick was 43 ms awake and 18 ms with the screensaver on [P].

**Rotary encoder**

- ESP8266 supports `attachInterrupt` on GPIO 0–15 with `IRAM_ATTR` ISRs.
- Use a table-driven quadrature (Gray-code) state machine on both edges of CLK and DT that accumulates into a `volatile int8_t`. That is about 40 lines and is host-testable.
- Use loop-side debounce or OneButton 2.6.2 for click, long-press and double-click.
- Carry over the predecessor's lessons [P]:
  - ISR on CLK GPIO14 and switch GPIO0;
  - the "ignore an immediate direction flip but remember the last good direction" debounce;
  - an ISR event ID so events arriving mid-tick are not lost.
- GPIO0 is a boot strap pin; it is fine for a button that is not held at reset.
- Library alternatives: mathertel/RotaryEncoder 1.6.0 (call `tick()` from an ISR), ai-esp32-rotary-encoder 1.7 (supports ESP8266).

**Si5351**

- etherkit/Si5351Arduino 2.3.0 was released 2026-10-10. It removes heap use in register writes and fixes PLL planning [V]. It is new, so pin it and bench-test it.
- pavelmc/Si5351mcu is dormant (2021).
- The predecessor kept its own driver in `src/` [P].
- Choice: a small own driver, since the board needs a handful of fixed or derived frequencies. Validate register maths in host tests against etherkit's algorithm or datasheet examples. Use etherkit 2.3.0 if general synthesis is needed.

### 2.9 Serial CLI (topic 9)

| Option | Assessment |
|---|---|
| SimpleCLI 1.1.4 | String- and heap-based, "≥ 32 KB RAM recommended". Not a fit. |
| embedded-cli 0.1.4 | Single-header C, static allocation, history and autocomplete. A good line editor. |
| microrl | Dormant since 2017. |
| **Custom (recommended)** | One `constexpr` command descriptor table (name, args, risk class, handler, help string ID) feeds the CLI, the HTTP API (`/api/cmd`) and the OLED actions. The predecessor's "one command table for every panel" lesson [P]. |

Tokenise in place from a fixed 128-byte line buffer. Demultiplex Improv frames before the CLI (§2.4). Optionally embed embedded-cli for history and tab completion.

### 2.10 Host tests and static analysis (topic 10)

**Host tests**

- Add an `[env:native]` with `test_framework = doctest`. PlatformIO's registry serves doctest 2.4.12 [V], which is good C++ ergonomics. Unity 2.6.1 is the alternative (the predecessor ran 25 Unity cases in seconds [P]).
- Use GoogleTest 1.17 (registry) if mocks are wanted. Catch2 3.16.1 has no PlatformIO runner.
- Use fakes, not mocks [P]:
  - `Wire.h` backed by a TV5725 register file (six 256-byte segments, segment select at 0xF0);
  - `LittleFS.h` as a map;
  - `millis()` as a variable that `delay()` advances;
  - seed the chip from real captures.
- On Windows, the native env needs a host compiler. A clang/gcc exists on this machine under `~/.local/share/retcomm/toolchains/...` [V], but it was not exercised with `pio test`. CI on `ubuntu-24.04` has gcc.

**Static analysis**

- `pio check --tool cppcheck` uses tool-cppcheck 2.22 [V].
- For clang-tidy, prefer the **pip wheel 22.1.8** over PlatformIO's bundled 19.1 [V]. Run it on the native env's `compile_commands.json` (`pio run -e native -t compiledb`), because xtensa flags confuse clang [U].
- For clang-format, pin **22.1.8** in CI (`uvx clang-format@22.1.8`) to match the local clang-format 22, or upgrade both to 23.1.3.

**Size budgets: verified gotcha**

- Overriding `board_upload.maximum_ram_size` below the actual usage printed "RAM 103.8%" and the build still reported **SUCCESS** [V].
- Overriding `board_upload.maximum_size` had **no effect at all** on ESP8266, because the limit comes from the ldscript [V].
- Budgets therefore need a script (`post:` extra_script or a CI step) that parses `xtensa-lx106-elf-size -A firmware.elf`:

  | Region | Sections | Test build value [V] |
  |---|---|---|
  | DRAM | `.data` + `.rodata` + `.bss` | 31,136 B |
  | IRAM | `.text` + `.text1` | 29,431 B |
  | Flash | `.irom0.text` | 336,128 B |

- Fail when a budget is exceeded or a PR grows DRAM by more than 512 B [P rule].
- Also gate the web bundle with a gzip-size check (a script, or size-limit 14.2.0).

### 2.11 CI/CD, releases and dependency updates (topic 11)

**GitHub Actions (latest, all [V])**

| Action | Latest | Date | Use as |
|---|---|---|---|
| actions/checkout | v7.0.1 | 2026-07-20 | `@v7` |
| actions/setup-python | v7.0.0 | 2026-07-20 | `@v7` |
| actions/setup-node | v7.1.0 | 2026-10-08 | `@v7` |
| actions/cache | v6.1.0 | 2026-06-26 | `@v6` |
| actions/upload-artifact | v7.0.2 | 2026-10-07 | `@v7` |
| actions/download-artifact | v8.0.2 | 2026-10-07 | `@v8` (cross-major pairing with upload v7 assumed compatible [U]) |
| actions/configure-pages | v6.0.0 | 2026-03-25 | `@v6` |
| actions/upload-pages-artifact | v5.0.0 | 2026-04-10 | `@v5` |
| actions/deploy-pages | v5.0.1 | 2026-09-01 | `@v5` |
| astral-sh/setup-uv | v10.3.0 | 2026-10-09 | `@v10` |
| pnpm/action-setup | v6.1.0 | 2026-09-05 | `@v6` |
| googleapis/release-please-action | v5.0.0 | 2026-04-22 | `@v5` |
| softprops/action-gh-release | v3.0.3 | 2026-08-30 | `@v3` (or plain `gh release upload`) |

Pin actions by commit SHA in practice; Renovate keeps the SHAs current.

**Release automation**

- **release-please** (recommended): release PRs with a generated CHANGELOG from Conventional Commits. `release-type: simple` plus `extra-files` bumps `firmware/include/version.h` and `web/package.json`. Humans control when to cut a release, which suits firmware.
- semantic-release 25.0.9 releases on every push and is npm-centric (plugins needed). Not recommended here.
- A release created with `GITHUB_TOKEN` **does not trigger other workflows** (`on: release`) [U: long-standing GitHub behaviour, not re-checked today]. Chain the build and Pages jobs in the same workflow (see §5).

**Dependency updates**

- Neither Renovate nor Dependabot has a PlatformIO manager [V].
- **Renovate (recommended)** covers github-actions, npm/pnpm, pip/uv, pre-commit natively, plus a **custom regex manager** for `platformio.ini`. Pair it with a custom datasource over the PlatformIO registry JSON API, which works: `https://api.registry.platformio.org/v3/packages/<owner>/library/<name>` returns `version.name` and `released_at` [V].
- Dependabot covers actions and npm only. You would also need a scheduled `pio pkg outdated` job.

**Conventional Commits**

- Use commitlint 21.2.3 with config-conventional, run by **lefthook 2.2.2** (single binary via npm). The `pre-commit` 4.7.0 framework is equally fine; it can also run clang-format.
- Enforce PR titles too (squash merges).

**Publishing**

- See §2.5 and the workflow in §5.
- Keep the last N (for example 5) stable versions plus beta on Pages.
- GitHub Pages has published-site size and bandwidth limits [U: about 1 GB site, a 100 GB/month soft limit per GitHub docs, not re-checked]. Firmware at ~370 KB per version is far below that.

### 2.12 Future-proofing: an ESP32 drop-in (topic 12)

- **Ai-Thinker ESP-C3-12F** (ESP32-C3, RISC-V 160 MHz, ~400 KB SRAM, 4 MB flash, Wi-Fi 4 + BLE 5) is marketed as pin-compatible with the ESP-12F footprint [U: not verified today].
  - Before committing, check the exact pad-to-GPIO map against the GBS board: I²C pads, encoder pads, and the CH340 TX/RX on U0TXD/U0RXD (GPIO21/20).
  - Check the strapping pins (GPIO2, 8, 9 on C3 versus GPIO0, 2, 15 on ESP8266) against the board's pull-ups. Check the auto-reset wiring: on C3, BOOT is GPIO9.
- Ai-Thinker's ESP-12K (ESP32-S2) is another footprint candidate [U].
- Toolchain:
  - **pioarduino `platform-espressif32` 55.03.312-1**, i.e. Arduino-ESP32 3.3.12 on IDF 5.5.5 [V]. Arduino-ESP32 4.0 (IDF 6.1) is at RC1 [V].
  - Official `platformio/platform-espressif32` 7.1.3 offers only Arduino 2.0.17, or native ESP-IDF 6.1 [V].
  - The GCC used by IDF 6.x is 15.2.0 (per platform 7.0.0 notes) [V]. IDF 5.5's GCC is 14.2 [U].
- What the C3 buys:
  - real A/B OTA with rollback;
  - about 5× the RAM, which ends the heap fights;
  - ESPAsyncWebServer with the maintained AsyncTCP, or `esp_http_server`;
  - Improv over BLE;
  - ESP Web Tools `ESP32-C3` builds (merged factory image at 0x0).
- Design now for that path:
  - keep `hal/` interfaces for the net server, FS, OTA, GPIO/ISR and I²C;
  - keep the API command table transport-neutral;
  - keep the web UI chip-agnostic;
  - keep the manifest generator multi-chip.

---

## 3. Risks and mitigations

| # | Risk | Impact | Mitigation |
|---|---|---|---|
| 1 | ESP8266 core and platform stagnation: no release since 2023, GCC 10.3 | No upstream fixes; C++23 unavailable | Pin exact versions; keep a HAL; plan the ESP32-C3 hardware path (§2.12). |
| 2 | ESPAsyncTCP archived (if async is chosen) | TCP-layer bugs never fixed upstream | Prefer the sync ESP8266WebServer; vendor-patch if needed. |
| 3 | Heap exhaustion (~15–22 KB idle [P]) | Resets, failed OTA, dropped pages | Polling, ≤ 2 heavy clients, no TLS, static rings, stack buffers, second IRAM heap, heap telemetry, a "busy" 503 page [P]. |
| 4 | Loop stalls with the sync server (up to 5 s from a bad client) | OLED or encoder lag; timing machines delayed | Small responses, `Connection: close` for big ones, gzip UI, and long sequences as state machines tolerant of jitter. Measure on the bench. |
| 5 | OTA brick: no A/B or rollback; power loss during the eboot copy | USB recovery needed | Validate before `end()` (board ID, size, magic); gzip to shorten transfers; documented web-installer recovery; GPIO0 button for bootloader entry. |
| 6 | FS image flashes erase user profiles | Data loss | Embed the UI in firmware; never ship an FS part in normal updates; JSON export and import of profiles. |
| 7 | Web Serial limits (Chromium only), CH340 drivers, boards without auto-reset | Users cannot flash | README matrix, a manual boot-mode tip, and the esptool CLI as a fallback (v5 command names). |
| 8 | GitHub release assets lack CORS; API rate limits (seen today) | Installer or update check fails | Serve binaries and `latest.json` from Pages (CORS `*` verified). |
| 9 | Mixed content and local-network restrictions | Pages page cannot call the device | Updates are initiated from the device UI (http) pulling from Pages (https), not the reverse. |
| 10 | Tooling churn: TS 7 vs typescript-eslint, Preact 11 brand new, Solid 2 RC, Vitest 5 and MSW 3 just released | Broken builds after updates | Biome instead of typescript-eslint; fallback Preact 10.29.x; Renovate grouping plus CI. |
| 11 | Signing key loss or misuse, if signing is ever enforced | Fleet cannot OTA | Keep signing opt-in; if adopted, store the key offline with a backup and sign only in a protected CI environment. |
| 12 | Licence mix: ESPAsyncWebServer is LGPL-3.0; Improv sdk-cpp is Apache-2.0; U8g2 is BSD-2 [U]; etherkit's licence not checked [U] | Distribution obligations | Choose the project licence early (GPL-3.0 is compatible with all of these [U]) and keep a `THIRD_PARTY.md`. |
| 13 | Improv and CLI sharing UART0, plus boot ROM output at 74880 baud | Garbled provisioning | A byte-level demultiplexer and Improv framing with checksum; test with ESP Web Tools on real hardware. |
| 14 | Settings schema evolution | Old profiles unreadable | Version plus CRC header, migration functions, and host tests over stored fixtures. |

---

## 4. Sample `platformio.ini`

The `[esp8266_base]` and `[env:gbs]` part was built successfully on 2026-10-13 with PlatformIO 6.2.0 in an equivalent `[env]`-based layout, with a test sketch using every listed library plus ESP8266WebServer, DNSServer, mDNS, LittleFS and `HeapSelectIram`. Results: **RAM 31,136 B (38.0%)**, **flash 368,631 B (35.3%)** [V]. Lines marked `[U]` were not executed.

```ini
; GBS Reimagined, PlatformIO project configuration (sample)
[platformio]
default_envs = gbs
src_dir      = firmware/src
include_dir  = firmware/include
lib_dir      = firmware/lib
test_dir     = firmware/test
data_dir     = firmware/data          ; only for dev FS images; releases do not ship an FS part

[esp8266_base]
platform  = espressif8266@4.2.1       ; Arduino core 3.1.2, toolchain-xtensa GCC 10.3.0
framework = arduino
board     = esp12e                    ; ESP-12E/F, 4 MB flash
board_build.f_cpu       = 160000000L
board_build.flash_mode  = dio         ; safe default for ESP-12F (QIO likely works [U])
board_build.ldscript    = eagle.flash.4m1m.ld   ; 1019 KB sketch | ~2 MB OTA staging | 1000 KB LittleFS
board_build.filesystem  = littlefs
upload_speed  = 460800
monitor_speed = 115200
monitor_filters = esp8266_exception_decoder, time
lib_compat_mode = strict
lib_ldf_mode    = chain
build_unflags = -std=gnu++17
build_flags =
  -std=gnu++20
  -Wall -Wextra
  -DPIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY                ; MSS 536 (core default), saves heap
  -DPIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48_SECHEAP_SHARED ; 2nd heap in IRAM (HeapSelectIram); measure ICACHE impact
  -DATOMIC_FS_UPDATE                                      ; only matters if FS OTA is ever used
  -DGBS_BOARD_ID=\"gbs-esp8266-4m\"                       ; [U] checked by the OTA image validator
lib_deps =
  olikraus/U8g2@2.36.18
  bblanchon/ArduinoJson@7.4.3
  ; etherkit/Etherkit Si5351@2.3.0          ; only if not using the in-tree driver
  ; esp32async/ESPAsyncWebServer@3.12.1     ; only for the async alternative
  ; esp32async/ESPAsyncTCP@2.0.0            ; (archived upstream)
extra_scripts =
  pre:tools/pio/embed_ui.py          ; [U] web/dist/index.html.gz -> firmware/include/generated/ui_bundle.h (+ETag)
  post:tools/pio/size_budget.py      ; [U] parse `xtensa-lx106-elf-size -A`, fail on budgets (PIO keys do not fail [V])
  post:tools/pio/gzip_firmware.py    ; [U] firmware.bin -> firmware.bin.gz (OTA payload)

[env:gbs]
extends = esp8266_base

[env:gbs_debug]
extends = esp8266_base
build_type = debug
build_flags =
  ${esp8266_base.build_flags}
  -DDEBUG_ESP_PORT=Serial
  -DGBS_LOG_LEVEL=4

[env:native]                          ; host unit tests: pio test -e native   [U: not executed here]
platform = native
test_framework = doctest
build_flags =
  -std=gnu++20
  -DHOST_BUILD
  -Ifirmware/test/host/include        ; fakes: Arduino.h, Wire.h (TV5725 register file), LittleFS.h, time
build_src_filter = +<core/> +<model/> +<proto/>   ; hardware-free modules only
```

---

## 5. GitHub Actions outline

Two workflows plus one reusable build. The YAML is an outline: structure and versions are checked, but it has **not been executed** [U].

### 5.1 `.github/workflows/ci.yml` (PRs and main)

```yaml
name: ci
on:
  pull_request:
  push: { branches: [main] }
permissions: { contents: read }
concurrency: { group: ci-${{ github.ref }}, cancel-in-progress: true }

jobs:
  build:
    uses: ./.github/workflows/build.yml          # reusable: web + firmware + checks

  commitlint:
    if: github.event_name == 'pull_request'
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v7
        with: { fetch-depth: 0 }
      - uses: pnpm/action-setup@v6
      - uses: actions/setup-node@v7
        with: { node-version: 24, cache: pnpm }
      - run: pnpm install --frozen-lockfile
      - run: pnpm commitlint --from ${{ github.event.pull_request.base.sha }} --to HEAD
```

### 5.2 `.github/workflows/build.yml` (reusable, `on: workflow_call`)

```yaml
on:
  workflow_call:
    inputs: { release_tag: { type: string, default: "" } }
jobs:
  web:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v7
      - uses: pnpm/action-setup@v6
      - uses: actions/setup-node@v7
        with: { node-version: 24, cache: pnpm, cache-dependency-path: web/pnpm-lock.yaml }
      - run: pnpm -C web install --frozen-lockfile
      - run: pnpm -C web typecheck                 # tsc --noEmit (TypeScript 7)
      - run: pnpm -C web lint                      # biome ci .
      - run: pnpm -C web test                      # vitest run (happy-dom + MSW)
      - run: pnpm -C web exec playwright install --with-deps chromium
      - run: pnpm -C web e2e                       # playwright vs vite preview + mock device
      - run: pnpm -C web build                     # vite + singlefile -> dist/index.html(.gz)
      - run: node tools/size/web_budget.mjs web/dist/index.html.gz   # e.g. <= 64 KiB gzip
      - uses: actions/upload-artifact@v7
        with: { name: web-ui, path: web/dist/index.html.gz }

  firmware-checks:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v7
      - uses: astral-sh/setup-uv@v10
      - uses: actions/cache@v6
        with:
          path: ~/.platformio
          key: pio-${{ runner.os }}-${{ hashFiles('platformio.ini') }}
      - run: uv tool install platformio==6.2.0
      - run: git ls-files 'firmware/**/*.[ch]pp' 'firmware/**/*.h' | xargs uvx clang-format@22.1.8 --dry-run -Werror
      - run: pio test -e native                    # doctest host suite
      - run: pio run -e native -t compiledb && uvx clang-tidy@22.1.8 -p . $(git ls-files 'firmware/src/core/*.cpp')
      - run: pio check -e gbs --tool cppcheck --fail-on-defect=medium --skip-packages

  firmware:
    needs: [web]
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v7
      - uses: actions/download-artifact@v8
        with: { name: web-ui, path: web/dist }
      - uses: astral-sh/setup-uv@v10
      - uses: actions/cache@v6
        with: { path: ~/.platformio, key: pio-${{ runner.os }}-${{ hashFiles('platformio.ini') }} }
      - run: uv tool install platformio==6.2.0
      - run: pio run -e gbs                        # pre-script embeds UI; post-scripts: budget + gzip
      - run: python tools/size/fw_budget.py .pio/build/gbs/firmware.elf tools/size/budget.json --report "$GITHUB_STEP_SUMMARY"
      # optional (opt-in signing): write ${{ secrets.OTA_PRIVATE_KEY }} to a temp file, run core tools/signing.py on firmware.bin.gz
      - run: |
          cd .pio/build/gbs
          ver="${{ inputs.release_tag }}"; ver="${ver:-dev-${GITHUB_SHA::7}}"
          cp firmware.bin    "gbs-reimagined-${ver}-esp8266-4m.bin"
          cp firmware.bin.gz "gbs-reimagined-${ver}-esp8266-4m.ota.bin.gz"
          sha256sum gbs-reimagined-* > SHA256SUMS
      - uses: actions/upload-artifact@v7
        with:
          name: firmware
          path: |
            .pio/build/gbs/gbs-reimagined-*
            .pio/build/gbs/SHA256SUMS
            .pio/build/gbs/firmware.elf
            .pio/build/gbs/firmware.map
```

### 5.3 `.github/workflows/release.yml` (release-please, then build, release assets, Pages)

```yaml
name: release
on:
  push: { branches: [main] }
permissions: { contents: write, pull-requests: write }

jobs:
  release-please:
    runs-on: ubuntu-24.04
    outputs:
      created: ${{ steps.rp.outputs.release_created }}
      tag:     ${{ steps.rp.outputs.tag_name }}
    steps:
      - uses: googleapis/release-please-action@v5
        id: rp
        with:
          config-file: release-please-config.json        # release-type: simple, extra-files: version.h, web/package.json
          manifest-file: .release-please-manifest.json

  build:
    needs: release-please
    if: needs.release-please.outputs.created == 'true'
    uses: ./.github/workflows/build.yml
    with: { release_tag: "${{ needs.release-please.outputs.tag }}" }

  publish-assets:
    needs: [release-please, build]
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/download-artifact@v8
        with: { name: firmware, path: out }
      - run: gh release upload "${{ needs.release-please.outputs.tag }}" out/gbs-reimagined-* out/SHA256SUMS --repo "$GITHUB_REPOSITORY"
        env: { GH_TOKEN: "${{ github.token }}" }

  pages:                                   # chained here: GITHUB_TOKEN-created releases do not trigger `on: release`
    needs: [release-please, publish-assets]
    runs-on: ubuntu-24.04
    permissions: { contents: read, pages: write, id-token: write }
    environment: { name: github-pages, url: "${{ steps.deploy.outputs.page_url }}" }
    concurrency: { group: pages, cancel-in-progress: false }
    steps:
      - uses: actions/checkout@v7
      - uses: astral-sh/setup-uv@v10
      # Rebuild the WHOLE site each time (Pages artifact replaces the site):
      #  - gh release download for the last N stable + latest pre-release tags
      #  - site/firmware/<ver>/*.bin, site/manifest.json, site/manifest-beta.json,
      #    site/latest.json {version, url(.ota.bin.gz), sha256, size, board, notes_url},
      #    site/index.html (installer; esp-web-tools@10.4.0 pinned, EN/pt-BR strings)
      - run: uv run tools/installer/build_site.py --repo "$GITHUB_REPOSITORY" --keep 5 --out site
        env: { GH_TOKEN: "${{ github.token }}" }
      - uses: actions/configure-pages@v6
      - uses: actions/upload-pages-artifact@v5
        with: { path: site }
      - uses: actions/deploy-pages@v5
        id: deploy
```

### 5.4 Generated `manifest.json` (ESP Web Tools 10.x schema [D])

```json
{
  "name": "GBS Reimagined",
  "version": "1.2.0",
  "funding_url": "https://github.com/sponsors/<owner>",
  "new_install_prompt_erase": true,
  "new_install_improv_wait_time": 15,
  "builds": [
    {
      "chipFamily": "ESP8266",
      "parts": [{ "path": "firmware/1.2.0/gbs-reimagined-1.2.0-esp8266-4m.bin", "offset": 0 }]
    }
  ]
}
```

Paths resolve relative to the manifest URL. Add an `ESP32-C3` build entry when that hardware exists.

### 5.5 Renovate sketch for `platformio.ini` [U: untested]

This uses Renovate's custom regex manager and custom datasource. Check the field names (`managerFilePatterns` vs. `fileMatch`) against the Renovate 44 docs.

```json5
{
  "extends": ["config:recommended", ":semanticCommits", "helpers:pinGitHubActionDigests"],
  "customManagers": [{
    "customType": "regex",
    "managerFilePatterns": ["/(^|/)platformio\\.ini$/"],
    "matchStrings": ["(?<owner>[A-Za-z0-9_-]+)/(?<name>[A-Za-z0-9 ._-]+?)@(?<currentValue>\\d+\\.\\d+\\.\\d+)"],
    "depNameTemplate": "{{owner}}/{{name}}",
    "packageNameTemplate": "{{owner}}/library/{{name}}",
    "datasourceTemplate": "custom.platformio"
  }],
  "customDatasources": {
    "platformio": {
      "defaultRegistryUrlTemplate": "https://api.registry.platformio.org/v3/packages/{{packageName}}",
      "transformTemplates": ["{ \"releases\": versions.{ \"version\": name, \"releaseTimestamp\": released_at } }"]
    }
  },
  "packageRules": [
    { "matchDatasources": ["custom.platformio"], "groupName": "platformio libraries" },
    { "matchManagers": ["npm"], "groupName": "web toolchain", "schedule": ["before 6am on monday"] }
  ]
}
```

The platform line (`espressif8266@4.2.1`, endpoint `platformio/platform/espressif8266`) needs a second matcher.

---

## 6. Measurement appendix

The measurements in this report (framework bundle sizes, the PlatformIO sample builds, the C++
standard probe and the raw version dumps) were made in a scratch workspace during the research
session on 2026-10-13 and are not part of the repository. Results quoted above:

- `pio run` of the sample app with ESP8266WebServer: RAM 31,136 B, flash 368,631 B; with
  ESPAsyncWebServer + one event source: RAM 32,788 B, flash 379,169 B.
- `firmware.bin` 372,784 B, gzip -9 269,985 B; a nearly empty 4m1m `littlefs.bin` 1,024,000 B
  (11,053 B gzipped).
- Framework micro-app sizes: the table in section 2.7.

Re-run them against the real firmware rather than trusting these numbers: the size budget in CI
(`tools/size/`) reports the current values on every build.
