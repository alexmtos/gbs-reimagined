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

## The knob and the small screen

The screen on the board always works, even when the TV shows nothing. Its home screen says what
the board is doing (for example "Scaling" and "240p > 1080p"), the input and its frequency, the
output clock, and the board's address on your network.

| Do this | To |
|---|---|
| Press the knob | open the menu, enter an item, confirm a value |
| Turn it | move through the menu, or change a value |
| Turn it while holding it down | change a number ten steps at a time |
| Hold it for about a second | go back (and, while editing, undo the change) |

Picture settings change as you turn, so you can watch the TV. The output resolution and frame lock
change only when you press, so the picture does not jump through every mode on the way. The
screen goes back home after 30 seconds untouched and turns itself off after the time set in
**System → Display saver**; any turn or press wakes it, and so does a change of signal.

**Safe mode.** If a setting ever leaves you without a picture and without the web page, restart the
board and, **while the first screen shows "Hold knob: safe mode"**, press and hold the knob for
five seconds. The board then starts with the factory settings and 480p output, without changing
your saved settings; restart normally to get them back. (Do not hold the knob while plugging the
board in: at that moment the same button starts the chip's update mode instead.)

## The board's own switches and buttons

Some controls are wired directly into the video path; the firmware cannot see their position, so
check them when something looks wrong. This is what each one did on our bench (a console on the
front RGBS inputs, HDMI output):

| Label on the board | What it does |
|---|---|
| R / G / B knobs | Lower the level of the red, green or blue input. **Leave them at minimum** (turned fully down) for the full signal; turning one up dims that colour. Useful if a console's colours look burnt even after **Calibrate levels**. |
| HDMI ON / OFF | Turns the HDMI output off. If the board says it is scaling but the TV over HDMI shows nothing, check this switch. |
| RGBS Sync | Changes how the sync of the front RGBS inputs is taken. Leave it released unless the picture rolls or drops out with your source. |
| Sync on Green | Takes sync from the green signal: for component (YPbPr) and RGsB sources. |
| RGBHV / RGBS | Made no difference with the front RGBS inputs; it concerns the VGA input. |
| Compatibility mode | Made no difference to the input or the HDMI output; it may concern the VGA/RGBS output. |
| Power | Board power. |

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

## Picture shape, position and size

On the **Output** page:

- **Aspect** — the shape of the picture.
  - **4:3**, the default, shows consoles the way they were designed to be seen: with black bars at
    the sides at **1080p** and **720p**, filling the screen at **1280x960** and **480p / 576p**,
    and with thin black bars above and below at **1280x1024**, whose screen is a little taller than
    4:3.
  - **16:9** is for games with a widescreen mode (GameCube, PS2, Wii): the picture fills the width
    at 1080p and 720p; at 1280x960 and 1280x1024 it gets black bars above and below, so it still
    looks right on a 4:3 or 5:4 monitor. At 480p / 576p it looks the same as 4:3.
  - **Full screen** fills the screen whatever its shape, stretching the picture.
- **Horizontal position** and **Vertical position** — move the picture, for consoles that put it a
  little off centre. Each vertical step is one line of the console's picture. Where the picture
  reaches the edge of the screen, it is cut there.
- **Horizontal size** and **Vertical size** — make the picture bigger or smaller, about half a
  percent per step. They are advanced settings: turn on **Show advanced settings** at the bottom of
  the page. A picture bigger than the screen is cut at the edges.

Some combinations have no effect, because the scaler cannot do them cleanly:

- at **15 kHz (CRT)** only the position works, and **Passthrough** takes none of these settings;
- at **1080p** the picture cannot be made narrower than at the start;
- at **1280x960**, **1280x1024** and **480p / 576p** it cannot be made wider than at the start, and
  at 480p / 576p not shorter either.

At 1080p, PAL consoles look about 10 % wider than 4:3, for the same reason; choose 720p or
1280x1024 for an exact 4:3 picture.

Force 60 Hz (on the **Output** page, advanced settings) is for displays that do not accept 50 Hz:
a PAL console is then shown at 60 Hz. Frame lock cannot follow it there, so motion judders a little
(one frame in five is shown twice); leave it off if your display takes 50 Hz. At 1080p it also gives
PAL games their exact 4:3 width.

## Scanlines and the deinterlacer

On the **Picture** page:

- **Scanlines** darken the gaps between the console's lines, as a CRT shows them. They work on
  240p and 288p games and pause by themselves while the console sends 480i or 576i (menus, some
  games), coming back when it returns to 240p. **Scanline strength** goes from 1 (faint) to 8
  (black gaps); stronger lines make the picture darker — about a sixth darker at 4, the default,
  and about two fifths at 8.
- The lines are perfectly even when the output is a whole multiple of the console's picture:
  **1280x960**, **720p** or **480p** for 240p games. At **1080p** the gaps alternate slightly in
  thickness, which some people notice on large screens.
- **Deinterlacer** (advanced settings) chooses how 480i and 576i pictures are rebuilt. **Motion
  adaptive**, the default, keeps still detail such as text sharp and steady. **Bob** never shows
  combing on fast motion but makes thin lines shimmer on still pictures.

## Sampling and sync (advanced)

On the **Input** page, with **Show advanced settings** on:

- **Sync quality** shows how much of the last second the board stayed locked to the console's
  lines. 100 % is what you want; anything lower means the picture is, or is about to be, unstable.
- **Sync threshold** separates the sync from the picture for consoles that send sync on green or on
  the Y of component video. Auto finds a working value by itself; set a number only if the picture
  rolls or drops out, and watch the sync quality while you do. If the screen goes blank, wait: the
  board puts the previous value back after 20 seconds unless you confirm.
- **Sampling phase** chooses where in each pixel the input is sampled. It makes little difference
  for 240p consoles; try it on 480p or computer sources if fine detail shimmers.
- **Video LPF** filters the input before it is sampled. Lower values soften the picture and can
  calm a noisy source; Auto chooses per kind of source. This one also asks for confirmation.

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
2. Choosing the output resolution for your display
3. Picture settings
4. Troubleshooting, by the message the board shows
5. Updating and restoring
