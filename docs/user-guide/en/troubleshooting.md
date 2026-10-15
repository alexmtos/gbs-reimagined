# GBS Reimagined — when something is wrong

For people using GBS Reimagined on a RetroScaler GBS-C. The rest of the guide is in
[README.md](README.md).

The board always says what it is doing: on its own screen, on the **Status** page and in the
console's `status`. Find the message here.

**No signal on RGB** (or on Component, or Auto; on the board: *No signal*) — the board hears no
console on that input. For a few seconds after the board is switched on, this is normal.

- Check that the console is on and its cable is firmly in the board.
- The **Input** setting (on the **Input** page, or in the board's menu) should be **Auto**, or
  match the cable:
  **RGB** for SCART, the RGBS jacks and VGA; **Component** for YPbPr.
- Component and sync-on-green cables need the board's **Sync on Green** switch pressed (see
  [The board's own switches and buttons](README.md#the-boards-own-switches-and-buttons)).

**Sync is arriving, but it is not stable** (*Sync unstable*) — the board hears the console but keeps
losing it.

- Check the cable and its connection, and the **RGBS Sync** and **Sync on Green** switches.
- For component and sync-on-green sources, try **Sync threshold** (on the **Input** page, advanced
  settings) while you watch **Sync quality**; see
  [Sampling and sync](README.md#sampling-and-sync-advanced).

**Signal found, settling** or **Locking to the source** — normal for a few seconds after a console
is switched on or changes mode. If it does not end, treat it as an unstable sync.

**Scaling 240p to 1280x960** (or another pair), but the display shows nothing:

- Check the display's input and the board's **HDMI ON / OFF** switch.
- The display may not accept that resolution. On the board, open the menu, **Output**, **Output
  resolution**, choose **1080p** or **720p** and press the knob. A change made on the web page goes
  back by itself after 20 seconds unless you press **Keep**.
- If nothing else helps, start the board in safe mode (see
  [The knob and the small screen](README.md#the-knob-and-the-small-screen)).

**Scaling**, but the picture is wrong: washed-out or burnt colours, see
[Input levels](README.md#input-levels); size, shape or position, see
[Picture shape, position and size](README.md#picture-shape-position-and-size).

**The scaler chip is not answering** (*Scaler not answering*) — the board's video chip does not
respond. Unplug the board's power for ten seconds and plug it back. If the message stays, the board
needs repair: the firmware cannot work without that chip.

**Safe mode: the saved settings are not in use, and changes are not saved** (on the board: *SAFE
MODE*) — the board was started with the knob held (see
[The knob and the small screen](README.md#the-knob-and-the-small-screen)). Restart it normally to
get your settings back.

**The web page does not open:** use the address shown on the board's home screen (some phones do not
understand `gbs.local`). If the board's screen shows its own network (**GBS-** and four characters),
it could not join yours: see [Joining your Wi-Fi](README.md#joining-your-wi-fi).
