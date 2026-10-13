## 12. Independent cross-check: the Wobbling Pixels "scaler shootout" (RetroTINK-4K CE vs Morph 4K vs OSSC Pro)

The most detailed independent comparison available is a public spreadsheet by YouTuber Wobbling Pixels, promoted by RetroRGB ("Scaler Shootout Document Update", 24 Feb 2026). The sheet itself says "Latest update: October 2026", so it reflects current firmware. It compares the **RetroTINK-4K CE**, the **Morph 4K + Analog Bridge (variant 1)** and the **OSSC Pro + Legacy AV board**. The RetroTINK-4K Pro is deliberately excluded as "a much more powerful scaler at a higher price point". Sources: [spreadsheet](https://docs.google.com/spreadsheets/d/16FBPBD3A1vN_khznTUO1bGrb3LxTIQXfwWfusPeNSsw), [RetroRGB post](https://retrorgb.com/?p=52163).

These are one reviewer's opinions and measurements, but they line up with forum consensus. The parts most relevant to UX:

| Topic | RetroTINK-4K CE | Morph 4K + Analog Bridge | OSSC Pro + Legacy AV |
|---|---|---|---|
| Price incl. accessories (US, early/late 2026) | ~$475 (+PSU) | ~$375 (+PSU, microSD) | ~$430 |
| **OSD/menu usability score** | **8.7**: "well organized and intuitive"; cropping logic takes time to learn | 8.0: mostly well organized, but input selection is "nested" and unintuitive. Input source and output resolution are not saved per profile | **6.5**: "logically structured" but slow to learn. The three operating modes (Pure LM / Adaptive LM / Scaler) "could be overwhelming for casual users". The built-in LCD helps when the TV loses sync |
| Firmware maturity | 8.8, very few bugs | 7.6, "several bugs", factory reset sometimes needed | 8.8 |
| Remote | Nearly every button mapped; **profiles assignable to numeric buttons** | Few buttons mapped; CEC and a **Web-GUI remote** as alternatives | Nearly every button mapped; hot-key to the profile list (profiles not assignable to buttons) |
| File management | SD card via PC | SD via PC **or over Wi-Fi web GUI** (create/rename/upload/delete profiles, masks, modelines; firmware update) | microSD via PC; profiles also in internal memory (up to 115); renaming via config file |
| Auto-load of profiles | At power-up; MiSTer core / DV1-capable HDMI mods; **SVS switch port** (analog switches via extra hardware) | At power-up; SDP/DV1 InfoFrames; **CEC**; per resolution; per Game-ID | At power-up only |
| Auto Phase / Auto Decimation | Yes; phase re-optimized automatically after profile load and on resolution change | No (manual sub-/decimation phase) | Yes (not always correct) |
| Auto Sample Rate Detection | Vertical **and** horizontal resolution | Vertical and horizontal (partial on Saturn, no N64 de-blur) | Vertical only |
| Auto Gain | Yes (RGB, YPbPr, VGA) | Yes | **No**; manual gain "requires additional tools and more effort" |
| Sync/lock modes | Triple Buffer, Auto Triple Buffer, Genlock, VRR, Frame Lock, TBC | Triple Buffer, Auto Triple Buffer, Magiclock2/Genlock, Frame Lock | Triple Buffer, Source Hz, Frame Lock (plus Pure/Adaptive line-multiplier modes) |
| Measured lag (TimeSleuth, 240p60) | Genlock ~2.05 ms; Triple Buffer 1.6–17.5 ms rolling | Genlock 1.56 ms; Triple Buffer 17–33 ms rolling | **Pure LM 0.06–0.10 ms**; Adaptive LM 0.84 ms; Scaler (Framelock) ~18 ms; Triple Buffer 18–34 ms rolling |
| Resolution transitions (240p↔480i) | Seamless or nearly, with "Smooth I/P Switch" | 1–2 s with visible shifting/flicker | Nearly seamless |
| "Blank Res. Change" (black frames to hide transitions) | Yes | Yes (incomplete) | No |
| Aspect / auto-scale presets | 4:3, 16:9, 1:1 PAR, Auto Fill, Auto Fill Integer, Auto Fill Integer+, Auto Fill Integer HV, Auto Fill Integer HV+ | Auto Fill, 4:3 (SDTV), 32:27 (SDTV), 160:119 (CRT TV), 16:9, 1:1 PAR, many DAR/PAR values | Auto, Integer underscan, Integer overscan, 4:3, 8:7, 16:9, 1:1, Full |
| Deinterlacing methods | Motion Adaptive, CRT Simulation, Weave, Bob, Blend, Linear, LCD Blending | Motion Adaptive (tunable combing vs flicker), CRT Simulation, Weave, Bob | Motion Adaptive (tunable), Weave, Bob, **Non-Interlace Restore** |
| Bundled profiles | Profiles for 65 systems; ~145 CRT/LCD look profiles | 28 systems; ~16 look profiles | 14 systems + 5 presets; ~15 look profiles |

**Lessons for GBS Reimagined:**

1. **Usability scores follow defaults and automation, not feature count.** The CE wins because it auto-phases, auto-detects sample rate, auto-gains and ships many ready-made profiles. The OSSC Pro has as many options but scores lowest because users must understand its modes.
2. **Save everything in a profile**, including input port and output resolution. The reviewer flags both the Morph and the OSSC Pro for not doing this.
3. **Use a secondary display when the main picture is gone.** The OSSC Pro's LCD is praised because it still works when the TV loses sync. Our OLED (and the web UI) have the same advantage.
4. **Hide resolution changes.** Black frames during I/P switches ("Blank Res. Change"), plus a mode that keeps the output timing constant across them ("Smooth I/P Switch"), are visible quality signals for users.
5. **Lag terms are quantified.** The reviewer's classes are: < 1 ms "zero lag", 1–10 ms "extreme low lag", 11–19 ms "low lag", 20–30 ms "medium", > 31 ms "high". Lag claims for GBS Reimagined should be measured and published in the same terms.
