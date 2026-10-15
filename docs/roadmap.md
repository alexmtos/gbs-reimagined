# Roadmap

Milestones are delivered in order; each ends with a **bench gate** on the RetroScaler GBS-C (capture
card verification, see the `bench-verify` skill) before the next one starts.

## M0 — Foundations

Research, documentation, repository structure, Claude Code configuration, CI.

- [x] Market, technology and reference-project research (`docs/research/`)
- [x] Register database and baseline programs as data, with provenance (`schema/`)
- [x] Code generator with `--check` mode
- [x] Chip access layer and Si5351 driver with host tests
- [x] Architecture, video engine and UX design documents; ADRs 0001–0012
- [x] CLAUDE.md files, rules, skills, agents, hooks
- [x] CI: generator check, host tests, firmware build, web build, size report
- [x] Bench: full flash backup of the board as it ships today, and a reference capture
      ([bench log](bench/M0.md))

## M1 — First picture, monitored everywhere

- Boot sequence, Detector, Loader, Settler, Watcher, FrameLock (ratio) — `docs/design/video-engine.md`
- Output modes from the baseline programs + HD/RGBHV bypass (bypass experimental: no HD or PC source on the bench to verify it)
- Settings M1 subset, persisted; revert on risky changes; safe mode
- Status model; web UI (Status, Picture, Output, Input, System), OLED home + menu, serial console
- Wi-Fi: station + setup access point with captive portal, mDNS
- **Gate:** 240p, 480i, 480p and PAL 576i sources each reach a verified picture at 960p/1024p, 1080p,
  720p and 480p; 240p↔480i switch without a blank frame; settings survive a power cycle.
  **Passed on the bench 2026-10-13 ([bench M1](bench/M1.md)): 20 of 20 source/output pairs
  frame-locked with a picture; the 240p↔480i switch keeps sync and program (a short flicker on
  the TV).**

## M2 — Profiles and processing

- Profiles with auto-load by input signature, SVS/remote triggers, import/export
- Scanlines, deinterlacer choice, sharpness levels, geometry (aspect, position, size)
- Advanced sampling controls (phase, SOG threshold, gain, LPF) with live sync feedback
- Force 60 Hz
- Experimental: custom output timings (ADR 0004, ADR 0015)
- **Gate:** each setting verified on the capture card to do what its help text says.
- **Progress:** geometry (aspect, position, size) verified with NTSC and PAL sources on every
  output, 2026-10-14; scanlines, the deinterlacer choice and the sampling controls (SOG threshold,
  ADC phase, input filter, live sync quality) and Force 60 Hz verified the same day
  ([bench M2](bench/M2.md)). Gain is M1's calibrate levels and auto gain; "blank on mode change"
  was dropped (user decision). Profiles (ADR 0014) verified the same day: save/load, auto-load
  across two consoles, switcher lines on serial and HTTP, revert, startup modes, export/import and
  backup. Integer scale and the computed modes 1024x768, 800x600, 1600x1200 and 1920x1200
  (ADR 0015) verified the same day, with PAL at 1080p now an exact 4:3.

## M3 — Distribution

- Browser installer (ESP Web Tools) and OTA update from the web UI preserving settings and profiles
- Signed release images, release automation, changelog
- User guide in English and Brazilian Portuguese; troubleshooting by status headline
- **Gate:** a blank board is installed from the browser, joins Wi-Fi and shows a picture following
  only the user guide.
- **Progress:** firmware update over the network (`/api/update`, the web page's *Firmware update*,
  the release step's update file) verified 2026-10-15 ([bench M3](bench/M3.md)); "Check for
  updates" works once the installer site is public. Browser installer (`installer/`, ESP Web
  Tools 10.4.0) with Improv Wi-Fi Serial in the firmware: served locally, the board updated over
  USB and given its Wi-Fi from Chrome, 2026-10-15 ([bench M3](bench/M3.md)). The *pages*
  workflow publishes the site with the released firmware and `latest.json` after each release,
  once the repository is public. First public release: 1.0.0 once the gate passes (user
  decision, 2026-10-15).

## Open items that need the bench

| Id | Question | Why it matters |
|---|---|---|
| B-01 | Which physical jacks land on ADC input sets 0, 1 and 2 | Input names in every interface. **Front RGBS inputs = set 1, component = set 0 (2026-10-13); VGA open.** |
| B-02 | What the "Compatibility mode" switch, the RGBHV/RGBS switch, the sync buttons and the R/G/B knobs actually do | User guide troubleshooting. **Traced on the front RGBS input 2026-10-13 ([board-gbsc.md](hardware/board-gbsc.md)); VGA-jack and output-side effects open.** |
| B-03 | Free heap at idle with the web UI open, and during an OTA upload | OTA feasibility (the reference fork lost HTTP OTA below ~15 KB). **Idle measured 2026-10-13: ≥ 38 KB free, 35.5 KB low-water ([bench M1](bench/M1.md)); during an upload 24.9–26.9 KB at the lowest, 2026-10-15 ([bench M3](bench/M3.md)).** |
| B-04 | Whether the MS9288A accepts every output mode at the frame-locked rate | Which modes are offered on HDMI. **Yes for all five on this bench, 2026-10-13 ([bench M1](bench/M1.md)).** |
| B-05 | Which of the 12 programs give a clean picture on this board for each source | Default output per standard |
| B-06 | Whether GPIO15/GPIO16 are broken out on the GBS-C (IR receiver option) | Future remote control |
