# GBS Reimagined — user guide

For people who own a RetroScaler GBS-C and want to play their consoles on a modern TV or a
monitor. No programming knowledge needed.

> **This firmware is not ready to install yet.** This guide grows with each milestone; the first
> installable version arrives with milestone M1 ([roadmap](../../roadmap.md)).

## What the board does

The GBS-C takes the analogue video of a console or computer and turns it into a picture your
display accepts: HDMI for modern TVs, or VGA / RGBS for monitors and CRTs. GBS Reimagined is the
software that runs on the board's small computer and decides how that conversion is done.

You will be able to control it three ways:

- **From a phone or computer** on the same Wi-Fi, in a web page with every option explained.
- **With the knob and the small screen** on the board itself — this always works, even when the TV
  shows nothing.
- **From a USB serial terminal**, for advanced users and console switchers.

## The board's own switches and buttons

Some controls are wired directly into the video path; the firmware cannot see their position, so
check them when something looks wrong. Their exact function is still being confirmed on the bench.

| Label on the board | Kind |
|---|---|
| HDMI ON / OFF | slide switch |
| RGBHV / RGBS | slide switch |
| Compatibility mode | slide switch |
| RGBS Sync | latching button |
| Sync on Green | latching button |
| R / G / B | knobs |
| Power | latching button |

## Coming in this guide

1. Installing GBS Reimagined from your browser
2. First power-up and joining your Wi-Fi
3. Choosing the output resolution for your display
4. Picture settings
5. Troubleshooting, by the message the board shows
6. Updating and restoring
