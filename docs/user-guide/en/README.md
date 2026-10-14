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

## Joining your Wi-Fi

The first time, the board does not know your network, so it opens one of its own.

1. Power the board. It opens a Wi-Fi network called **GBS-** followed by four letters and digits
   that are unique to your board (for example `GBS-EAC4`). The password is **`qqqqqqqq`** (eight
   letters q) — the same one the original GBS-Control project uses.
2. Connect your phone or computer to that network. The GBS Reimagined page opens by itself; if it
   does not, open **http://192.168.4.1** in the browser.
3. Go to the **Network** tab, press **Find networks**, pick your home network, type its password
   and press **Join**. Your password goes straight to the board.
4. The board joins your network and closes its own network about a minute later (sooner if your
   phone leaves it). Put your phone back on your usual network and open **http://gbs.local**.
   Some Android phones do not understand `.local` addresses: use the address shown on the
   **Network** page, or the one your router lists for the board.

If the board cannot reach your network for 30 seconds (the router is off, or you changed its
password), it opens its own network again so you can fix it, and keeps trying your network every
two minutes while nobody is connected to it.

## Changes that need your confirmation

Some changes can leave the TV without a picture — the output resolution and frame lock, for
example. When you make one from the web page or the serial console, a banner asks you to
**Keep** it. If you do not answer within 20 seconds, the board goes back to how it was: a blank
TV fixes itself. The waiting time can be changed in **System → Confirm within**.

## Input levels

Some consoles send a brighter signal than the board expects, which makes whites look flat and
"burnt" and colours washed out. Two tools help (turn on **Show advanced settings**, page
**Input**):

- **Calibrate levels** — show a completely white screen on the console (the 240p Test Suite has
  one; any full-white screen works), then press the button. It takes a few seconds and the colours
  may flicker meanwhile. The board remembers the result for that input (RGB or component). If the
  screen is not white enough, it tells you and changes nothing.
- **Auto gain** — while you play, lowers the input level whenever the brightest parts of the
  picture are clipped. It only ever darkens, and starts again for each new console you connect.

## Serial console (advanced)

Connect the board's USB port to a computer and open a serial terminal at **115200 baud**. Type
`help` for the list of commands. The most useful:

| Command | What it does |
|---|---|
| `status` | what the board is doing, with the figures |
| `list` | every setting with its current value |
| `set picture.sharpness medium` | change a setting (`get` reads one) |
| `output 1080p` / `input rgb` | shortcuts for the output resolution and the input |
| `keep` / `revert` | answer a change that waits for confirmation |
| `wifi` | the network state; `wifi <name> <password>` joins a network |
| `calibrate` | calibrate levels against a white screen |
| `factory-reset` | everything back to defaults (type it twice) |

## Coming in this guide

1. Installing GBS Reimagined from your browser
2. The knob and the small screen
3. Choosing the output resolution for your display
4. Picture settings
5. Troubleshooting, by the message the board shows
6. Updating and restoring
