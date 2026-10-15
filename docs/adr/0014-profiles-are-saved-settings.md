# 0014. Store profiles as saved setting values, chosen by input signature

- Status: accepted
- Date: 2026-10-14

## Context

M2 adds profiles: named sets of settings that a user, the board itself or an external switcher can
load ([roadmap](../roadmap.md), [ux.md, Profiles](../design/ux.md#profiles-m2)). Forces:

- **Settings are not registers** ([ADR 0007](0007-settings-not-registers.md)). The reference fork
  keeps 8 profiles in `/p/<n>` with a per-standard assignment, beside up to 64 user register
  overrides, and upstream stores whole 432-value programs per standard and slot
  ([gbs-control-mechanics.md](../research/gbs-control-mechanics.md) §§2.4, 11.1, 11.2; reference
  project, not reproduced here). Neither fits a firmware whose only user-facing state is its
  settings.
- **Persistence already exists:** `/settings.bin` is a CRC-checked record of (id hash, value) pairs,
  written atomically through `FileStore` (`core/record.*`, `core/settings_store.*`); records are
  capped at 512 bytes. LittleFS has 1000 KB for settings and profiles (tech-stack.md).
- **Telling consoles apart:** the engine knows the port, the sync type, the standard and the line
  count per field. 240p and 480i are one standard and must never reload a program (video-engine.md).
  Two consoles with the same port and timing cannot be told apart by the board.
- **Triggers:** SVS (`SVS NEW INPUT=<n>`, `SVS CURRENT INPUT=<n>`) and `remote prof<n>` are what
  switchers such as the Donut Dongle and gscart bridges already send to RetroTINK scalers
  (market-scalers.md §§4, 15 C13). The board's free GPIOs cannot take an IR receiver without a
  hardware change: GPIO15 is a boot strap, GPIO16 has no interrupt, GPIO2 would cost the status LED
  (board-gbsc.md, B-06).
- **Memory:** a POST body arrives as a heap `String`; a backup measured 1 967 bytes with two
  profiles of about 560 bytes each ([bench M2](../bench/M2.md), profiles), so 16 profiles make
  about 10 KB, against an idle heap of 38 KB (bench M1, B-03). Static RAM is within 2 KB of its 40 KB budget
  (memory-budget.md), so new fixed text belongs in flash.

The user decided the open points on 2026-10-14 (decisions 1–7 below). Decisions 8–10 and the
details added to the others came up while building it; the user approved them the same day.

## Decision

1. **A profile is a saved set of setting values**: every setting of the Output, Input and Picture
   groups, stored as (id hash, value) pairs like `/settings.bin`. System settings, the level
   calibration and network credentials are global and never part of a profile.
2. **Up to 16 profiles**, slots 1–16, one file each (`/profile<n>.bin`, a record with magic `GBRP`): a
   name (up to 23 bytes of UTF-8 limited to Latin-1, which the OLED can draw), an optional input
   signature and the values. Unknown ids are ignored on load, as for settings.
3. **Snapshot model.** Loading copies a profile's values into the current settings; later changes
   affect the current settings only, and the board shows the profile as *modified* until the user
   saves it (over the same slot or another). The active slot and the modified flag survive a reboot
   in `/profile-active.bin`. Saving over a slot keeps its name and signature unless the user gives
   new ones.
4. **Input signature** = port, sync type, standard and source lines per field, captured from the
   running source (10 stable engine ticks, 200 ms) when the user saves with "use for this source".
   Auto-load runs only for profiles that have a signature, when a newly settled source differs from
   the last one evaluated (port, sync and standard equal and lines within two count as the same).
   Match levels, most specific first: port + sync + standard + lines (±2) → port + standard → port;
   ties go to the lowest slot. No match loads nothing. 240p↔480i is the same source and never
   reloads. An automatic load leaves `input.port` alone (the source is already on that input).
5. **Triggers** without new hardware: the web page, the OLED (Profiles in the menu, and a long
   press on the home screen), the serial console (`profile list|load|save|rename|source|delete`),
   HTTP (`/api/profiles`, `/api/profile`), and the switcher lines `SVS NEW INPUT=<n>` and
   `remote prof<n>` (load profile n) on the serial console and as the body of `POST /api/svs`.
   `SVS CURRENT INPUT=<n>` loads only if profile n is not the active, unmodified one.
6. **Revert:** a load from the web or the console that changes a risky setting arms the timed
   revert; a revert puts every setting back as it was before the load (changes made while waiting
   go too) and the profile that was active then. A new source or a switcher line arriving while a
   revert is pending undoes the unconfirmed change first, then loads.
   Loads from the OLED confirm themselves, like the OLED's own changes (ux.md, Safety); automatic
   and switcher loads do not arm it either: the signature or the switcher is the user's consent.
   Saving is refused while a revert is pending.
7. **Backup:** a full backup is a JSON document with the format name and version, the firmware
   version, the schema hash, every current setting (including System) as text, the level
   calibration and all profiles (values as text, keyed by setting id). Wi-Fi is never included.
   Single profiles export and import on their own. Restore is sent piece by piece (the settings,
   the calibration, then one profile per request), never as one large body; slots the backup does
   not mention stay. Importing over the profile in use leaves no profile active.
8. **`system.startupProfile`**: Last used (−1, the default) boots the saved current settings and the
   saved active profile; the first source keeps that profile, unsaved changes included, when it is
   the profile's best match, and otherwise auto-load picks as usual. Auto by input (0) lets the
   first source auto-load like any other, reloading a modified profile. Profile n (1–16) loads n at
   boot without the revert, then behaves like Last used. Safe mode ignores profiles entirely: no
   load, no save.
9. **A manual or switcher load owns the next source**: a source that settles within 10 s of the
   load is recorded without auto-loading, because the load itself may have changed the port, or the
   switcher may announce the input before its video arrives. Later changes auto-load again.
10. **Factory reset keeps the profiles** and leaves none active; deleting them is one by one.

## Consequences

- Profiles reuse the settings machinery (records, hashes, text forms, the controller's apply path);
  a renamed id resets that value in old profiles exactly as it does in `/settings.bin`.
- A profile that changes the output resolution, loaded automatically, makes the source load twice
  the first time it appears (the engine loads before the controller sees the settled signature).
  Measure it; if it matters, the signature can be evaluated at detection instead.
- Profiles cannot tell two consoles with identical timing apart; SVS or a manual load is the answer,
  and the user guide says so.
- SVS over the USB serial port works from a computer or a bridge; a switcher wired directly to the
  board's UART would share the line with the console output and the boot messages, which is not
  supported.
- The 10 s hold of decision 9 is a design choice, not a measurement; the bench should confirm a
  switcher's video settles inside it.
- IR stays out until the hardware question (B-06) is settled.
- New interfaces to keep stable: the profile file format, `/api/profiles`, `/api/profile`,
  `/api/svs`, `/api/backup`, `/api/levels`, the console commands and the backup JSON.
