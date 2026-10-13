# Third-party components

Components shipped in the firmware image or the web UI, with their licences. Build-only tools
(PlatformIO, Vite, TypeScript, Biome, Vitest, MSW, uv, ruff) are not distributed and not listed.

| Component | Version | Licence | Where |
|---|---|---|---|
| ESP8266 Arduino core (incl. ESP8266WebServer, LittleFS, Updater, mDNS, lwIP) | 3.1.2 | LGPL-2.1 (core), various per library | firmware |
| ESP8266 NONOS SDK | bundled with the core | Espressif MIT-style licence | firmware |
| Preact | 11.0.1 | MIT | web UI |
| @preact/signals | 2.11.3 | MIT | web UI |
| U8g2 (planned, M1) | 2.36.18 | BSD-2-Clause | firmware |
| improv-wifi/sdk-cpp (planned, M1) | 1.2.7 | Apache-2.0 | firmware |

Data, not code: the TV5725 register loci and baseline register programs from gbs-control (GPL-3.0),
see [NOTICE.md](NOTICE.md).

Update this table whenever a dependency is added to `firmware/platformio.ini` or to the
`dependencies` of `web/package.json`.
