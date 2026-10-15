# GBS Reimagined — user guide

For people who own a RetroScaler GBS-C and want to play their consoles on a modern TV or a
monitor. No programming knowledge needed.

> Install it from your browser at **https://alexmtos.github.io/gbs-reimagined/**: see
> [Installing from your browser](#installing-from-your-browser).

## What the board does

The GBS-C takes the analogue video of a console or computer and turns it into a picture your
display accepts: HDMI for modern TVs, or VGA / RGBS for monitors and CRTs. GBS Reimagined is the
software that runs on the board's small computer and decides how that conversion is done.

You can control it three ways:

- **From a phone or computer** on the same Wi-Fi, in a web page with every option explained.
- **With the knob and the small screen** on the board itself — this always works, even when the TV
  shows nothing.
- **From a USB serial terminal**, for advanced users and console switchers.

## Installing from your browser

You need the board, a computer with **Chrome** or **Edge** (phones and tablets cannot install) and
a USB cable that carries data (some cables only charge). Installing replaces the firmware the board
has now, for example GBS-Control; its settings do not carry over.

1. Power the board as usual and plug its micro-USB port into the computer. Its screen lights up as
   it always does.
2. Open **https://alexmtos.github.io/gbs-reimagined/** and click **Install**. Choose the port named
   **USB-SERIAL CH340** (or similar) and click **Connect**. The installer's window opens and
   offers **Install GBS Reimagined**. If no port is listed, see
   [If the installation does not work](#if-the-installation-does-not-work).
3. The installer's window is in English. Choose **Install GBS Reimagined**. The first time, tick
   **Erase device**, so that nothing from the previous firmware is left behind, then choose
   **Next** and **Install**. A progress bar advances; do not unplug the board until the window
   says **Installation complete!**
4. Choose **Next**. If you ticked **Erase device**, the window now asks for your Wi-Fi: choose your
   network, type its password and choose **Connect**. The password goes straight from your browser
   to the board. When the board has joined, the form closes and **Visit Device** appears; it opens
   the board's page. To do this later, choose **Skip** and follow
   [Joining your Wi-Fi](#joining-your-wi-fi). If the window does not ask, the board kept the
   network it knew, or see [Joining your Wi-Fi](#joining-your-wi-fi).
5. Connect the console to one of the board's video inputs (SCART, the RGBS jacks, VGA or
   component) and the display to its HDMI output, then switch both on. The board looks for the
   console on every input by itself. After a few seconds its screen says **Scaling** and the
   display shows the game. The board starts at the **Auto** resolution: 1280x960 for 60 Hz
   consoles, 1280x1024 for 50 Hz ones. If the display shows nothing, see
   [When something is wrong](troubleshooting.md).

### If the installation does not work

- **No port in the list:** try another cable and another USB port. On Windows, the board's USB
  chip, a WCH CH340, may need its driver: it is called CH341SER and comes from the chip's maker,
  WCH (wch-ic.com).
- **"Failed to open serial port":** close every other program that uses the port (Arduino IDE,
  PlatformIO, a serial monitor), then try again.
- **The installation stopped halfway:** click **Install** and start again. The chip's own loader,
  which the installer talks to, is never written, so an install over USB can always be repeated.
- **The board does not join your network:** check the password, and that the network is 2.4 GHz.
  Click **Install** again; the window finds the board and offers **Connect to Wi-Fi** or **Change
  Wi-Fi**.
- **The window did not ask for your Wi-Fi:** click **Install** again; the window finds the board
  and offers **Connect to Wi-Fi**. Or use the board's own network, as in
  [Joining your Wi-Fi](#joining-your-wi-fi).

## The knob and the small screen

The screen on the board always works, even when the TV shows nothing. Its home screen says what
the board is doing (for example "Scaling" and "240p > 1080p"), the input and its frequency, the
output clock, and the board's address on your network.

| Do this | To |
|---|---|
| Press the knob | open the menu, enter an item, confirm a value |
| Turn it | move through the menu, or change a value |
| Turn it while holding it down | change a number ten steps at a time |
| Hold it for about a second | go back (and, while editing, undo the change); on the home screen, open the profiles |

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

The installer asks for your network right after installing. If you skipped that, or the board
cannot reach the network it knows, it opens one of its own. The board uses 2.4 GHz networks only:
its Wi-Fi chip, an ESP8266, has no 5 GHz radio.

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

To change the network later, use the **Network** page, or the installer page: click **Install**,
choose the board's port, then **Change Wi-Fi**.

## Changes that need your confirmation

Some changes can leave the TV without a picture — the output resolution and frame lock, for
example. When you make one from the web page or the serial console, a banner asks you to
**Keep** it. If you do not answer within 20 seconds, the board goes back to how it was: a blank
TV fixes itself. The waiting time can be changed in **System → Confirm within**. Loading a profile
that changes one of these settings asks the same way, and a revert brings back everything the
profile changed.

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
- **Integer scale** — shows every line of the console as the same number of screen lines (for
  240p games: four at 1080p, where the picture is otherwise 4.5 times taller), so scanlines and
  pixels come out even. The picture gets a little smaller, with black borders, and **Vertical size**
  has no effect while it is on. For 240p games it changes nothing at 1280x960, 1280x1024, 720p and
  480p, which already show them at whole multiples; for PAL consoles it applies at every output.
  The multiple is counted in the console's 240p (or 288p) lines, so a 480i menu is exact only at
  1080p and 1280x960.
- **Horizontal size** and **Vertical size** — make the picture bigger or smaller, about half a
  percent per step. They are advanced settings: turn on **Show advanced settings** at the bottom of
  the page. It is the same setting as **Show advanced** on the **System** page: it stays on, and
  the board's menu shows them too. A picture bigger than the screen is cut at the edges.

Some combinations have no effect, because the scaler cannot do them cleanly:

- at **15 kHz (CRT)** only the position works, and **Passthrough** takes none of these settings;
- at **1080p** the picture cannot be made narrower than at the start;
- at **1280x960**, **1280x1024** and **480p / 576p** it cannot be made wider than at the start, and
  at 480p / 576p not shorter either.

At 1080p, PAL consoles and **Integer scale** need a picture narrower than the scaler alone can
make: the board then draws each line in finer steps, which your display does not notice, and the
picture comes out at its exact shape.

The output resolutions marked **(exp.)** — 1024x768, 800x600, 1600x1200 and 1920x1200 — are for
monitors and TVs that prefer them, such as a 1920x1200 PC monitor or an old 1024x768 screen. They
are computed from the display standards rather than taken from the scaler's tested set, and have
been tried on one board and one TV. If your display shows nothing after you choose one on the web
page or the serial console, the board goes back to the previous resolution unless you confirm (see
[Changes that need your confirmation](#changes-that-need-your-confirmation)). With a PAL
console these modes run at 50 Hz to keep motion smooth; the board's HDMI output may then show the
picture a little narrow and to the right. **Force 60 Hz** fixes the shape (with a small judder), or
adjust the position and size.

Force 60 Hz (on the **Output** page, advanced settings) is for displays that do not accept 50 Hz:
a PAL console is then shown at 60 Hz. Frame lock cannot follow it there, so motion judders a little
(one frame in five is shown twice); leave it off if your display takes 50 Hz.

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

## Profiles

A profile remembers everything on the **Output**, **Input** and **Picture** pages, so each console
can have its own picture. The System settings, the level calibration and the Wi-Fi are shared by
all profiles. There are 16 slots.

- **Save:** set the picture up for a console, then on the **Profiles** page choose a slot, give it a
  name and press **Save**. Tick **Load it by itself when this source appears** to have the board
  pick this profile whenever that console comes back.
- **Load:** press **Load** next to a profile, or on the board: hold the knob on the home screen (or
  open **Profiles** in the menu), choose the profile and **Load**.
- **Changes after loading** stay unsaved: the page and the board's screen show the profile as
  *modified* (a `*` after its name on the board) until you press **Save here** on it. Loading it
  again puts the saved values back.
- **Loading by itself:** when a new console's picture settles, the board loads the profile marked
  for it. It looks first for one saved from a console on the same input sending the same kind of
  picture (for example 240p/480i, or 480p) with the same number of lines, give or take two; then for
  the same input and kind of picture; then for the same input alone. Switching a console between 240p
  and 480i (or 288p and 576i) never changes the profile. Two consoles that send exactly the same
  kind of signal on the same input cannot be told apart: load their profiles by hand, or use a
  switcher (below).
- **Startup profile** (on the **Profiles** page and in **System**): *Last used* starts where you
  left off, unsaved changes included, unless a different console's profile matches the first
  picture; *Auto by input* always loads the profile that matches; a numbered profile loads that
  one at power-on.
- **Export and import:** **Export** saves one profile as a file; choose a slot and import the file
  there, on this board or another one.
- **Backup:** **Download a backup** saves every setting, the level calibration and all profiles in
  one file (never the Wi-Fi password). **Restore a backup** puts back what the file holds; profiles
  in slots the file does not mention stay as they are. A factory reset keeps the profiles.
- **Console switchers:** the commands that switchers send to RetroTINK scalers work here too:
  `SVS NEW INPUT=3` or `remote prof3` loads profile 3; `SVS CURRENT INPUT=3` loads it unless it is
  already in use and unchanged. Send them from a computer or a bridge through the board's USB port,
  or over the network to `http://gbs.local/api/svs`, for example
  `curl -d "remote prof3" http://gbs.local/api/svs`. Wiring a switcher straight to the board's
  internal serial pins is not supported: that line also carries the board's own messages.

## Updating the firmware

New versions keep your settings, profiles, level calibration and Wi-Fi. On the web page, open
**System** → **Firmware update**:

- **Check for updates** looks for a newer version on the project's site, downloads it, makes sure
  it arrived intact and installs it. The computer or phone showing the page must be online. Tick
  **Include beta versions** to be offered test versions too.
- Or install from a file: download the file ending in **.ota.bin.gz** from a release, choose it
  under **Update from a file** and press **Install**. The other file, ending in plain **.bin**, is
  for the browser installer and the board refuses it here.
- Or over USB, from the installer page: click **Install**, choose the board's port, then **Update
  GBS Reimagined**. This also keeps everything.

Sending takes a few seconds. The board then restarts by itself, and the picture and the page are
back in about 20 seconds. Keep the board powered meanwhile: if the power goes out while it copies
the new version, it may need to be installed again from the browser over USB (choose the same
version and do not erase the board, so your settings stay).

Before writing anything, the board checks that the file was made for it, and refuses one meant for
another board. **Check for updates** also refuses a download that arrived damaged. If an update
does not go through, the board keeps the version it had.

## When something is wrong

The board always says what it is doing: on its own screen, on the **Status** page and in the
console's `status`. [When something is wrong](troubleshooting.md) lists each message with what to
try, most likely first.

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
| `profile` | the saved profiles; `profile load 3`, `profile save 3 Mega Drive`, `profile source 3 on` |
| `factory-reset` | settings and levels back to defaults, profiles kept (type it twice) |

## Coming in this guide

1. Choosing the output resolution for your display
2. Picture settings
