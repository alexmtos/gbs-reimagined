# User experience specification

For designers and developers of the three interfaces (web, OLED + encoder, serial console). What
the user can do, what each interface shows, and the conventions they share. The evidence is in the
[market study](../research/market-scalers.md) (cited as `MS §n`).

## Principles

1. **A good picture with zero setup.** Plugging in a source and a display is enough; every setting
   has a default chosen for compatibility first (MS §1.1).
2. **Two tiers.** Every interface shows *Essential* controls first and keeps *Advanced* ones one
   step away, behind an explicit toggle (MS §1.3).
3. **Never strand the user.** Anything that can blank the output reverts after 20 s unless
   confirmed; a safe mode is always reachable; the OLED works when the TV shows nothing (MS §1.4).
4. **Every control visibly does something, or says why it cannot.** A setting that has no effect in
   the current state is shown disabled with the reason ("Frame lock: inactive — no signal").
5. **Say it in words, then in numbers.** Status leads with a sentence ("Scaling 240p from RGBS to
   1280×960 at 59.94 Hz") followed by the figures experts want (MS §1.8).
6. **Use the community's names.** Labels follow the vocabulary users already know from RetroTINK,
   OSSC and MiSTer (MS §13). The chip's register names appear only in expert tools.
7. **Honest scope.** No feature is offered that the hardware cannot deliver (masks, HDR, VRR, BFI,
   4K, image-based auto phase — MS §15).

## Settings catalogue

Each setting lives in `schema/settings.yaml` with its id, type, range, default, tier, labels and
help in every language, and the milestone that delivers it. Identifiers below are the schema ids.

### Output

| Id | Label | Values (default first) | Tier | Milestone |
|---|---|---|---|---|
| `output.resolution` | Output resolution | **Auto** (960p for 60 Hz sources, 1024p for 50 Hz), 1080p, 1280×1024, 1280×960, 720p, 480p/576p, 15 kHz, Passthrough (exp.); M2: 1024×768, 800×600, 1600×1200, 1920×1200 (exp., [ADR 0015](../adr/0015-computed-modes-on-the-1080p-program.md)) | Essential | M1 |
| `output.frameLock` | Frame lock | **On** (output rate follows the source via the clock generator), Off (fixed 60/50 Hz) | Essential | M1 |
| `output.force60` | Force 60 Hz | **Off**, On (50 Hz sources output at 60 Hz) | Advanced | M2 |
| `output.aspect` | Aspect | **4:3**, 16:9, Full | Essential | M2 |
| `output.integerScale` | Integer scale | **Off**, On (the source's lines at a whole multiple; vertical size then has no effect) | Essential | M2 |
| `output.hPosition` / `vPosition` | Position | −64…+64, **0** | Essential | M2 |
| `output.hSize` / `vSize` | Size | −64…+64, **0** | Advanced | M2 |

### Input

| Id | Label | Values | Tier | Milestone |
|---|---|---|---|---|
| `input.port` | Input | **Auto**, RGB (SCART / RGBS / VGA), Component (YPbPr) | Essential | M1 |
| `input.syncThreshold` | Sync threshold (SOG) | **Auto**, 0–31 | Advanced | M2 |
| `input.phase` | Sampling phase | **Auto**, 0–31 (shown in degrees, 11.25° steps) | Advanced | M2 |
| `input.autoGain` | Auto gain | **Off**, On (lowers the gain while the picture clips; restarts per source) | Advanced | M1 |
| *Calibrate levels* (action) | Calibrate levels | with a white screen: per-channel gain so white sits just under clipping, kept per input | Advanced | M1 |
| `input.gain` | Gain (R, G, B) | 0–255 each (**preset value**) | Advanced | M2 |
| `input.videoLpf` | Video LPF | **Auto**, 150, 110, 70, 40 MHz | Advanced | M2 |

### Picture

| Id | Label | Values | Tier | Milestone |
|---|---|---|---|---|
| `picture.brightness` | Brightness | −32…+32, **0** | Essential | M1 |
| `picture.contrast` | Contrast | −32…+32, **0** | Essential | M1 |
| `picture.saturation` | Saturation | −32…+32, **0** | Essential | M1 |
| `picture.sharpness` | Sharpness | **Off**, Low, Medium, High | Essential | M1 |
| `picture.scanlines` | Scanlines | **Off**, On (progressive 15 kHz sources) | Essential | M2 |
| `picture.scanlineStrength` | Scanline strength | 1–8, **4** | Essential | M2 |
| `picture.deinterlace` | Deinterlacer | **Motion adaptive**, Bob | Advanced | M2 |

### System

| Id | Label | Values | Tier | Milestone |
|---|---|---|---|---|
| `system.language` | Language | **English**, Português (Brasil) | Essential | M1 |
| `system.oledSaver` | Display saver | Off, 1, 3, **5**, 10 min | Advanced | M1 |
| `system.encoderReverse` | Reverse knob | **Off**, On | Advanced | M1 |
| `system.startupProfile` | Startup profile | **Last used**, Auto by input, profile N | Essential | M2 |
| `system.revertSeconds` | Confirm changes within | 10, **20**, 30 s | Advanced | M1 |

Network credentials and the hostname are not settings: they live in their own store, are never
exported in a profile, and are changed through the network pages.

## Profiles (M2)

Specified in [ADR 0014](../adr/0014-profiles-are-saved-settings.md); the points below are the UX side.

- Up to 16 named profiles. A profile stores every setting of the Output, Input and Picture groups,
  including input and output resolution (a common complaint about competitors, MS §15 C11); System
  settings and the level calibration stay global. It is one action away on every interface.
- Loading copies the values; changes afterwards show the profile as *modified* until it is saved.
  The profile in use shows on the web status page, the OLED home screen (last line, `*` when
  modified) and the console `status`.
- **Auto-load by input signature**, for profiles saved with "use for this source", most specific
  match first: port + sync type + standard + lines → port + standard → port; no match loads
  nothing. 240p↔480i never reloads (MS §15 C12). A manual load holds until the source changes.
- **SVS-compatible triggers** on serial and HTTP (`SVS NEW INPUT=<n>`, `remote prof<n>`), so
  existing switchers (Donut Dongle, Extron/gscart bridges) work unchanged (MS §15 C13).
- Export/import as JSON; a full backup includes all profiles and settings.
- **Web:** a Profiles page lists the slots in use with Load, Save here, Rename, Use for this source /
  Stop loading by itself, Export and Delete (saving over and deleting ask for a second press); a form
  saves the current settings or imports a file into a chosen slot; Backup downloads and restores
  everything. The startup profile is a list (Last used, Auto by input, each slot by name).
- **OLED:** *Profiles* in the menu, or a long press on the home screen, lists the profiles (`«` marks
  the one in use; the bottom line says which source it loads for, "Only when chosen", or "Changed,
  not saved" for the one in use when it is modified) followed by *Save as new* (first free slot, no
  signature; shown while a slot is free) and *Back*. A profile opens *Load*, *Save here*,
  *Auto-load* On/Off (for the current source) and *Delete*; saving over and deleting ask for a
  second press. Loads from the OLED confirm themselves, like its own changes. Names are given on
  the web or the console: the OLED has no text entry.

## Safety

- **Revert:** changing the output resolution, frame lock or advanced sampling from the web or the
  serial console starts a countdown (default 20 s). The web page and the OLED both show it with
  *Keep* / *Revert* buttons. Changes made on the OLED confirm themselves when the user presses
  *Keep* there.
- **Safe mode:** hold the knob for 5 s while the boot splash shows (not at power-on: the button is
  the ESP8266 boot strap and would enter the bootloader). Safe mode boots with default settings
  and 480p output without touching saved profiles.
- **Factory reset** asks twice and offers a backup download first (web) or says how to get one
  (OLED, serial). It keeps the saved profiles (none is in use afterwards); they are deleted one by
  one.

## Status

One status model feeds all interfaces.

1. **Headline** — one sentence, from a fixed ordered list, so the most fundamental problem always
   wins: *scaler not answering* › *no signal on {input}* › *sync unstable* › *signal found, settling*
   › *locking* › *scaling {source} to {output}* › *paused*. The healthy sentence never claims the
   display shows a picture; the board cannot see its output.
2. **Source:** input, sync type (RGBS / RGBHV / sync-on-green), standard, lines, progressive or
   interlaced, H frequency (kHz), V frequency (Hz).
3. **Output:** mode, pixel clock, rate, frame-lock state with the reason when inactive.
4. **Sampling:** samples per line, phase, SOG threshold, gain, PLL lock (n/8).
5. **Board:** firmware version, uptime, free heap, Wi-Fi RSSI and address, Si5351 detected.

## Interfaces

### Web

Pages: **Status** (headline, figures, quick input/output/profile pickers) · **Picture** · **Output** ·
**Input** · **Profiles** · **System** (network, language, display, firmware update, backup, factory
reset) · **Diagnostics** (event log, write journal, chip capture and diff, register explorer — all
behind the Advanced toggle).

- Live state through one push stream; a banner shows connection state (*live*, *reconnecting*,
  *firmware mismatch*).
- Controls show three states: default (grey), changed and pending (amber), saved (normal).
- Mobile-first, one column on phones; keyboard reachable; dark theme by default (used next to a TV)
  with a light theme.
- Every control has a one-line hint; hints and labels come from the schema, so they match the OLED
  and the console.
- First visit runs the **setup wizard**: language, display type (HDMI TV · VGA monitor · 15 kHz CRT),
  region (60/50 Hz), Wi-Fi. The CRT choice restricts the output list to 15 kHz.

### OLED + encoder

128×64, six text lines in a font with Latin-1 accents.

- **Home** shows the status headline (wrapped), the source line and the output line; it is the
  screen the board returns to after 30 s of inactivity.
- **Menu:** Output ▸ Input ▸ Picture ▸ Profiles ▸ Network ▸ Status ▸ System. Advanced items appear
  when *System ▸ Show advanced* is on.
- **Gestures:** rotate = move or change; press = enter or confirm; long press (≥ 0.6 s) = back;
  press-and-turn = coarse steps (×10); double press on a value = reset to default.
- The bottom line shows a terse hint for the highlighted item (≤ 21 characters).
- **Network ▸ Join** shows a QR code for the setup access point and then the board's address.
- The revert countdown takes over the screen with *Keep* highlighted.

### Serial console (115 200 baud, English)

Line-based, case-insensitive, `help` everywhere.

```text
status                       headline and figures
get <setting>                e.g. get output.resolution
set <setting> <value>        e.g. set picture.sharpness medium
list [prefix]                settings with current values
output <mode>                shortcut for set output.resolution
input <auto|rgb|component>
profile [list]               saved profiles; * marks the one in use
profile load|delete <n>
profile save <n> [name]      current settings into slot n (keeps its name and signature)
profile rename <n> <name>
profile source <n> on|off    load n by itself for the current source, or only when asked
wifi <ssid> <password>       join a network
keep | revert                answer a pending revert
calibrate                    calibrate levels against a white screen
reboot | factory-reset       factory-reset asks to be typed twice
SVS NEW INPUT=<n>            switcher compatibility (also SVS CURRENT INPUT=<n>)
remote prof<n>               RetroTINK-style profile trigger
expert on|off                raw chip tools: reg <FIELD> [value], dump <seg>, levels r|g|b [n]
json on|off                  machine-readable replies for tools
```

## Internationalisation

- English is the default and the reference; Brazilian Portuguese ships from M1. Strings live in
  `schema/i18n/<locale>.yaml`, keyed by setting id and message id; the generator produces the web
  bundles and the OLED tables.
- The serial console stays English (user decision), so scripts never depend on the language.
- The OLED font covers Latin-1, which includes every Portuguese character.
