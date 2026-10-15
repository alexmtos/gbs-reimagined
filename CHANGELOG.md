# Changelog

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
