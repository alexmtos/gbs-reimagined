# Third-party components

Components shipped in the firmware image, the web UI or the installer page, with their licences.
Build-only tools (PlatformIO, Vite, TypeScript, Biome, Vitest, MSW, happy-dom, uv, ruff) are not
distributed and not listed.

| Component | Version | Licence | Where |
|---|---|---|---|
| ESP8266 Arduino core (incl. ESP8266WebServer, LittleFS, Updater, mDNS, lwIP) | 3.1.2 | LGPL-2.1 (core), various per library | firmware |
| ESP8266 NONOS SDK | bundled with the core | Espressif MIT-style licence | firmware |
| Preact | 11.0.1 | MIT | web UI |
| @preact/signals | 2.11.3 | MIT | web UI |
| U8g2 | 2.36.18 | BSD-2-Clause | firmware |
| Improv (improv-wifi/sdk-cpp) | 1.2.7 | Apache-2.0 | firmware |
| ESP Web Tools (its web bundle, which carries the rows below) | 10.4.0 | Apache-2.0 | installer page |
| improv-wifi-serial-sdk | 2.8.0 (pinned by ESP Web Tools) | Apache-2.0 | installer page |
| esptool-js | 0.6.x, as bundled by ESP Web Tools | Apache-2.0 | installer page |
| Material Web (@material/web) | as bundled by ESP Web Tools | Apache-2.0 | installer page |
| Lit (lit, lit-html, lit-element, @lit/reactive-element, @lit/context) | as bundled by ESP Web Tools | BSD-3-Clause | installer page |
| pako | as bundled by ESP Web Tools | MIT AND Zlib | installer page |
| tslib | as bundled by ESP Web Tools | 0BSD | installer page |
| atob-lite | as bundled by ESP Web Tools | MIT | installer page |

Data, not code: the TV5725 register loci and baseline register programs from gbs-control (GPL-3.0),
see [NOTICE.md](NOTICE.md).

Update this table whenever a dependency is added to `firmware/platformio.ini` or to the
`dependencies` of `web/package.json` or `installer/package.json`.
