# Changelog

## [1.1.0](https://github.com/alexmtos/gbs-reimagined/compare/v1.0.3...v1.1.0) (2026-10-15)

### New

* **Settings that have no effect right now are dimmed and say why.** Examples: the picture settings in Passthrough, the size at 15 kHz, scanlines on an interlaced source ([#19](https://github.com/alexmtos/gbs-reimagined/pull/19)).
* **Tidier pages:**
  * Output, Picture and System are split into sections;
  * the backup moved to System, next to the firmware update;
  * the startup profile is on Profiles only, and importing a profile has its own form.
* **"Default"** next to a changed setting puts its default value back.
* **A fuller Status page:**
  * input, sync, format, resolution, address, Wi-Fi signal, clock generator, and how long the board has been on;
  * a dash instead of 0.00 when there is no signal.
* **Better on a phone:**
  * the tabs wrap;
  * the "Keep these settings?" banner stays at the top;
  * controls are larger to touch and on/off settings look like switches;
  * on the board's setup network, every page points to the Network page.
* **Easier to read:** the colours of the light theme have more contrast, and screen readers hear the help and the messages.

### Changed

* **Force 60 Hz** now shows without the advanced settings.
* **Passthrough** (not yet verified on a board) is offered only with the advanced settings, on the web page and on the board's menu.

## [1.0.3](https://github.com/alexmtos/gbs-reimagined/compare/v1.0.2...v1.0.3) (2026-10-15)

### Fixes

* **One language for the board:** the language list at the top of the web page now sets the language of the page and of the board's screen together ([#17](https://github.com/alexmtos/gbs-reimagined/pull/17)).
* **The web page shows what the board is doing:**
  * it reloads itself after a firmware update;
  * it follows changes made with the knob, the console, a switcher or a profile that loads by itself;
  * it says "No signal" plainly and why frame lock is inactive;
  * it shows the result of "Calibrate levels" and shows "Auto" on sliders.
* **Nothing is lost by accident:** replacing a profile, restoring a backup and restarting the board ask for a second press. A restore also says what it could not put back.
* **Clearer texts:**
  * help that matches what each setting does;
  * the same names on the web page, the board's screen and the console;
  * unnamed profiles named in the board's language;
  * quoted network names and passwords in the serial console.

## [1.0.2](https://github.com/alexmtos/gbs-reimagined/compare/v1.0.1...v1.0.2) (2026-10-15)


### Bug Fixes

* **web:** one advanced switch for the page, the System setting and the board ([#15](https://github.com/alexmtos/gbs-reimagined/issues/15)) ([f58ee3e](https://github.com/alexmtos/gbs-reimagined/commit/f58ee3e345e2731d4e50162fa9fbddae6fd84f27))

## [1.0.1](https://github.com/alexmtos/gbs-reimagined/compare/v1.0.0...v1.0.1) (2026-10-15)


### Bug Fixes

* **web:** colour only questions and failures in setting notes ([#13](https://github.com/alexmtos/gbs-reimagined/issues/13)) ([ba6cfde](https://github.com/alexmtos/gbs-reimagined/commit/ba6cfde1d5d17735424f3f4d196df893a7e376d5))

## 1.0.0 (2026-10-15)

The first public release of GBS Reimagined, new firmware for the RetroScaler GBS-C. Install it from
your browser at https://alexmtos.github.io/gbs-reimagined/ (Chrome or Edge on a computer); the
[user guide](https://github.com/alexmtos/gbs-reimagined/blob/main/docs/user-guide/en/README.md)
([em português](https://github.com/alexmtos/gbs-reimagined/blob/main/docs/user-guide/pt-BR/README.md))
explains every step.

### Highlights

* **Install from the browser:** flash the board over USB and give it your Wi-Fi in the same window ([#11](https://github.com/alexmtos/gbs-reimagined/pull/11)).
* **Update over the network** from the board's own page, keeping settings, profiles and Wi-Fi; or over USB from the installer page ([#10](https://github.com/alexmtos/gbs-reimagined/pull/10)).
* **Three ways to control it:** a web page on your phone or computer, the board's own screen and knob (which work even when the TV shows nothing), and a USB serial console. Web page and board screen in English and Brazilian Portuguese ([#2](https://github.com/alexmtos/gbs-reimagined/pull/2)).
* **Picture:** 240p, 480i, 288p and 576i consoles scaled to 1080p, 1280x1024, 1280x960, 720p, 480p/576p or 15 kHz, with frame lock; PAL consoles show their whole 576-line picture. HD and computer sources pass through (experimental) ([#2](https://github.com/alexmtos/gbs-reimagined/pull/2)).
* **Shape and position:** 4:3, 16:9 or full screen, position and size, integer scale, and four computed resolutions for monitors: 1024x768, 800x600, 1600x1200 and 1920x1200 (experimental) ([#4](https://github.com/alexmtos/gbs-reimagined/pull/4), [#9](https://github.com/alexmtos/gbs-reimagined/pull/9)).
* **Scanlines** and a choice of deinterlacer ([#5](https://github.com/alexmtos/gbs-reimagined/pull/5)); **Force 60 Hz** for displays that do not take 50 Hz ([#7](https://github.com/alexmtos/gbs-reimagined/pull/7)).
* **Input:** level calibration from a white screen and optional auto gain; sampling phase, sync threshold and filter controls with a live sync-quality meter ([#2](https://github.com/alexmtos/gbs-reimagined/pull/2), [#6](https://github.com/alexmtos/gbs-reimagined/pull/6)).
* **Profiles:** 16 saved pictures, loaded by hand, by the console that appears, or by a console switcher (the commands RetroTINK scalers accept); export, import and a one-file backup ([#8](https://github.com/alexmtos/gbs-reimagined/pull/8)).
* **Safe changes:** a change that can leave the display blank goes back by itself unless you keep it, and a safe mode at start-up brings the picture back whatever the settings ([#2](https://github.com/alexmtos/gbs-reimagined/pull/2)).
* **Wi-Fi:** the board joins your network, or opens its own (GBS-XXXX) for setup, and says why a join failed ([#2](https://github.com/alexmtos/gbs-reimagined/pull/2)).

### Known limitations

* The installer's own window is in English; the page and the guide translate its labels.
* The experimental modes and passthrough have been tried on one board and one TV; passthrough of HD and computer sources has not been verified on a board.
