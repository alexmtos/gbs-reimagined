# Market research: retro video scalers, and what GBS Reimagined should copy

**Project:** GBS Reimagined, open-source firmware for the RetroScaler GBS-C (Tvia TV5725 + 2 MB SDRAM, Si5351, ESP8266, SSD1306 128x64 OLED, rotary encoder, MS9288A VGA-to-HDMI, LM1881).
**Date:** 2026-10-13.
**Scope:** UX and feature research on the products below. For each one: I/O, detection, sampling, scaling, timing, deinterlacing, latency, image processing, profiles, menus, OSD, firmware update, web/app control, diagnostics, and user praise and complaints.

**Method.** Official firmware changelogs, wikis and manuals come first, then independent reviews, then forum and issue-tracker reports. Some files were already downloaded by an earlier, interrupted pass and were reused: OSSC/OSSC Pro wiki sources, RetroTINK-5X wiki pages, the RT4K experimental changelog, the Wobbling Pixels shootout CSVs and gbs-control sources.

**Confidence legend**
- **[VERIFIED]** — read in a primary source.
- **[REPORTED]** — from a single secondary source.
- **[RECONSTRUCTED]** — assembled from several partial primary references, so order and wording may differ on the device.
- **[UNVERIFIED]** — inferred, or from memory or general knowledge, and needs checking.

**Sources that could not be read.** The official RetroTINK-4K manual lives on the ConsoleMods wiki ([CM-RT4K]). It returned a Cloudflare challenge to every automated method tried (direct, raw/API, reader proxy), and the Internet Archive was offline. The RT4K menu tree in section 2 is therefore **[RECONSTRUCTED]** from the official firmware changelogs, shop pages and RetroRGB coverage. Time Extension reviews and TechSpot also returned 403.

---

## 1. Executive summary: the UX patterns that make these products easy to use

The easiest products are not the ones with the fewest options. They are the ones where the user never has to understand the options to get a good picture, and can still find the options once they want them. The patterns below recur across the market leaders. Each is tagged with where it is done best.

1. **A working picture at power-on, with zero setup.** The RT5X manual's quick start ends with "If you are happy with the results, please proceed with gaming and read no further!" ([CM-RT5X-MAN]). The community wiki's beginner chapter is literally titled "Just Play the Damn Game™" ([CM-RT5X]). Defaults are CEA-standard 1080p, Triple Buffer and generic 4:3 sampling: maximum compatibility first, lowest lag second.
2. **Automation beats options in independent scoring.** In the 2026 Wobbling Pixels shootout, the RetroTINK-4K CE scored highest on usability (8.7) and the OSSC Pro lowest (6.5), although the OSSC Pro has comparable depth. The CE's lead came from auto-phase, auto sample-rate detection (vertical **and** horizontal), auto gain, and profiles for 65 systems. The OSSC Pro's three operating modes "could be overwhelming for casual users" ([WP-SHEET], [RRGB-SHOOT]).
3. **Two-tier menus: a simple page in front of an expert tree.**
   - RT4K "Simple Mode" (June 2026) puts input, output, profiles, basic picture controls and **a one-line tip for the highlighted item** on a single page. One remote key (AUX8) jumps between the simple and full menus ([RT4K-EXP], [RRGB-SIMPLE]).
   - OSSC/OSSC Pro offer an OSD "Simple/Full/Off" setting.
   - The RT5X hides exotic resolutions behind an "Advanced Res" toggle ([CM-RT5X]).
4. **Never strand the user without a picture.**
   - Morph 4K reverts an unconfirmed output resolution after 30 s; Morph 2K after 20 s, and it requires OK twice ([PFX-M4K], [PFX-M2K]).
   - Every product has an emergency reset: Morph holds Right for 5 s to force 1080p60; RT5X holds SAFE for 1 s for the default 1080p profile, or Back ×6 for the default profile.
   - The OSSC/OSSC Pro front display is praised because it "works when sync is lost" ([WP-SHEET]). Our OLED is the same asset.
5. **Hide resolution changes.** The RT4K/RT5X "Smooth I/P Switch" and "Blank Res. Change" make 240p↔480i (the "Chrono Cross problem") seamless. The shootout counts this as a quality signal ([RT4K-EXP], [CM-RT5X], [WP-SHEET]).
6. **Profiles that carry everything, load themselves, and are one button away.**
   - RT4K: 12 numeric remote buttons and a power-on profile. Auto-load by MiSTer core (DV1) or by switch input (SVS). "Keep Cur. Input / Keep Cur. Res" decide whether a profile may change the input or the resolution ([RT4K-EXP]).
   - Morph: a cascading auto-load from most to least specific context (Game ID → InfoFrame → CEC → input mode), with per-resolution sub-files ([PFX-AUTOLOAD]).
   - OSSC: "Link input→prof" and "Link prof→input" ([JHQ-OSSC]).
   - The most common complaint is a profile that **doesn't** store input and output resolution (Morph, OSSC Pro) ([WP-SHEET]).
7. **Visual feedback while calibrating.**
   - RT4K/RT5X tint clipped pixels red/blue or blue/purple while ADC gain/offset is adjusted.
   - RT5X draws magenta bars over cropped areas and shows a "1.00" aspect ruler.
   - RT4K shows the BFI pattern, a VU meter and the vertical-pan scale factor ("6x") ([RT4K-EXP], [CM-RT5X]).
8. **Status screens written for humans, with the numbers experts need.**
   - RT5X Status: Source, Output Res, Input Lines, H-Freq, V-Freq, Samples/Line, ADC Clock and **Buffer Lag in ms** ([CM-RT5X]).
   - OSSC INFO: lines, p/i, cycles per frame ([JHQ-OSSC]).
   - Morph: RX Input Analyzer, TX Info, and Debug/Self Test with PLL locks, lag and temperature ([PFX-M4K]).
   - RT4K's Sync Lock line **explains why** VRR fell back to buffered mode ([RT4K-EXP]).
9. **Network convenience, done carefully (Morph 4K/2K).**
   - Wi-Fi setup by QR code to a soft-AP wizard.
   - mDNS hostname, web remote, and a file manager for profiles, masks and modelines.
   - OTA update with channels and in-OSD release notes, plus a rescue system ([PFX-M4K]).
   - Morph uses an ESP32 for this, which is the closest analogue to our ESP8266.
10. **Small consistent interaction conventions** make dense menus usable:
    - Morph shows untouched values grey, pending (unconfirmed) values yellow and saved values white.
    - Coloured remote keys step ×10 (coarse) and a dedicated key resets a value to default.
    - File browsers remember position and show "more above/below" arrows (RT4K).
    - Holding a key auto-repeats (RT5X) ([PFX-M4K], [RT4K-EXP], [CM-RT5X]).
11. **Firmware updates must not destroy user work.** RT4K, OSSC and Morph keep profiles on SD across updates. The RT5X's notice "Firmware updates erase saved profiles" is a long-standing pain point ([RT5X-EXP]).
12. **Automation hooks for multi-console setups.**
   - The RT4K serial protocol (`remote <button>`, `SVS NEW INPUT=n`) spawned a community ecosystem (Donut Dongle, Extron/gscart integration) with no extra work from the vendor ([RT4K-EXP], [SVIRANT-DD]).
   - OSSC/OSSC Pro expose `input`, `prof` and `rc` serial commands ([JHQ-OSSC], [JHQ-OSSCPRO]).

**What this means for a TV5725 + ESP8266 board.**
- Most of the *UX* patterns above cost RAM and design effort, not silicon, so they are **feasible**: defaults, two tiers, revert-on-timeout, OLED-as-lifeline, profiles and auto-load, status pages, web wizard and remote, human-readable serial, smooth I/P.
- Most of the *image-processing* differentiators of 2024–2026 are **not feasible**: masks, HDR, VRR, BFI, xBR, 4K, true auto-phase from frame analysis, and an on-TV text menu.
- The project should compete on "plug-and-play plus honest diagnostics", not on CRT simulation. See sections 14–15.

---

## 2. RetroTINK-4K Pro and RetroTINK-4K CE

### 2.1 Product and I/O

| | RT4K Pro | RT4K CE |
|---|---|---|
| Price (Oct 2026) | US$750 | US$475 ([RT-4KCE-SHOP]) |
| Analog inputs | HD-15 (RGBHV/RGBS/RGsB/YPbPr/S-Video/CV), SCART (RGBS/RGsB/YPbPr/SV/CV), rear RCA, front CV/SV | Same list ([RT-4KCE-SHOP]) |
| Digital input | HDMI | HDMI 1.4, up to 1080p60/1200p60 ([RT-4KCE-SHOP]) |
| Audio in | Analog per input, TOSLINK, HDMI | Same; audio source independent of video |
| Output | HDMI 4K60 4:4:4, deep color, 1440p120/1080p120, custom modelines, interlaced outputs (480i/1080i) | Same class; CE max input width ~2048 px ([RT4K-EXP] v1.44) |
| Pro-only items seen in changelog | RoTATE (rotation), ASRC audio resampler, new horizontal-blur filter engine, 2nd-gen IVTC engine | — ([RT4K-EXP]) |

- "Pro-only" status is **[VERIFIED]** for each item named in the changelog. There may be more differences.
- A sibling, the **RetroTINK-6X CE** (Aug 2026, US$230), shares the platform but is analog-only at 1440p. See section 9.

### 2.2 Detection and sampling
- **Input mode detection** covers 240p/288p/480i/576i/480p/576p/720p/1080i/p, VGA/DOS modes and "Wide Tolerance" for bad sync (Neo Geo, Atari 2600/7800). It replaced the old "Slow Lock" ([RT4K-EXP] v1.0.8).
- **Auto sample-rate detection ("auto decimation").** The ADC samples at a high rate (up to 4095 samples per line). The firmware then detects the console's dot clock as a "decimation factor" and an auto-phase.
  - It runs continuously and was rewritten three times; v1.84 (Sep 2026) promises "no more hunting between multiple valid phases" ([RT4K-EXP]).
  - Phase is re-optimised after profile load and resolution changes ([WP-SHEET]).
- **Sample Rate Detection menu modes:**
  - Saturn (320/352/640);
  - "N64 Auto Deblur" (detects the N64 interpolation and reconstructs 320 wide);
  - EDTV Off/On/On+VGA (forces 3432/3456 samples per line for 480p/576p, or CEA timing on RGBHV for Dreamcast);
  - LumaCode;
  - "Link Scaler/Vert." (one vertical setting shared by all decimation factors) ([RT4K-EXP] v1.31, v1.42, v1.86, v1.87.4).
- **Native Sampling** (Video ADC menu): the ADC rate tracks the scaling mode so no horizontal scaling is needed ([RT4K-EXP] v1.44).
- **Auto gain** works on RGB, YPbPr and VGA. **Real-time clipping indicators** appear while ADC offset/gain or decoder contrast/brightness are adjusted: red at maximum, blue at minimum ([RT4K-EXP] v1.9.9.2).
- **Decoder (SDP):** 3D comb, Enhanced S-Video (auto luma/chroma alignment) and NTSC-50 ([RT4K-EXP]).

### 2.3 Scaling and geometry
- **Scaling modes:** 4:3, 16:9, 1:1 PAR, Proportional, Free Form, Auto Fill, Auto Fill Int, Auto Fill Int+, Auto Fill Int HV, Auto Fill Int HV+ ([WP-SHEET], [RT4K-EXP] v1.23, v1.44).
  - "Int+" rounds the vertical factor up, i.e. overscans (like the 5X's 1080p Over).
  - "Int HV" also integer-rounds the horizontal factor from the PAR.
- **Vertical Pan** slides an overscanned integer image within the overscan instead of trimming top and bottom separately. Its readout shows the factor ("6x") ([RT4K-EXP] v1.27, v1.28).
- **Crops** are per decimation factor. AUX4 resets crops. Auto-crop for HDMI uses AUX1 (vertical only), AUX2 (crop and stretch to 4:3) and AUX3 (crop and stretch to 16:9) ([RT4K-EXP] v1.0.5, v1.44).
- **Blank Res. Change** blanks during hard resolution changes. It defaulted to on, then changed to off because it interfered with "wonky sources like Atari or VHS" ([RT4K-EXP] v1.2.7, v1.3.1).
- **Smooth I/P Switch** (Deinterlacer menu): 240p↔480i and 288p↔576i become seamless. "240p/288p will now report as 480i/576i, just like on a PVM", and they share one profile slot ([RT4K-EXP] v1.9.1).

### 2.4 Output timing and refresh
- **Sync Lock list (current order):** Adaptive Triple Buffer (default), Gen Lock, Frame Lock, TBC, Triple Buffer, VRR ([RT4K-EXP] v1.28, v1.94).
  - **Adaptive Triple Buffer** switches between true CEA-861 50 Hz and 60 Hz variants rather than just re-clocking, because some TVs reject 60 Hz timings clocked at 50 Hz.
  - **TBC** is Gen Lock with extra input-rate filtering and slower convergence.
  - **Gen Lock** regenerates the clock to follow the source. Its capture range is +19 %, enough for 70 Hz DOS on a 60 Hz modeline.
- **True VRR** arrived in v1.92 (Sep 2026) for RT4K Pro, RT4K CE and RT6X CE.
  - HDMI "VRR Format" is Off/FreeSync/VESA; Sync Lock "VRR" engages it.
  - If the display lacks VRR, the unit falls back to Adaptive Triple Buffer and **"the Sync Lock line shows why"**.
  - ALLM is auto-enabled when the display reports it ([RT4K-EXP] v1.92–v1.98).
- **Latency (CE, TimeSleuth, 240p60):** Genlock ~2.05 ms; Triple Buffer 1.6–17.5 ms, rolling ([WP-SHEET]).

### 2.5 Deinterlacing
- Methods: Motion Adaptive (multi-field motion, edge-adaptive interpolation), CRT Simulation, Weave, Bob, Blend, Linear, LCD Blending.
- Inverse 3:2/2:2 telecine; the 2nd-generation engine on the Pro "does not need manually tuned thresholds" ([WP-SHEET], [RT4K-EXP] v1.96).

### 2.6 Image processing
- **Pipeline:** 12-bit linear-light RGB; HDR10/HLG; expanded gamut that keeps "illegal" composite colours; CSC files and dithering ([RT4K-EXP]).
- **Scanlines:** standard, "Energy Conservation" (brightness-preserving), "Normalize Energy" and pseudo-interlaced.
- **Masks:** BMP files on SD, with "Multiply" or "Spatial Redistribution" algorithms and alpha keying.
- **Other effects:** LCD effects (RGB/BGR subpixel); XBR smoothing.
- **BFI:** Variable MPRT and CRT beam simulation. The BFI menu "displays the exact BFI pattern".
- **Horizontal Blur filter family (Pro):** RC Lowpass, RC Peaking, Gaussian, Butterworth, Elliptic and three notch types aimed at specific dither frequencies (e.g. Genesis 3.35 MHz).
- **Source bundles:** [RT4K-EXP], [RT4K-REL].

### 2.7 Profiles
- **Storage:** profiles are `.rt4` files on SD. They are saved with an **on-screen keyboard** and land in the current folder ([RT4K-EXP] v1.59).
- **Folder notes:** a folder may hold an `_info.txt` "folder note", which is rendered under the file list ([RT4K-EXP] v1.31, v1.37).
- **Loading:** profiles are assignable to remote buttons 1–12; a power-on profile can be set ([RT-4KCE-SHOP], [RT4K-BLOG]).
- **Auto-load:**
  - **DV1:** MiSTer core name → `/profile/DV1/<core>.rt4` ([RT4K-EXP] v1.4.7).
  - **SVS:** switch input n → first file matching `/profile/SVS/S<n>_*.rt4`. It is driven by `SVS NEW INPUT=<n>` over serial, with `SVS CURRENT INPUT=<n>` as a keep-alive every ~1–2 s. "Other devices may also use this system" ([RT4K-EXP] v1.6.7).
- **"Keep Cur. Input" and "Keep Cur. Res" (Off / On / On + Auto Scale)** are global toggles in Load Profile ([RT4K-EXP] v1.24).
  - "On + Auto Scale" translates a profile's scaling to the current output resolution.
  - Neither applies to SVS or DV1 auto-loads.
- **Starter packs and Simple Mode loading:** "Browse Starter Packs" reads `/profile/_starter`. In Simple Mode, a profile *not saved on this unit* applies picture settings only; once saved on the unit it also restores input and resolution ([RT4K-EXP] v1.27).
- **Bundled content:** profiles for 65 systems and ~145 CRT/LCD looks ([WP-SHEET]).

### 2.8 Menu tree outline [RECONSTRUCTED]

```
Main menu (Regular mode)
├─ Input ............ HDMI | HD-15 (RGBHV/RGBS/RGsB/YPbPr/SV/CV) | SCART | RCA | Front CV/SV
├─ Output ........... 4K60, 1440p, 1080p, 480p, 120 Hz modes, custom modeline slots 1-4 (3/4 from /modelines)
├─ Scaling/Crop ..... scaling mode, Vertical Pan, H/V interpolation, crops, Blank Res. Change
├─ Image Proc. ...... scanlines (std / Energy Conservation / Normalize Energy / Pseudo Interlace),
│                     masks, LCD, XBR, Horizontal Blur (Pro), RoTATE (Pro)
├─ Deinterlacer ..... method, Inverse 3:2/2:2, Smooth I/P Switch
├─ Color Correction . input/output gamma, input gain, CSC, dithering, expanded gamut, HDR PQ
├─ BFI .............. modes (incl. Variable MPRT, CRT beam), beam gain/steepness, pattern view
├─ Video ADC ........ gain/offset (+ auto gain), sample rate, phase, LPF, Wide Tolerance, Native Sampling
├─ Sample Rate Detection .. auto decimation, Saturn / N64 Auto Deblur / EDTV / LumaCode, Link Scaler/Vert.
├─ SDP .............. decoder settings, 3D comb, Enhanced S-Video, standard (NTSC-50 …)
├─ HDMI Input/RX .... auto-crop, DV1 decimation (Infoframe/Measure), Target Decimate, A/DAC (Analogue)
├─ HDMI Output ...... VRR Format, HDR, Limited Range, deep color
├─ Sync Lock/Buffering .. Adaptive Triple Buffer | Gen Lock | Frame Lock | TBC | Triple Buffer | VRR
├─ Audio ............ source, ADC rate, VU meter, ASRC (Pro)
├─ Profiles ......... Load (Keep Cur. Input / Keep Cur. Res), Save, power-on, buttons, auto-load DV1/SVS,
│                     Browse Starter Packs
└─ Advanced Settings
   ├─ OSD/Firmware .. Check SD Card (update), auto-sleep, sleep on cold boot, Switch to Simple Mode
   └─ Status ........ multi-page status screens
Simple Mode (one page): Input · Output Resolution · Profiles (Starter Packs / Browse All) ·
                         basic picture controls · tip line for highlighted item; AUX8 = full menu
```

- The menu names are each cited in [RT4K-EXP]/[RT4K-REL]/[RRGB-SIMPLE] ("Image Proc. Menu", "Deinterlacer Menu", "Video ADC menu", "Sample Rate Detection", "HDMI Output menu", "Buffering menu", "OSD/Firmware", "Load Profile menu", "SDP menu", "A/DAC menu", "HDMI RX menu", "Color Correction menu", "BFI menu", "Audio menu", "status screens").
- Grouping and order are **[RECONSTRUCTED]**.

### 2.9 Remote, OSD, firmware, serial, diagnostics
- **Remote keys**, from the serial "remote" command names used by the Donut Dongle project ([SVIRANT-DD]):
  - power, menu, back, ok, the four arrows;
  - input, output, scaler, sfx, adc, prof, prof1–prof12;
  - gain (auto gain), phase (auto phase), pause, safe;
  - genlock, buffer;
  - res4k, res1080p, res1440p, res480p, res1–res4 (custom modelines);
  - aux1–aux8, diag, stat, aud, col.
  - Custom IR code sets load from `ir_remote.txt` ([RT4K-EXP] v1.28).
- **OSD extras:**
  - a banner BMP on SD (8-bit palette);
  - the menu renders even when the output is 240p/480i ("temporal downscaling");
  - file-browser scroll arrows, and the browser remembers its sub-directory and cursor;
  - scaler-mode help tips ([RT4K-EXP], [RT4K-REL]).
- **Auto-sleep** when there is no signal, with a warning that a button press cancels. These are global settings, not per profile ([RT4K-EXP] v1.6.0–1.6.2).
- **Firmware update:** unzip to SD (`rt4kup.bin`), then either "Advanced Settings > OSD/Firmware > Check SD Card" or a power-on with Reset held.
  - The LED reports progress (pink, then blue) and errors (red blink).
  - Profiles, CSCs, banners, masks and modelines survive because they live on SD.
  - There are Release and Experimental channels on the website ([RT4K-REL]).
- **Serial:** USB and HD-15 at 2,000,000 baud by default, with remote emulation, SVS commands, an ACK/CRC16 transfer mode and the input-measurement struct ([RT4K-EXP] v1.6.6, v1.75, v1.80).
- **No native web UI or Wi-Fi.** File management is by SD card on a PC ([WP-SHEET]). A community member was building Wi-Fi via the SD slot **[REPORTED]**.

### 2.10 Praise and complaints
- **Praise:** "well organized, intuitive and logically structured" (usability 8.7); firmware maturity 8.8 with "very few bugs"; nearly every remote key mapped ([WP-SHEET]).
- **Complaint:** "Cropping logic takes a bit [of] time to understand between the different scale modes" ([WP-SHEET]).
- The Simple Mode beta exists because the full menu became dense. The developer asked for reports of "anything confusing, especially the on-screen tips and the way profiles load and save" ([RT4K-EXP] v1.27).

**Notable UX ideas:**
- Simple/Regular toggle on one key.
- Tip line for the highlighted item.
- Keep-Current-Input/Res when loading a profile.
- SVS serial auto-load.
- The Sync Lock line explains a fallback.
- Clipping tint during gain adjustment.
- BFI pattern view.
- Folder notes.
- Auto-sleep with cancellable warning.

---

## 3. RetroTINK-5X Pro

- **I/O:** SCART (RGB, CV), YPbPr RCA (CV on green; S-Video on green/blue since 3.69), S-Video, stereo audio; HDMI out; micro-USB power ([CM-RT5X], [CM-RT5X-MAN]).
- **Input range:** 240p–1080i; 720p and 1080i are sampled 4:2:2 ([CM-RT5X-MAN]).
- **Price:** US$325 ([TE-5X]).
- **Defaults:**
  - CEA 1080p "Fill", polyphase Bilinear Sharp, Triple Buffer.
  - Frame Lock auto-engages for 480p/576p/720p/1080i ([CM-RT5X-MAN]).
- **Output Res.:**
  - 1080p (Fill) is non-integer.
  - 1080p (Over) is 5× for 240p and crops top and bottom "like a CRT".
  - 1080p (Under) is 4× with black bars.
  - Also 1200p, 720p and 480p.
  - "Advanced Res" (hidden by default) adds 1920x1440, 2560x1440, 1536p (iPad), 240p/288p and 480i/576i ([CM-RT5X]).
- **H. Sampling presets ("optimal timings"):**
  - Generic 4:3, Generic 16:9;
  - SNES/PCE 256, Gen/Sat 320, PS1 320, N64 320, NEOGEO 320, Saturn 352, PS1 384, HiRes 512, HiRes 858;
  - GBI Optimal (360p only), DTV-A/DTV-B (480p only), DTV 858 (3:2), DTV 858 (4x).
  - Presets are filtered by an input/output matrix.
  - Phase Detect: Auto, or 1 of 5 … 5 of 5. Lock Speed: Slow/Medium/Fast.
  - "Not able to automatically switch its H. Sampling Preset when the resolution changes" ([CM-RT5X]).
- **V-Sync (HDMI menu):** Triple Buffer (default), Frame Lock, Gen Lock (units sold after July 2021), TBC (3.63).
  - "Lock to 60 Hz" and "Lock to 50 Hz".
  - VRR FreeSync flag; Inject HDR (HLG/HDR10); Blue Only; AVINFO AR ([CM-RT5X], [RT5X-EXP]).
- **Latency:** ~0.25 frame in Frame Lock and 0.25–1.25 frames in Triple Buffer ([CM-RT5X-MAN]). Later firmware states 1–3 ms "global low lag" ([CM-RT5X]).
- **Deinterlacing:** Motion Adaptive, Weave, Blend, Bob, Smooth-Bob, plus MA Smoother.
  - Bob Offset (−4…+4) and Bob Scanline (Post-Deint./Pre-Deint./CRT Simulate).
  - Field Order; Pre-scaler (1/2, 1/3 line drop); DS Pass-thru; Frame Blend.
  - Inverse telecine 3:2 and 2:2 ([CM-RT5X]).
- **Post-processing:**
  - Presets: Slot Mask, A. Grille-1, BVM, PVM-600, FV310, Consumer-1/2, LCD.
  - Each preset can be tweaked with "+Scanline Beam" (Exponential/Gaussian/S. Gaussian/LCD), "+Scanline Str.", "+Intensity Mod.", "+Mask", "+Gamma Boost", "+Color Boost" and "+Pre-emph" ([CM-RT5X]).
- **Video ADC:**
  - SDTV LPF (Off/Light/Medium/Strong), EDTV LPF;
  - SoG Thresh (20–300 mV, default 120 mV);
  - LLPLL Gain/BW, Pre-/Post-Coast;
  - ADC Phase in 32 steps of 11.25°.
  - **ADC-2:** Y/Pb/Pr gain and offset, Clamp Length/Current. Clipping is highlighted live (min blue, max purple), calibration is stored per resolution, and the wiki gives a step-by-step 240p Test Suite procedure ([CM-RT5X]).
- **Profiles:**
  - 10 slots, named with an on-screen keyboard (fw 2.98).
  - Startup Profile in the OSD menu.
  - "Default" exists only in Load (so it can't be overwritten).
  - **Back ×6** loads Default; **SAFE held 1 s** loads the default 1080p profile ([CM-RT5X]).
  - Firmware updates erase profiles ([RT5X-EXP]).
- **Main menu:**
  - [Input] [Output Res.] [Scaling/Crop] [Interp./Deint.] [H. Sampling] [Post. Proc.]
  - [SDTV Decoder] [Video ADC] [HDMI] [OSD] [Save Profile] [Load Profile] [Status] [About]
  - Video ADC-2 is reachable via AUX1 ([CM-RT5X]).
- **OSD design:**
  - Top-left menu; size Tiny/Small/Normal; selectable Blanking Color.
  - Magenta crop bars.
  - A "4:3 ruler": a 1.00 readout when V size matches the preset's PAR.
  - Held arrow keys auto-repeat ([CM-RT5X]).
- **Status page:** Source, Output Res, Input Lines, Input H-Freq, Input V-Freq, Samples/Line, ADC Clock, Buffer Lag (ms). An uptime counter was added in 3.63 ("Days without a green screen") ([CM-RT5X], [RT5X-EXP]).
- **Remote (premium):** direct keys for every input, every output resolution, each menu, profiles 1–10, SAVE/LOAD, STAT, ABOUT, PHASE ±, SAFE, SYNC-LOCK, SYNC-BUF and AUX1–4 (AUX4 = pause) ([CM-RT5X]).
- **Firmware:**
  - The official path is a Windows FTDI tool, started by holding Menu while plugging in USB (LED red).
  - Community alternatives: the Python "tinkup" and a browser-based flasher.
  - "It is not possible that a genuine RetroTINK device can be 'bricked' by a failed firmware update" ([CM-RT5X-MAN], [CM-RT5X]).
- **v4.0 "CRT Simulation Edition" (experimental, May 2026):**
  - Ports the RT4K linear-light pipeline and HDR10/HLG.
  - "Quasi-4K" 3840x960 / 2880x1200 / 2880x1080 modes.
  - v4.01 gives 50 Hz and 60 Hz sources independent output-resolution selection, and makes HDR "Auto" from the sink EDID ([RT5X-EXP], [TE-5XV4]).
- **Praise:** "blissfully easy to use"; "you can just plug it in or tweak settings a lot" ([TE-5X]).
- **Complaints:** profile loss on firmware update; H. Sampling preset not switching with resolution; HDR "Inject" needs manual Gamma/Color Boost tweaks ([CM-RT5X]).

**Notable UX ideas:**
- Safe-key recovery and Back ×6 to defaults.
- "Default" not overwritable.
- Hidden Advanced Res.
- Per-resolution ADC calibration with clip tint.
- Status page with Buffer Lag.
- The "1.00" aspect ruler.
- Presets built as base plus "+" overrides.

---

## 4. OSSC 1.x (Open Source Scan Converter)

- **I/O:**
  - AV1 SCART (RGBS/RGsB/YPbPr; sync from CV, luma or CSYNC, 75 Ω).
  - AV2 RCA (YPbPr/RGsB).
  - AV3 VGA (RGBHV/RGBS/RGsB/YPbPr, TTL sync).
  - HDMI out (DVI on early boards), with SCART audio to a 3.5 mm jack and HDMI audio on v1.6+.
  - No analog video out ([JHQ-OSSC]).
- **Architecture:** a line multiplier, not a frame-buffer scaler.
  - Latency "less than 2 input scanlines"; fast 240p↔480i recovery.
  - All processing in RGB ([JHQ-OSSC]).
- **Detection:** "Autodetect input": Off / Current input / All inputs. Right on the remote restarts the scan ([JHQ-OSSC]).
- **Menu tree [VERIFIED]** ([JHQ-OSSC]):

```
Video in proc ... Video LPF (Auto/Off/95/35/16/9 MHz), Reverse LPF, YPbPr color space (601/709/Auto),
                  R/G/B offset & gain, Pre-ADC gain, Clamp/ALC offset, ALC V/H filter, Lumacode (+palette)
Sync opt. ....... Analog sync LPF, Analog STC LPF, Analog sync Vth (0-350 mV, def 124), Hsync tolerance,
                  Vsync threshold, H-PLL Pre-/Post-Coast, ADC PLL BW, FPGA PLL BW
Line mult opt. .. 240p/288p proc (Passthru/Line2x…Line6x), 384p/400p proc, 480i/576i proc (Passthru/
                  Line2x bob/Line3x laced/Line4x bob), 480p/576p proc, 960i/1080i proc, Passthru mode,
                  Line2x…Line6x mode (Generic 4:3/16:9, 512/384/320/256x240 optim.), Line5x format
                  (1920x1080/1600x1200/1920x1200), 256x240 aspect (Pseudo 4:3 DAR/1:1 PAR),
                  480p in sampler (Auto/DTV 480p/VESA 640x480/PSP), 400p in sampler, Allow upsample2x,
                  Advanced timing tweaker (H. samplerate, H. s.rate frac, sync/backporch/active lengths,
                  H/V border, Sampling phase 0-347°)
Output opt. ..... TX mode (HDMI RGB/YCbCr444/DVI), HDMI ITC, HDMI Aspect flag, HDR flag, VRR flag,
                  480p/576p pillarbox
Scanline opt. ... Scanlines (Off/Auto/…), Sl. strength, Sl. hybrid str, Sl. method, Sl. alternating,
                  Sl. alignment, Scanline type, Custom Sl.
Post-proc. ...... (masks), Shadow mask, Custom shadow mask, Sh. mask strength, Border color,
                  Mask brightness, DIY latency tester
Compatibility ... Full TX setup, Allow TVP HPLL2x, AV3 use alt. RGB, Full VSYNC bypass,
                  Default HDMI VIC, Panasonic hack, Disable PR2x
Audio options ... Down-sampling, Swap L/R, Mono, Pre-ADC gain
Settings opt. ... Link prof->input, Link input->prof, Initial input (incl. last used), Autodetect input,
                  Auto AV1/2/3 Y/Gs, LCD BL timeout, OSD (Off/Full/Simple), OSD status display
                  (2/5/10 s/Off), OSD cursor color, Phase hotkey, Load/Save profile, SD Load/Save profile,
                  Reset profile, Fw. update, Launch 2nd FW
```

- **Optimal timings:** "512/384/320/256x240 optim." modes sample at the console's dot clock and pixel-repeat the output. They are marked "For advanced users only — requires clock and phase adjustments".
  - The wiki keeps a per-console table: dots per line, OSSC base mode, H.samplerate (e.g. Mega Drive 320 → 427.50) and PC modelines ([JHQ-OSSC], [JHQ-OPT]).
  - Line3x–Line6x outputs are not CEA/VESA, so "generally accepted only by monitors" ([JHQ-OSSC]).
- **Profiles:**
  - Internal slots; serial `prof <0…14>`. The menu documents 0–9, with the P.LOAD hotkey reaching 10+.
  - SD profiles since v1.20.
  - Names come from `prof_n_i.txt` / `prof_n.txt` (13 characters).
  - The last loaded slot loads at startup ([JHQ-OSSC]).
- **OSD and front display:**
  - 16×2 character LCD plus an on-screen OSD (Full/Simple/Off).
  - A **status OSD on input mode change** for 2/5/10 s.
  - INFO shows the profile and preset, then lines per frame, p/i, special processing and cycles per frame ("divide 27000000 by it to get Hz") ([JHQ-OSSC]).
- **Remote:** 0–9 select input and format. Hotkeys for scanline mode, type and strength, LINEMULT_MODE, SAMP_PHASE±, SHMASK±, PROFILE_LOAD. Phase keys are off by default because they conflict with Yamaha AVR volume codes ([JHQ-OSSC]).
- **Serial (JTAG UART):** `input <1…9>`, `prof <0…14>`, `rc <0|1>` ([JHQ-OSSC]).
- **Firmware:** a `.bin` in `fw/` on a FAT32/exFAT SD card, then Settings → Fw. update. Numbered error codes (−1 SD not detected … −9 CRC). A second firmware can be stored and launched (e.g. the VoidScaler downscaler) ([JHQ-OSSC], [JHQ-VOID]).
- **Diagnostics:** a built-in **display latency tester** using a photodiode and a white-box pattern ([JHQ-OSSC-LAT]).
- **Complaints:**
  - Not plug-and-play: optimal modes need phase and samplerate tweaking, and Line3x+ fail on many TVs.
  - Lacks auto gain ([WP-SHEET] for the Pro; same front-end class).
  - Hobbyist case ([TE-5X] comparison).

**Notable UX ideas:**
- The status OSD on mode change.
- "Divide 27 MHz by cycles" transparency.
- Link input↔profile.
- Last-used input.
- Second-firmware slot.
- Numbered update error codes.
- Console optimal-timing table as data, not as code.

---

## 5. OSSC Pro

- **I/O:**
  - AV1 SCART, AV2 5×RCA, AV3 HD-15 plus 3.5 mm audio, AV4 HDMI, EXP (Legacy AV card for CV/SV), TOSLINK.
  - HDMI out up to 2560×1440@60 (2880×2160 with 2× pixel repetition).
  - The **Extra AV out** expansion gives analog outputs ([JHQ-OSSCPRO]).
- **Operating modes:**

  | | Pure LM | Adaptive LM | Scaler (framelock) | Scaler (no framelock) |
  |---|---|---|---|---|
  | Latency | max 2 lines | max 60 lines | ~1 frame | 1–2 frames |
  | Display compatibility | Variable | Very high | Very high | Full |
  | Deinterlacing | Bob, NI restore | Bob, NI restore | Bob, Weave, MA | Bob, Weave, MA |

  ([JHQ-OSSCPRO])
- **Scaler opt.:**
  - **DFP output mode:** 720x480/576 (+WS), 1280x720 (24–240 Hz), 1280x1024, 1080i, 1080p (24–120 Hz), 1600x1200, 1920x1200, 1920x1440, 2560x1440, 2880x2160.
  - **CRT output mode:** 240p/288p/480i/576i (+WS), 384p, 480p, 540p, 1024x768(i), 1280x960, 2048x1536.
  - **Framelock:** On, Off (source Hz), Off (preset Hz), Off (50/60/100/120 Hz). **Framelock multiplier** 1–10×.
  - **Aspect ratio:** Auto, 4:3, 16:9, 8:7, 1:1 source PAR, Full.
  - **Scaling algorithm:** Auto, Integer (underscan), Integer (overscan), Nearest, Lanczos3, Lanczos3_sharp, Lanczos3&3_sharp, Lanczos4, GS sharp/medium/soft, Custom. Plus Edge threshold.
  - **Deinterlace:** Bob/Weave/Motion adaptive, with Motion shift.
  - **Generic samplerate:** Auto/Lowest/Highest.
  - Per-mode sampling presets; Adv. timing with Clock & Phase, Size/crop, Position, Reset preset ([JHQ-OSSCPRO]).
- **Post-proc.:** border colour and brightness (to avoid OLED wear); shadow masks A-Grille, TV, PVM, PVM-2530, XC-3315C, C-1084, JVC, VGA, and Custom (MiSTer mask format); BFI for duplicated frames and BFI strength ([JHQ-OSSCPRO]).
- **Settings:**
  - Power-up state, Initial input, OSD Off/Full/Simple, OSD status display, cursor colour.
  - Fan PWM, LED PWM, Chardisp contrast/fade out.
  - Disable/Customize/**Bind IR remote** (learn any NEC remote).
  - Load/Save profile ("Slot 0 is loaded automatically at power-up"), SD profiles, Fw. update ([JHQ-OSSCPRO]).
- **Signal information** page: input/output timings, firmware, uptime. A **test pattern mode** cycles every output preset to test display compatibility ([JHQ-OSSCPRO]).
- **Front panel:** button, 4-way mini-joystick and character OLED. LEDs: green = on, red flashing = resync, **blue = framelock** ([JHQ-OSSCPRO]).
- **Remote:** dedicated P-LM/A-LM/SCL mode keys, SCL.RES, SCL.FL, SCL.S.ALG, SCL.D.ALG, SCL.AR, SCL.ZOOM, SCL.PAN, P.LOAD and LM.DIL ([JHQ-OSSCPRO]).
- **Serial:** `input`, `prof`, `rc`, `poweroff`, `poweron` ([JHQ-OSSCPRO]).
- **Measured** (240p60): Pure LM 0.06–0.10 ms, Adaptive LM 0.84 ms, Scaler framelock ~18 ms, Triple Buffer 18–34 ms ([WP-SHEET]).
- **Shootout scores:**
  - Usability **6.5**: "logically structured … but takes more time to understand". The three modes "could be overwhelming for casual users".
  - No auto gain.
  - Auto-load only at power-up.
  - Input source not stored per profile.
  - Profiles renamed via a config file ([WP-SHEET]).
  - Price ~US$430 with Legacy AV ([WP-SHEET]).

**Notable UX ideas:**
- The blue "framelock" LED.
- A test-pattern mode for display compatibility.
- IR learning ("Bind IR remote").
- Output modes listed with their supported refresh ranges.
- Integer under/overscan named explicitly.
- Border brightness for OLED wear.

---

## 6. PixelFX Morph 4K (and Morph 2K)

### 6.1 Morph 4K
- **I/O:**
  - Base unit: HDMI in (≤1080p60), HDMI out 4K60 4:4:4, USB-A serial, microSD, USB-C power.
  - **Analog Bridge** modules: SCART+Component, VGA+Component, VGA+VGA.
  - A Crosspoint card with analog in and out is announced ([PFX-M4K], [VGP-MORPH]).
  - **ESP32 Wi-Fi** (no Bluetooth) runs the WebUI.
  - Price ~US$375 with the bridge ([WP-SHEET]).
- **Main menu [VERIFIED]** ([PFX-M4K]):

```
Presets ....... Browse presets, Save as/Delete auto preset, Create new preset, Save to preset,
                Reload auto preset, Restore previous settings, Restore default settings,
                Install/update from presets archive (over Wi-Fi, from GitHub)
Input ......... Port A | Port B | HDMI; analog type: CVBS green, CVBS hsync, S-Video, YPbPr,
                RGB TTL, RGB 75, RGsB
                Main: Horiz. Samples, Horiz. Phase, DE left/width/top/height, Horiz. Prescale,
                      Low pass filter (Auto), Audio select
                Color: Gain (Pre-ADC, R/G/B), Offset R/G/B, ALC V-Filt-Coeff, SOG LPF + Threshold,
                       SS Threshold, HPLL Pre/Post-Coast, Line Length Tol., Clamp adjust
                HDMI: Input CS, 422 Interp., Metadata adjust, DV1/FXD, EDID
Video
 ├ Scaler ......... Zoom, Aspect Ratio (Auto + DAR/PAR list), Horiz./Vert. Interpolation
 │                  (off = nearest, softer…sharper), Horiz./Vert. Prescale (/1…/32)
 ├ Shift / Crop ... Input/Output H/V shift, Crop L/R/T/B
 ├ Deinterlacer ... Field Order, Method (Motion Adaptive default, Bob, Weave, CRT sim.),
 │                  Sensitivity, Interpolator, Detector (Zero-lag / Symmetric), Noise threshold
 ├ Smoothing ...... XBR, HQX
 ├ Retro FX ....... H/V Scanlines (Off/On/On (alt)/Adaptive; saturation, strength, thickness/glow),
 │                  Mask (type, intensity, transform)
 ├ Color Correction HDR, Color Profile, Input Gamma, Color Space Conversion, Output Gamma/PQ
 ├ Output Resolution 1080p, WQHD, 4K, 1080p (120Hz), WQHD (120Hz) + custom modelines (WebUI)
 └ Advanced Video . Frame Lock (Off (triple buffered) / Normal / Minimal (lowest lag)),
                    Triple Buffer (Auto (50/60Hz) / Force 60Hz / Force 50Hz), BFI, Color Space,
                    AVI IF ITC/CN, Inject VFR-EMP, RX DV1/FXD, RX Input CS, RX 422 Interp., RX EDID
System ........ WiFi (Setup Wizard, Station Settings), Firmware (Release Notes, Update Channel,
                Check for Update, Update Firmware, Update from SD, Select Alternative Firmware),
                OSD (Font, Endless menu, Endless FB, CEC, Version Check, Notifications, Screensaver,
                Menu H/V-Align, Margin, Padding), Morph Settings (LED RGB, Menu RGB, Preset auto-load,
                Overscan marker, Write debug log, Signal modifier, Fan), SD Card Info, Game ID, EDID,
                Debug / Self Test, Snake, Factory Reset, Info
Restart ....... Enter Standby Mode, Restart Morph
RX Input Analyzer .. RX Status, Input Timings, Audio, AVI/SPD/VS InfoFrames
TX Info ........ TX Output, TX SCDC Status
CEC remote
```

- **Auto-load cascade** (fw ≥ 3.9.46) ([PFX-AUTOLOAD]):
  - The context is resolved most-specific first: Game-ID → SPD InfoFrame (FXD / DV1 / Vendor) → CEC OSD name → input source (HDMI/DVI/analog). The first existing file wins.
  - Each context also tries `CONTEXT@WIDTHxHEIGHTp.ini` and `CONTEXT@HEIGHTp.ini` before `CONTEXT.ini`, with an optional `/presets/Auto/default.ini`.
  - Example: `/sdcard/presets/Auto/By_Gameid/PS1/…@320x240p.ini`.
  - This is the best-documented auto-profile design on the market.
- **UX conventions** ([PFX-M4K]):
  - Values are grey by default, **yellow when changed but not confirmed** and white once saved; Menu/Back without OK discards.
  - Yellow/blue keys step ×10; a dedicated "Reset to Default" key.
  - Output-resolution **auto-revert after 30 s**.
  - Emergency 1080p reset by holding the front Right button for 5 s.
  - Screensaver; notification timeout; OSD font and placement; LED and banner colour; a Snake game.
- **Wi-Fi onboarding:**
  - System → WiFi → Start Setup Wizard shows a **QR code**. The phone joins the "MORPH 4K" AP and opens `http://192.168.4.1/wiz`.
  - The wizard then picks the SSID, takes the password and sets WebUI credentials.
  - The WiFi page shows SSID, channel/signal quality, mDNS hostname (`morph4k.local`), IP and MAC. A coloured key reveals the web credentials ([PFX-M4K]).
- **WebUI:** profile file manager (create, rename, upload, download, delete), mask designer, custom modelines, IR remote programming, **web remote**, firmware update ([PFX-M4K], [WP-SHEET], [VGP-MORPH]).
- **API:** a WebSocket at `ws://morph4k.local/ws/api` with JSON SD-card commands (`sdcard={"a":"list","v":["/sdcard/presets"]}`), plus REST endpoints such as Game ID ([PFX-API]).
- **Firmware:**
  - OTA over Wi-Fi with Stable and other channels and release notes in the OSD; an SD card is required since 3.9.x.
  - SD-card flash by holding the circle button.
  - A **Rescue System** via Wi-Fi AP or SD ([PFX-M4K]).
- **Diagnostics:**
  - Debug / Self Test: clock source CLKIN/XTAL, PLL_A/B and HDMI PLL lock, monitor sense, hot-plug (❤ when healthy), **lag in ms**, input CS, core temperature, IR address and command.
  - RX analyzer and TX info pages; a `debug.txt` log on SD ([PFX-M4K]).
- **Measured** (240p60): Genlock 1.56 ms; Triple Buffer 17–33 ms ([WP-SHEET]).
- **Complaints:**
  - Firmware maturity 7.6, "several bugs", factory reset sometimes needed.
  - **Input selection "consist[s] of several nested menus"**.
  - **Input source and output resolution can't be pre-configured per profile.**
  - Crop adjustments sluggish; selecting a mask is slow.
  - Few remote keys mapped, so profiles can't go on buttons.
  - Wi-Fi rename takes 4–5 s, and there is no bulk delete ([WP-SHEET]).

### 6.2 Morph 2K (2026)
- Analog-only: composite, S-Video, SCART RGBS, component, and VGA over SCART. A **TW9912 + TVP7002** front end ([PFX-M2K]).
- **Outputs:** 480p, 720p, 1080p, 1440p 4:3.
- **Sync modes:** "ultra low lag, fully buffered, and genlock".
- **Processing:** MAD; scanlines and masks; HDR injection, VRR; Wi-Fi WebUI; CEC; IR; profiles cross-compatible with Morph 4K ([PFX-M2K]).
- **Resolution change UX:** OK confirms, a second OK saves, and it reverts after 20 s; "!" marks the current mode ([PFX-M2K]).
- **Price and timing:** US$199.99; announced April 2026, pre-orders June 2026 ([TE-M2K-NEWS], search snippets) **[REPORTED]**.

**Notable UX ideas:**
- Specificity cascade with resolution sub-contexts.
- Grey/yellow/white value states.
- ×10 coarse step and a reset-to-default key.
- QR Wi-Fi wizard.
- mDNS name shown on device.
- Web credentials shown on device.
- Update channel with release notes.
- Self-test page with health ticks.

---

## 7. Micomsoft XRGB-mini Framemeister (legacy reference)

- **Top menu:** Output Mode, HDMI Output, DVI Output, Image Mode, Aspect Ratio, Screen Set, Color Set, Visual Set, Zoom Set, Sync Set, HDMI Set, Video Set, Audio Set, Options, Special, Data, Status. The full hierarchy is in Micomsoft's OSD PDF ([JHQ-FM]).
- **Image Mode** bundles deinterlacing and LPF into named presets: STANDARD, GAME1, GAME2, MEISTER, ANIME, MOVIE, NATURAL, PICTURE.
  - Users must learn that "the low-pass filter incurs a *severe* loss of detail".
  - The community recommends PICTURE for progressive and NATURAL for interlaced sources ([JHQ-FM]).
- **Visual Set → Auto Scaler:** OFF/GAME/VIDEO maps to H_SCALER/V_SCALER (GAME 6/6, VIDEO 6/5). The wrong one gives "highly inaccurate vertical scaling" ([JHQ-FM]).
- **Sync Set → Sync Mode:** OFF (standard 60) / AUTO (follow input). A known bug leaves sync lock partially on ([JHQ-FM]).
- **HDMI Set:** DeepColor, Output Color, Output Range. FULL is reportedly ignored on 2.04 ([JHQ-FM]).
- **Lag** around 20 ms up to fw 1.10 ([JHQ-FM]).
- **Firmware:** an SDSC card (no SDXC) with the folder in the root; power on to flash. Any version can be up- or downgraded ([JHQ-FM]).
- **Known issues that define what users hate:**
  - very slow interlace↔progressive switching ("blank screen for too long");
  - unstable 480p output after 240p→480i until H_POS is touched;
  - scanlines wrong at 1080p;
  - **adjusting H/V width blanks the screen**;
  - aspect-ratio controls reversed ([JHQ-FM]).
- A user comment sums up the Framemeister's reputation: "profile / settings stuff on Framemeister too fiddly. OSSC was nice and simple. Tink 5X is best of both" ([TE-5X] comments) **[REPORTED]**.

**Lessons:**
- Named "picture modes" that secretly bundle a lossy filter are an anti-pattern.
- Adjustments must not blank the picture.
- Mode switches must be fast.

---

## 8. Extron-class scalers (concepts only)

Professional AV scalers (e.g. Extron DSC 301 HD, DSC 3G-3G A) solve the same problem for non-gamers. The concepts below are worth borrowing; product details are not:

- **Auto-Image™:** one-touch automatic sizing and centring, and on PC inputs typically phase as well, for the current input ([EXT-3G]).
- **Auto Memory:** automatically stores size, position and picture settings **per detected input signal**, and recalls them when that signal returns ([EXT-3G]). This is the conceptual ancestor of "input-signature profiles".
- **Input presets vs user presets:** per-input memories are separate from user-saved presets ([EXT-3G], [EXT-301]).
- **Executive mode:** front-panel lockout, enabled only by SIS command or software ([EXT-301]). It is useful for a "kiosk/kid" lock.
- **SIS (Simple Instruction Set):** short, documented, human-typable ASCII commands over RS-232/USB/Ethernet, with a response for every command ([EXT-301]). Extron's SIS is also what the community's Extron switch → RT4K bridges speak ([SVIRANT]).
- **Other common concepts:**
  - EDID management ("EDID Minder" is an Extron trademark term);
  - output-rate selection with "match input" options;
  - built-in test patterns for display set-up;
  - video mute and freeze;
  - an auto-switch input priority.
  - These are **[UNVERIFIED]** for the two specific models cited; they are general Extron product-line features.

---

## 9. Other notable 2024–2026 scalers

| Product | Date / price | Positioning | Notes relevant to us |
|---|---|---|---|
| **RetroTINK-4K CE** | 2025; US$475 | Lower-cost RT4K | Same menu and profiles as Pro; HDMI 1.4 in; usability leader in the 2026 shootout ([RT-4KCE-SHOP], [WP-SHEET]) |
| **RetroTINK-6X CE** | Aug 2026; US$230 | Analog-only, 2560x1440p60 (300 MHz), 1080p120, 3840x1080 "quasi-4K" | Inputs: VGA (also SV/CV and **SVS serial**), RCA, SCART, front SV/CV; **no HDMI in, no USB serial**; auto phase "with no test pattern needed"; Smooth I/P; no IVTC; **imports RT4K profiles and auto-resizes them**, and 6X profiles load on 4K units; ships a Wobbling Pixels profile pack; a community web profile editor ("PIPe") supports it ([RT-6X-BLOG]) |
| **PixelFX Morph 2K** | Apr–Jun 2026; US$199.99 | Analog-only 1080p/1440p 4:3, TW9912 + TVP7002 | Wi-Fi WebUI, CEC, profiles shared with Morph 4K ([PFX-M2K], [TE-M2K-NEWS]) |
| **RetroTINK-5X "4.0"** | May–Jun 2026 (experimental) | Legacy unit gets RT4K pipeline, HDR10, quasi-4K | Shows the vendor back-porting UX (independent 50/60 Hz output selection, HDR Auto from EDID) ([RT5X-EXP]) |
| **Retail GBS-C variants** (RetroScaler, Mcbazel/ODV "GBS-C") | ongoing | gbs-control pre-installed, Wi-Fi config | Listicles describe them as "mid-range … WiFi config, adaptive deinterlacing" **[REPORTED]**; retail price not verified |

**Market reading.** In 2026 two vendors launched **US$200–230 analog-only** scalers (Morph 2K, RT6X CE). Both have Wi-Fi or SVS automation, auto phase, profiles shared with their 4K siblings, and starter profile packs.
- A ~US$50–100 GBS-C cannot compete on picture processing. (That price range is **[UNVERIFIED]**.)
- It can compete on price, VGA/RGBS output for CRTs, openness and a web UI, if the out-of-box experience is as good.

Independent cross-check: the Wobbling Pixels shootout (RT4K CE vs Morph 4K + Analog Bridge vs OSSC Pro + Legacy AV). It is already summarised in [market-shootout-2026.md](market-shootout-2026.md). Key rows are reused in section 12 ([WP-SHEET], [RRGB-SHOOT]).

---

## 10. GBS-Control firmwares (the incumbent on our hardware)

### 10.1 Mainline ramapcsx2/gbs-control
- **Claims:** "very low lag"; "no synchronization loss switching 240p/480i (output runs independent from input, sync to display never drops)"; an on-demand motion-adaptive deinterlacer; auto gain and auto offset; component→RGBHV bypass; NTSC/PAL/EDTV/HD and VGA from 192p to 1600x1200 ([GBSC-README]).
- **Web UI tree** [VERIFIED from `public/src/index.html.tpl`] ([GBSC-REPO]):

```
Resolution .... 1920x1080 | 1280x1024 | 1280x960 | 1280x720 | 480p/576p | 15KHz (downscale) | Pass Through
               ("Your selection will also be used for startup")
Presets ....... 72 named slots: select slot, load preset, save preset
ADC Gain ...... gain −/+, Auto Gain ("Calibrate for a few seconds on a white screen")
Picture Control  move / scale / borders (arrow pads)
Filters ....... scanlines, intensity, line filter, peaking, step response (each with a one-line hint)
Settings ...... Matched Presets, Full Height, Low Res: Use Upscaling, Output RGBHV/Component,
               Force PAL 50Hz to 60Hz, Disable External Clock Generator, ADC calibration,
               Active FrameTime Lock + Switch Lock Method, Deinterlace Method (Motion Adaptive/Bob),
               Developer Mode, Save Filtering Per Slot
Developer ..... Toggle Console, MEM/HS Left/Right, List Options, Print Info, Get Video Timings,
               Freeze Capture, ADC Filter, Cycle SDRAM, Debug View, HTotal++/--, Resync HTotal,
               PLL divider++, Invert Sync, SyncWatcher, SyncProcessor, Oversampling, 60/50Hz HDMI,
               IF Auto Offset, SOG Level--, Reset Chip
System ........ Enable OTA, Restart, Reset Defaults, Backup/Restore, Wi-Fi (Access Point / Station)
```

- **Tab icons:** input, control_camera, blur_on, tune, developer_mode, bolt. They are icon-only; text appears inside each tab.
- **OLED menu** ([GBSC-REPO] `OLEDMenuImplementation.cpp`, [GBSC-WIKI-OLED]):
  - OSD → TV5725 icon/bar overlay with a timeout.
  - Resolution → list + Downscale + Passthrough.
  - Presets → slots.
  - WiFi → SSID/IP/URL info.
  - Current Settings → info page.
  - Reset/Restore → Reset GBS / Restore Factory / Reset WiFi.
  - The wiki is explicit that the OLED "is not meant to replace the Web UI".
- **TV OSD:** `OSDManager` drives the chip's icon+bar OSD for Brightness, Contrast, Auto Gain, Scanlines, Line Filter, Move X/Y and Scale X/Y ([GBSC-REPO] `OSDManager.h`).
- **Serial:** about 60 single-character commands (`d`, `+`, `-`, `e`, `D`, …) ([GBSC-REPO] `gbs-control.ino`). They are powerful but undiscoverable.
- **Profiles:** 72 named slots; filter state per slot or global; backup and restore ([GBSC-REPO] public/README).

### 10.2 Notable forks
- **GBSC-Pro firmware** (Karabanov Aleksandr, 2023–2024; local copy) ([GBSC-PRO]):
  - Adds an **IR remote** and a **text TV OSD through an external STV9426 OSD chip**, plus PT2257 volume control.
  - An S-Video/AV input path, with a decoder implied by menu items "SV/AV input: Double line, Smooth, Bright, Contrast, Saturation, Compatibility".
  - Profile names.
  - OSD menu: Input (RGBs/RGsB/VGA/YPbPr/SV/AV), Resolution (1080/1024/960/720/pass), Screen, Color and System settings.
  - **Implication:** a text OSD on TV needed extra hardware. Public repo URL **[UNVERIFIED]**.
- **Raspberry-Pi lineage** (dooklink, paranoidnela forks): a USB-keyboard-driven OSD (e.g. F1 toggles) **[REPORTED]** ([GH-PARANOID]).
- **The project owner's own fork** (local branch of JuergenLeber/gbs-control; docs dated to 2026-09) is a bench-measured feasibility record against the RT4K menu ([FORK-PARITY], [FORK-TAXONOMY]):
  - **240p↔480i:** 0 blank frames in 6,600, with output vtotal fixed at 1111. This is the equivalent of "Smooth I/P".
  - **Vertical-total ceiling 1200:** clean at 1200, and the board resets at 1250, so 1920x1200 CVT-RB (vtotal 1235) is refused.
  - **Pseudo-interlace:** not achievable (the scanline effect doesn't alternate).
  - **Softening filters** (`IF_HS_SEL_LPF`, `VDS_D_RAM_BYPS`): not worth shipping.
  - **Temporal noise reducer:** "wrecks" the picture.
  - **Chip user-defined mode slot** `MD_USER_DEF_*`: works, so custom input modes are possible.
  - **Buffer-phase latency instrument** via the test bus: sub-frame only.
  - **OSD:** "no font/glyph/framebuffer registers exist" (exhaustive search).
  - **IR:** possible on GPIO2 at the cost of the status LED on a D1-mini build.
  - The fork also removed its profile system twice and records this as "a real gap".

### 10.3 Community complaints (gbs-control issue tracker, most-discussed)
These are the main themes, with example issues ([GBSC-ISSUES]).

| Theme | Example issues | Lesson |
|---|---|---|
| No picture or black screen after setup | #62 "No picture" (67 comments), #461 random black screen in passthrough, #411 | First-run must be foolproof; boot to a known-safe mode; show the reason on the OLED |
| Web UI unresponsive or confusing | #509 "There's a ton of people with the same issues, and nobody seems to want to help" (GUI unresponsive, resolutions inaccessible); #126 "Proposal for improved Web GUI" (responsive, icons and tooltips, "buttons show their intended state") | Robust, stateful, responsive UI; visible connection state |
| Sync problems with specific sources | #216 Neo Geo, #118 MVS, #190 Atari ST, #101 EGA 21.7 kHz, #341 PS2 RGsB no signal, #447 losing sync on white flashes | Expose sync tolerance, SOG threshold and coast settings with names; per-source presets |
| Tearing / frame-lock behaviour | #286 tear line "even with clock generator installed", #555, #470 | Make the lock state visible; explain the lock method; auto-recover |
| 50/60 Hz handling | #237 "Force PAL 50hz to 60hz doesn't do anything" | Every option must visibly do something, or say why it can't |
| Missing output modes | #178 no 1024x768/800x600/640x480 in the menu, #388 secondary 1080p widescreen | Users expect a resolution list with PC modes and an aspect choice |
| Colour and levels via the HDMI converter | #516 washed-out blacks at 960p and limited/full confusion | The HDMI path is a second digitizer (MS9288A); document it and offer level presets |
| Profiles | #105 "Import/Export presets and separate gain control for RGB" | Per-channel gain; portable profiles |
| Hardware variance | #47 GBS8200 mods, #296 "GBS v5.1 unable to use GUI", #439 clockgen install | Detect the board and clock generator; report it in Status |

**Notable UX ideas from gbs-control:**
- Picking a resolution also sets the startup preset.
- "Matched Presets" automatically chooses 960p for NTSC and 1024p for PAL.
- One-line explanations under every filter.
- Backup/restore.
- A developer mode hidden by default.

---

## 11. MiSTer video options (naming and UX reference)

From `MiSTer.ini` ([MISTER-INI]):
- `vscale_mode` 0 fit to height, 1 integer only, 2 0.5 steps, 3 0.25 steps, 4 integer with the core's aspect, 5 integer keeping the display aspect.
- `vscale_border` sets a border for overscanning TVs.
- `vsync_adjust` 0 off, 1 match the original VSync by adjusting the pixel clock ("butter smooth"), 2 low-latency single buffer. The ini notes that "not every display supports variable pixel clock" and advises "use 60Hz HDMI video modes as a base even for 50Hz systems".
- `vrr_mode` 0 none, 1 auto from EDID, 2 FreeSync, 3 VESA, 4 MiSTer VRR.
- `refresh_min` / `refresh_max` stop vsync_adjust and VRR outside the display's range.
- `video_mode`, plus `video_mode_ntsc` and `video_mode_pal` as separate bases.
- `video_info=1…10` shows video info on startup or on change, for N seconds. This is MiSTer's version of the OSSC status OSD.
- `direct_video` passes core timing over HDMI for DACs. `forced_scandoubler` and `vga_scaler` (VGA gets the scaler output). `composite_sync`, `vga_sog`, `vga_mode=rgb|ypbpr|svideo|cvbs`.
- `hdmi_limited` 0 full, 1 16–235, 2 16–255 "for VGA converters". This is relevant for our MS9288A path.
- `hdmi_game_mode`, `hdr`, `shmask_mode_default`, `preset_default`, `osd_timeout`, `video_off`.
- **Per-core `[core]` sections** in the same ini act as auto-loaded per-system profiles.
- **Core OSD names:** "Aspect ratio" (Original/Full Screen/custom), "Scale" (Normal / V-Integer / Narrower HV-Integer / Wider HV-Integer), "Scandoubler Fx" (HQ2x, CRT 25/50/75 %), "Vertical Crop". Names vary by core **[UNVERIFIED]**, from general knowledge.

**Takeaway.** The community already uses "Integer", "V-Integer", "HV-Integer", "Narrower/Wider", "vsync adjust/match", and "60 Hz base for 50 Hz content". Reuse these words.

---

## 12. Cross-product feature matrix

Legend: ✓ yes · ◐ partial or limited · ✗ no · — not applicable. The "GBS Reimagined (target)" column shows feasibility on TV5725 + ESP8266: **F** feasible, **P** partial, **NF** not feasible.

| Feature | RT4K Pro/CE | RT5X Pro | OSSC 1.x | OSSC Pro | Morph 4K | Framemeister | gbs-control | **GBS Reimagined (target)** |
|---|---|---|---|---|---|---|---|---|
| Analog in: RGBS/RGsB/YPbPr/RGBHV | ✓ | ◐ (no RGBHV) | ✓ | ✓ | ✓ (bridge) | ◐ | ✓ | **F** (SCART/RGBS/VGA/Component per board) |
| Composite / S-Video in | ✓ | ✓ | ✗ | ✓ (card) | ✓ (bridge) | ✓ | ✗ | **NF** (no decoder on GBS-C) |
| HDMI in | ✓ | ✗ | ✗ | ✓ | ✓ | ✓ | ✗ | **NF** |
| Max output | 4K60 / 1440p120 | 1440p (adv.) | 1440p (Line6x) | 1440p / 2160p rep. | 4K60 | 1080p | 1080p (presets up to 1920x1080 / 1280x1024) | **P**: ≤1080p60 on HDMI (MS9288A), ≤~162 MHz on VGA; vtotal ≤1200 measured |
| Analog video out | ✗ | ✗ | ✗ (adapter) | ✓ (card) | planned | ✗ | ✓ VGA/RGBS/YPbPr | **F**, a real differentiator (CRT/PVM users) |
| 15 kHz downscale | ◐ (480i/240p modelines) | ✓ (240p/480i out) | ✓ passthru | ✓ CRT modes | ✓ prescale | ✗ | ✓ "15KHz" | **F** |
| Auto input detect / switch | ◐ | ✗ | ✓ (Off/Current/All) | ◐ | ◐ | ◐ | ✓ | **F** |
| Auto gain | ✓ | ◐ manual + clip tint | ✗ | ✗ | ✓ | ✗ | ✓ | **F** (exists) |
| Clip indicator while adjusting | ✓ | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ | **P**: numeric/web only, no on-picture tint |
| Auto phase | ✓ continuous | ✓ | ✗ | ✓ | ✗ | ✗ | ✗ | **NF** true image-based; **P** manual sweep helper |
| Console sampling presets ("optimal") | ✓ auto | ✓ presets | ✓ optim. modes | ✓ presets | ◐ manual | ✗ | ✗ | **P**: PLLAD_MD presets, bench validation needed; HDMI path resamples |
| Manual samplerate / phase / LPF / SOG / coast | ✓ | ✓ | ✓ | ✓ | ✓ | ◐ | ◐ (dev tab) | **F** (registers exist) |
| Integer scaling | ✓ (Int/Int+/HV) | ✓ Over/Under | ✓ line mult. | ✓ under/overscan | ✓ | ✗ | ◐ (960p=4×) | **P**: 4× (960p) yes; 5× limited by vtotal/1080 |
| Aspect presets 4:3/16:9/8:7/1:1/Full | ✓ | ◐ | ◐ | ✓ | ✓ | ◐ (reversed bug) | ◐ | **F** |
| Frame lock | ✓ Frame/Gen Lock, TBC | ✓ | inherent | ✓ | ✓ Normal/Minimal | ◐ AUTO | ✓ FrameTime Lock | **F** (Si5351/vtotal); HDMI acceptance **P** |
| Triple buffer / free-run | ✓ + Adaptive 50/60 | ✓ + Lock to 60 | ✗ | ✓ (source/preset/50/60/100/120) | ✓ Auto/Force 60/50 | ✓ OFF | ✓ (independent output) | **F** |
| VRR | ✓ true VRR (2026) | ◐ flag | ◐ flag | ◐ flag | ◐ VFR-EMP flag | ✗ | ✗ | **NF** |
| HDR / BFI / 120 Hz | ✓ | ✓/◐ | ◐ flag | ✓ BFI | ✓ | ✗ | ✗ | **NF** |
| Smooth 240p↔480i | ✓ Smooth I/P | ✓ | ✓ (fast) | ✓ | ◐ (1–2 s) | ✗ slow | ✓ | **F** (0 blank frames measured) |
| Motion-adaptive deinterlace | ✓ | ✓ | ✗ (bob) | ✓ | ✓ | ✓ | ✓ | **F** |
| Inverse telecine 3:2/2:2 | ✓ | ✓ | ✗ | ✗ (NI restore) | ✗ | ◐ (PICTURE) | ✗ | **P/UNVERIFIED** (pull-down flag exists; usable IVTC unknown) |
| Scanlines | ✓ advanced | ✓ | ✓ | ✓ | ✓ | ◐ (wrong at 1080p) | ✓ basic | **F** basic; pseudo-interlace **NF** |
| CRT masks / shaders / xBR | ✓ | ✓ | ◐ masks | ✓ masks | ✓ | ✗ | ✗ | **NF** |
| Sharpness / peaking | ✓ (filter family) | ✓ pre-emph | ◐ reverse LPF | ✓ algos | ✓ interp. | ✓ | ✓ peaking/step | **F** |
| Profiles: count / storage | SD, unlimited | 10 internal | internal + SD | internal (115) + SD | SD | internal | 72 slots (flash) | **F** (LittleFS) |
| Profile stores input + output res | ✓ (+Keep toggles) | ✓ | via Link prof→input | ✗ input | ✗ | ? | ◐ | **F**, do it |
| Auto-load profiles | power-up, DV1, SVS | startup | Link input→prof | power-up | Game ID/SPD/CEC/mode cascade | ✗ | ✗ | **F**: by input signature + SVS/HTTP; DV1/CEC/Game ID **NF** |
| Remote | IR, 12 profile keys | IR (premium) | IR (L336) | IR + learning | IR, CEC, web | IR | ✗ | **P** (IR needs pin/receiver); web remote **F** |
| Web UI / Wi-Fi | ✗ (SD on PC) | ✗ | ✗ | ✗ | ✓ ESP32 | ✗ | ✓ ESP8266 | **F** (ESP8266, small heap) |
| Front display | LED | LED | 16×2 LCD | char OLED + joystick | LED + buttons | LEDs | OLED add-on | **F** (OLED + encoder) |
| On-TV text menu | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | icon/bar only | **NF** (icon+bar overlay **F**) |
| Status / info page | ✓ multi-page | ✓ incl. Buffer Lag | ✓ INFO | ✓ Signal info | ✓ RX/TX/self-test | ✓ Status | ◐ OLED "Current" | **F** |
| Test pattern for display compatibility | ? | ✗ | ✓ 480p default | ✓ all presets | ✗ | ✗ | ✗ | **P/UNVERIFIED** (needs a chip pattern source) |
| Firmware update | SD / reset-button | USB tool (erases profiles) | SD | SD | Wi-Fi OTA, SD, rescue | SD | Arduino/OTA | **F** OTA; rescue **P** |
| Serial / API | ✓ remote+SVS | ✗ | JTAG UART | JTAG UART | WebSocket/REST | ✗ | single-char console | **F** (human-readable + SVS + HTTP) |
| Measured lag (240p60) | ~2 ms Genlock | ~0.25 frame FL | <2 lines | 0.06–0.1 ms Pure LM | ~1.6 ms Genlock | ~20 ms | "very low" (unmeasured abs.) | **P**: relative phase instrument exists; absolute must be measured |

Sources: sections 2–11.

---

## 13. Terminology: community-standard option names

The table gives the term, what it means, who uses it, and the suggested GBS Reimagined label.

| Term (variants) | Meaning | Used by | Suggested label |
|---|---|---|---|
| **Frame Lock** (Framelock, FrameTime Lock) | Output refresh derived from input; judder-free; may lose sync on mode change | RT4K/5X, OSSC Pro, Morph, gbs-control | "Frame Lock" |
| **Gen Lock** | Regenerated output clock that tracks input frequency (smoothed) | RT4K/5X, Morph 2K | "Gen Lock (clock)" if Si5351-based tracking is implemented as such |
| **TBC** | Gen Lock plus input-rate cleanup and slow convergence | RT4K/5X | (optional) |
| **Triple Buffer** (Off (triple buffered), source Hz, free-run) | Independent output clock; frames repeated or dropped; max compatibility | all | "Triple Buffer" |
| **Adaptive Triple Buffer** / Auto (50/60Hz) | Free-run at 50 or 60 Hz to match the source family | RT4K, Morph | "Auto 50/60" |
| **Lock to 60 Hz** / Force 60Hz / Force PAL 50Hz to 60Hz | 50 Hz content output at 60 Hz | RT5X, Morph, gbs-control | "Force 60 Hz" |
| **VRR** (FreeSync, VESA; injection vs true) | Variable refresh signalling | RT4K, MiSTer | n/a (not feasible) |
| **Line2x … Line6x** / line multiplier / Passthru | Scan-line multiplication without a frame buffer | OSSC | "Line ×2/×3/×4" only if a true LM mode is offered; else avoid |
| **Pure LM / Adaptive LM / Scaler** | OSSC Pro operating modes | OSSC Pro | n/a (we are a scaler) |
| **Optimal timing / optimal sampling / optim. mode** | Sampling at the console's native dot clock | OSSC, RT5X, Wobbling | "Optimal sampling (per console)" |
| **Generic 4:3 / Generic 16:9** | Non-native sampling scaled to aspect | RT5X, OSSC | "Generic 4:3/16:9" |
| **H. samplerate / Samples per line / ADC Samp./Line / Horiz. Samples** | Total samples per input line (PLL divider) | OSSC, RT5X, Morph | "Samples per line" (show "H. samplerate" as alias) |
| **Sampling phase / ADC Phase / Horiz. Phase** | Clock phase of the samples (degrees or steps) | all | "Sampling phase" in degrees (32 × 11.25°) |
| **Decimation / Prescale / Input decimation** | Dividing oversampled pixels down to source pixels | RT4K, Morph | n/a (no oversampling) |
| **Video LPF / SDTV LPF / EDTV LPF / Low pass filter** | Pre-ADC anti-alias filter | OSSC, RT5X, Morph | "Video LPF" |
| **SOG threshold / Analog sync Vth / SS Threshold** | Sync slicer level | RT5X, OSSC, Morph | "Sync threshold (SOG)" |
| **H-PLL Pre-/Post-Coast** | PLL hold around vsync | OSSC, RT5X, Morph | "PLL coast (pre/post)" |
| **Hsync tolerance / Line Length Tol. / Wide Tolerance** | Accept irregular line lengths | OSSC, Morph, RT4K | "Sync tolerance" + "Wide tolerance" toggle |
| **Clamp / Clamp/ALC offset / Clamp Length** | Black-level clamp window | OSSC, RT5X, Morph | "Clamp position / width" |
| **Pre-ADC gain, Gain, Offset** | Analog levels per channel | all | "Gain / Offset (R G B)" + "Auto Gain" |
| **Auto Gain / Auto Phase / Auto Decimation** | Automatic level, phase and dot-clock detection | RT4K | "Auto Gain" (only what we can do) |
| **Smooth I/P (Switch)** | Seamless 240p↔480i | RT4K, RT5X | "Smooth I/P" |
| **Blank Res. Change** | Black frames during hard mode changes | RT4K | "Blank on mode change" |
| **Fill / Over / Under** (1080p Fill/Over/Under) | Non-integer fill; integer overscan; integer underscan | RT5X | "1080p Fill / Over / Under" |
| **Auto Fill, Int, Int+, Int HV** | RT4K scaling family | RT4K | Avoid the RT4K-specific names |
| **Integer (underscan / overscan)**, V-Integer, HV-Integer | Integer scale preference | OSSC Pro, MiSTer | "Integer (fit) / Integer (overscan)" |
| **1:1 PAR, 8:7, 4:3 DAR, Full** | Aspect choices | OSSC Pro, Morph, RT4K | same |
| **Zoom / Pan / Crop / Vertical Pan / Border** | Geometry | all | "Size", "Position", "Crop", "Border" |
| **Motion Adaptive, Bob, Weave, Blend, Linear, Smooth-Bob, CRT Simulate** | Deinterlacers | RT4K/5X, Morph, OSSC Pro | "Motion Adaptive", "Bob" (+ others only if real) |
| **Inverse telecine 3:2 / 2:2, NI restore** | Cadence recovery; non-interlace restore | RT4K/5X, OSSC Pro | only if implemented |
| **Scanlines** (strength, hybrid, alignment, alternating, beam) | Line darkening | all | "Scanlines" + "Strength" |
| **Masks** (Aperture grille, Slot, Shadow) | Sub-pixel CRT masks | RT4K/5X, OSSC, Morph | n/a |
| **Peaking / Sharpness / Step response / Pre-emph** | Edge enhancement | gbs-control, RT5X | "Sharpness" (peaking) + "Edge sharpness" (step) |
| **Full / Limited range** (Colorspace, Output Range, hdmi_limited) | RGB quantisation | all | "HDMI levels: Full / Limited" (MS9288A-dependent) |
| **ITC / content type flag** | AVI InfoFrame hints | OSSC, Morph | n/a (no InfoFrame control) [UNVERIFIED for MS9288A] |
| **Profile / Preset / Slot / Memory** | Saved settings | all | "Profile" (user) vs "Preset" (built-in) |
| **Startup profile / Power-on profile / Initial input / Last used** | Boot behaviour | RT4K/5X, OSSC | same |
| **Link input→profile, Auto-load, SVS, DV1, Game ID** | Automatic profile selection | OSSC, RT4K, Morph | "Auto-load by input"; "SVS" for compatibility |
| **Status / Info / Signal information / RX Input Analyzer** | Live signal data | all | "Status" |
| **Safe mode / Emergency reset / Default profile** | Recovery | RT5X, Morph | "Safe mode" |
| **Test pattern** | Built-in output for display testing | OSSC Pro | "Test pattern" (if feasible) |

---

## 14. Hardware feasibility basis (TV5725 + ESP8266 + Si5351 + MS9288A)

**From the brief:**
- TV5725: triple 8-bit 160 MHz ADC; sync processor with SOG slicer; motion-adaptive and bob deinterlacer using SDRAM; up/down scaler; analog RGB DAC (~162 MHz).
- Output pixel clock from the Si5351.
- HDMI is analog RGB re-digitised by the MS9288A (≤165 MHz, ~1080p60 ceiling, ≤1080 input lines).
- No HDMI input, no 4K, no HDR. Audio is passthrough only.

**Additional facts and caveats** (detail and sources in [market-hardware-notes.md](market-hardware-notes.md)):

1. **OSD discrepancy.** The brief says "a simple character OSD generator". The documented TV5725 OSD registers expose an **8-icon ROM (Brightness, Contrast, Hue, Sound, Up/Down, Left/Right, V-size, H-size), one bar, 3-bit colours, zoom and position**. There are no font or text registers ([GBSC-TV5725H]), and the owner's fork found none in an exhaustive search ([FORK-TAXONOMY]). Treat **on-TV text menus as NOT FEASIBLE** unless the full Tvia datasheet shows otherwise **[UNVERIFIED]**. The GBSC-Pro fork added an external STV9426 OSD chip to get text, which supports this reading.
2. **Sampling controls exist as registers:**
   - `PLLAD_MD`: 12-bit samples per line.
   - `PA_ADC_S`: 32 phase steps.
   - `ADC_FLTR`: LPF.
   - `ADC_SOGCTRL`: 5-bit SOG threshold.
   - Per-channel gain and offset; clamp window; pre/post coast; sync-pulse thresholds.
   - Status registers report H/V totals, polarities, SOG state, PLL lock and a pull-down detect flag.
   - All of this is enough for OSSC-style manual sampling and a rich status page ([GBSC-TV5725H]).
3. **No frame access for the MCU.** The ESP8266 cannot read pixels, so RT4K-style auto-phase and auto-decimation from image analysis are not feasible. (A test-bus-based phase metric is **[UNVERIFIED]**.)
4. **Vertical total ceiling 1200** (measured; 1250 resets the chip) ([FORK-PARITY]). So 1200-line outputs (1920x1200 CVT-RB at vtotal 1235, 1600x1200, or a full 240p ×5 = 1200 active lines) are out of reach at 60 Hz. A 1080p "Over" mode (×5 zoom cropped to 1080 visible lines, vtotal 1125) remains possible in principle but is unvalidated. 960p (×4 integer) and 1080p Fill are the practical HDMI modes.
5. **HDMI is a second digitiser.** The MS9288A has its own ADC phase, mode table and "enhancement" processing, and is limited to standard timings and 165 MHz. Off-spec frame-locked refresh rates may not be accepted. Whether its I²C is wired to the ESP8266 is **[UNVERIFIED]** ([MS9288A], via 70-hardware.md).
6. **ESP8266 budget.** Under ~50 kB heap in station mode; the owner's fork runs at ~16 kB free with a 15 kB floor ([FORK-PARITY]). I²C is bit-banged. The web UI must be a small gzip bundle with a compact API; keep one live client.
7. **Si5351** synthesises arbitrary pixel clocks, which enables clock-based frame lock and exact refresh rates.

---

## 15. Ideas worth adopting for GBS Reimagined

Ratings:
- **FEASIBLE** — software only on the stated hardware.
- **PARTIAL** — feasible with stated limits or extra validation.
- **NOT FEASIBLE** — hardware ceiling.

Each line gives the idea, then where it comes from, then the rating and reason. **★** marks the highest-value items.

### A. Out-of-box and onboarding
1. **★ "Just play" defaults.** Auto-detect input; output 1280x960 for 60 Hz and 1280x1024 for 50 Hz (gbs-control "Matched Presets"), or 1080p Fill. Frame lock off until verified; auto gain on first signal. (RT5X quick start, gbs-control) — **FEASIBLE.**
2. **★ First-run wizard on OLED + web.** Choices: display type (HDMI TV / VGA monitor / 15 kHz CRT / PVM), region (60/50 Hz), which outputs are connected. The wizard sets safe defaults per choice, and the CRT choice enforces 15 kHz-only output. (Morph setup flow, RT5X beginner guide) — **FEASIBLE.**
3. **★ Two tiers everywhere.** "Essential" (input, output, profile, picture basics) vs "Advanced" (sampling, sync, developer). Applies to the web UI, the OLED tree and serial `help`. (RT4K Simple Mode, OSSC Simple OSD, RT5X Advanced Res, gbs-control Developer Mode) — **FEASIBLE.**
4. **One-line tip for the highlighted item:** an OLED bottom line and web tooltips/hints. (RT4K Simple Mode tips, gbs-control filter hints) — **FEASIBLE**, but the OLED fits ~21 chars per line, so tips must be terse or scroll.
5. **Hide modes the current display or path can't take.** Filter the output list by connected path; HDMI is limited to MS9288A-safe CEA/VESA modes. (RT5X preset matrix, OSSC Pro refresh ranges) — **FEASIBLE.**

### B. Safety and recovery
6. **★ Auto-revert of output mode after N seconds unless confirmed.** Show a countdown on the OLED and the web. (Morph 30 s / 2K 20 s; fork's 20 s revert) — **FEASIBLE.**
7. **★ Safe mode.** Hold the encoder at power-on to boot 640x480@60 VGA-safe or 480p with defaults. A long-press combination restores the default profile. Profiles are never overwritten by this. (RT5X SAFE / Back ×6, Morph hold-Right 5 s) — **FEASIBLE.**
8. **★ OLED as the lifeline.** Status and menu must work with no TV picture. Show "No signal", "Out of range?" and the current output mode, and offer one-click "back to safe". (OSSC/OSSC Pro display praised in [WP-SHEET]) — **FEASIBLE.**
9. **Firmware update preserves profiles and settings.** Keep LittleFS separate from the sketch, run a schema-versioned migration, and back up before updating. (RT4K/OSSC/Morph vs the RT5X complaint) — **FEASIBLE.**
10. **Rescue path.** A minimal recovery firmware or serial bootloader route, documented. (Morph Rescue System, OSSC 2nd FW) — **PARTIAL.** ESP8266 OTA needs free flash for the new image, and a true dual-image rescue depends on flash size (4 MB typical **[UNVERIFIED]** on GBS-C).

### C. Profiles
11. **★ Profiles save everything**, including input port, sync type and output mode. Add global "Keep current input" and "Keep current output" toggles applied when loading manually. (RT4K v1.24; complaints about Morph and OSSC Pro) — **FEASIBLE.**
12. **★ Auto-load by input signature, most-specific first.** Order: `port+sync+lines+p/i+Hz` → `port+lines+p/i` → `port` → `default`. Built from TV5725 status registers (H/V totals, polarity, SOG, interlace). (Morph cascade, OSSC Link input→prof, Extron Auto Memory) — **FEASIBLE.** It must debounce mode changes (240p↔480i should *not* reload when Smooth I/P is on, as RT4K does).
13. **★ SVS-compatible triggers.** Accept `SVS NEW INPUT=<n>` and `SVS CURRENT INPUT=<n>` and `remote prof<n>` on the serial console, and the same over HTTP (`/api/profile/load?slot=`). Existing Donut Dongle / Extron / gscart bridges then work unchanged. (RT4K v1.6.7, [SVIRANT-DD]) — **FEASIBLE.** Check UART0 sharing with the console and boot messages.
14. **Named profiles with web rename and an optional OLED character picker;** one-line notes per profile (RT4K `_info.txt`). — **FEASIBLE.**
15. **Startup profile:** specific, last used, or auto-by-input. (RT5X Startup Profile, OSSC Initial input / last used) — **FEASIBLE.**
16. **Quick profile access:** encoder long-press opens the profile list; the web UI offers 12 pinned "buttons" (the RT4K 1–12 idea). (RT4K, OSSC P.LOAD) — **FEASIBLE.**
17. **Built-in "starter" presets per console family.** Contents: sampling (samples per line, phase), geometry and deinterlacer; read-only and copy-to-edit. (RT4K starter packs, OSSC optimal-timings table, Wobbling packs) — **PARTIAL.** The data is small. Value depends on how well TV5725 sampling and the MS9288A re-sampling preserve sharpness; validate per console.
18. **JSON export/import and full backup/restore** (gbs-control already has backup/restore; issue #105) — **FEASIBLE.**
19. **DV1 / SPD InfoFrame / CEC / Game ID auto-load** (RT4K, Morph) — **NOT FEASIBLE.** No HDMI input, no CEC. External automation can still call the HTTP API (idea 13).

### D. Sampling and sync (OSSC-grade control, RT-grade labels)
20. **★ Named sampling controls:**
    - "Samples per line" (`PLLAD_MD`), "Sampling phase" (32 steps shown in degrees).
    - "Video LPF" (`ADC_FLTR`), "Sync threshold (SOG)".
    - "PLL coast pre/post", "Clamp position/width", R/G/B gain and offset.
    - "Sync tolerance" plus a "Wide tolerance" toggle.
    - Sources: OSSC, RT5X Video ADC, Morph Input. **FEASIBLE** (registers exist).
21. **Phase sweep helper.** Step the phase with live "stable / unstable" sync feedback and let the user pick by eye. Tap-to-save. (OSSC phase hotkeys, RT5X Phase Detect manual steps) — **FEASIBLE.** True auto-phase from image analysis (RT4K) is **NOT FEASIBLE** (no pixel access).
22. **Auto Gain with a visible result:** show the per-channel gain chosen and a "clipping detected" flag. (RT4K/RT5X) — **PARTIAL.** Auto gain exists; an on-picture red/blue clip tint is **NOT FEASIBLE** (no overlay); numeric or web indicators are fine.
23. **Calibration aids:** "Blue only" (zero R/G gain temporarily) for SDTV-style colour calibration (RT5X Blue Only) — **PARTIAL** (an approximation via ADC gains; verify there are no side effects).
24. **Custom input modes** using the chip's `MD_USER_DEF_*` slot for odd sources such as EGA 21.7 kHz or arcade (fork measured; issues #101, #190) — **FEASIBLE.**
25. **480p sampler choice:** DTV 858 vs VGA 800 samples per line, Auto by hsync width. (OSSC "480p in sampler") — **FEASIBLE.**

### E. Scaling and output
26. **★ Honest output list with market names:**
    - 480p/576p, 720p, **960p (240p ×4 integer)**, 1024p (PAL), 1080p **Fill**.
    - 1080p **Over** (240p ×5 zoom with crop) only if the scaler can do it within vtotal ≤1200.
    - 15 kHz (240p/288p), Pass-through.
    - VGA-only extras such as 1600x1200 only if validated.
    - Sources: RT5X Fill/Over/Under, gbs-control presets. **PARTIAL:** "Over" and >1080-line modes are constrained by the vtotal ceiling and the MS9288A's 1080-line limit.
27. **Aspect presets 4:3 / 16:9 / 8:7 / 1:1 PAR / Full, plus Size/Position/Crop with Vertical Pan** for overscanned integer modes (OSSC Pro, RT4K) — **FEASIBLE.**
28. **Adjustments must not blank the picture** (the Framemeister anti-pattern). Keep geometry edits live and journal them for undo. — **FEASIBLE** for scale/position (gbs-control does it live). Some timing changes need a resync; label them.
29. **Overscan border / blanking colour** (OSSC H/V border, RT5X Blanking Color, OSSC Pro border brightness) — **PARTIAL/UNVERIFIED.** A border via blanking timing is feasible. A border *colour* register was not found (the fork showed `VDS_UV_BLK_VAL` "is not it").
30. **Test-pattern mode** that cycles output modes to test the display (OSSC Pro) — **PARTIAL/UNVERIFIED.** The output-mode cycling is feasible; a chip-generated pattern source is unconfirmed (a blank field with the OSD bar may be the only "pattern").

### F. Timing and refresh
31. **★ Sync modes named as the market does.** Offer "Frame Lock" (Si5351 tracks input; FrameTime Lock fallback), "Triple Buffer" (free-run), "Auto 50/60" (free-run in the source's family) and "Force 60 Hz". Default to Triple Buffer/Auto for compatibility. (RT4K, Morph, RT5X, MiSTer vsync_adjust) — **FEASIBLE.** HDMI acceptance of off-spec rates through the MS9288A is **PARTIAL** and must be tested; VGA is the reference path.
32. **★ Lock status that explains itself.** A "Sync Lock: Frame Lock — not active: Si5351 not found / source out of range / HDMI path refused" line on OLED, web and Status. (RT4K VRR fallback reason; gbs-control issues #237, #286) — **FEASIBLE.**
33. **★ Smooth I/P, advertised by name,** with an optional "Blank on mode change". Already measured at 0 blank frames. (RT4K/RT5X) — **FEASIBLE.**
34. **Latency transparency.** A "Buffer lag" readout from the test-bus phase instrument, labelled as sub-frame, plus published absolute measurements in Wobbling's lag classes (< 1 ms "zero", 1–10 ms "extreme low", …). (RT5X Status Buffer Lag, Morph Debug lag) — **PARTIAL** (relative now; absolute needs a camera/TimeSleuth measurement).
35. **VRR, HDR, BFI, 120 Hz** — **NOT FEASIBLE** (MS9288A ≤165 MHz, no InfoFrame control, fixed-function chip).

### G. Deinterlacing
36. **Motion Adaptive / Bob choice with auto-engage on interlaced input;** scanlines are allowed with Bob (gbs-control) — **FEASIBLE.**
37. **Weave / Blend / field-order invert / Bob offset** (RT5X, Morph) — **PARTIAL/UNVERIFIED.** Needs per-mode bench proof (the fork found "Field Inversion is not `VDS_FIELD_FLIP`").
38. **Inverse 2:2/3:2 for PS2-style 480i** (RT4K/RT5X) — **PARTIAL/UNVERIFIED.** The chip reports a pull-down flag (`STATUS_DEINT_PULLDN`); whether a usable weave-on-cadence exists is unknown.

### H. Image processing
39. **Scanlines (240p, 480i Bob) with strength** — **FEASIBLE** (exists). **Pseudo-interlace, masks, shaders, xBR/HQX, LCD grids** — **NOT FEASIBLE** (measured or no block).
40. **Sharpness (peaking) and edge sharpness (step response); brightness/contrast/saturation/hue via VDS Y/U/V gains and offsets; black-level expansion** — **FEASIBLE.**
41. **Gamma control** — **NOT FEASIBLE/UNVERIFIED** (no LUT registers known). **Noise reduction and "soft" filters** — **NOT FEASIBLE** (the fork measured them as damaging or imperceptible).
42. **HDMI level preset (Full/Limited)** to fix washed-out blacks (issue #516; MiSTer `hdmi_limited=2` "for VGA converters") — **PARTIAL.** Depends on MS9288A control over I²C **[UNVERIFIED]**. Fallback: a TV5725 output-level preset with a clear note.

### I. Interfaces: web, OLED, OSD, serial, remote
43. **★ Wi-Fi onboarding like Morph.** The OLED shows a **QR code** to join the setup AP and open the wizard at `192.168.4.1`. It then shows the mDNS name (`gbs.local`), IP and RSSI. (Morph 4K) — **FEASIBLE.** A version-3 QR (29×29 modules) fits 64 px at 2 px/module.
44. **★ Web UI: responsive, stateful and honest.**
    - Show a connection/state banner.
    - Buttons reflect real state (gbs-control #126/#509).
    - Use a "Now" panel with plain-language sentences (see [gbs-control-lessons-platform.md](gbs-control-lessons-platform.md)).
    - Show pending vs saved values as grey/yellow/white (Morph).
    - **FEASIBLE** within the heap budget (static gzip bundle plus compact JSON).
45. **Web virtual remote and file manager** for profiles: rename, upload, download, delete, bulk actions (Morph WebUI; Morph complaint about no bulk delete) — **FEASIBLE** (LittleFS; keep files small).
46. **OLED interaction conventions:**
    - rotate = move/adjust;
    - press = enter/confirm;
    - long-press = back/cancel or reset-to-default;
    - press-and-turn = ×10 coarse step (Morph ×10);
    - wrap-around lists optional ("Endless menu");
    - remember cursor per menu (RT4K file browser).
    - **FEASIBLE.**
47. **TV OSD as a monitor-style quick-adjust:** icon plus bar for brightness, contrast, position, size and scanline strength, with OLED text alongside (gbs-control `OSDManager`) — **FEASIBLE.** A **full text menu on TV** (RT4K/OSSC/Morph) — **NOT FEASIBLE** without extra hardware (GBSC-Pro needed an STV9426).
48. **★ Status page** (OLED page plus web): input port and sync type, lines, p/i, H-freq, V-freq, samples per line, ADC phase, PLL lock, SOG state, output mode and pixel clock, lock mode and state, buffer phase, Si5351 present, uptime, heap, RSSI. (RT5X Status, OSSC INFO, Morph RX/TX/Self-test) — **FEASIBLE.**
49. **Status flash on mode change** (OSSC status OSD 2/5/10 s; MiSTer `video_info`) — **PARTIAL.** OLED and web toast are feasible; an on-TV text flash is **NOT FEASIBLE**.
50. **★ Human-readable serial console.** `help`, `status`, `input 2`, `output 960p`, `profile load 3`, `remote ok`, `SVS …`, with the legacy single-char commands kept as aliases and a JSON mode for tools. (RT4K, OSSC, Extron SIS) — **FEASIBLE.**
51. **IR remote with learning** ("Bind IR remote", OSSC Pro) — **PARTIAL.** Needs a TSOP receiver and a free interrupt-capable GPIO. On a D1-mini build only GPIO2 is free, at the cost of the LED. RetroScaler GBS-C pin availability is **[UNVERIFIED]**.
52. **CEC control** (Morph) — **NOT FEASIBLE.**
53. **OTA update from the web UI with release notes shown before install;** optional "check for update" against a static manifest (Morph channels) — **FEASIBLE** for local upload OTA. **PARTIAL** for an online check (HTTPS/TLS is heavy on the ESP8266; an HTTP manifest plus a signature check is possible).
54. **Auto-sleep on no signal,** with a cancellable warning on the OLED and a global setting (RT4K v1.6.x) — **FEASIBLE** (power down the TV5725 output and DACs; the ESP stays awake).
55. **Diagnostics export.** Download a JSON or text "support bundle" (status, settings, journal, register snapshot). (Morph `debug.txt`; fork `/api/capture/save`) — **FEASIBLE.**

### J. Positioning statements the research supports
- Lead with **analog output for CRTs (VGA/RGBS/YPbPr), open firmware, web plus OLED control, honest status, and "Smooth I/P"**. These are things the US$200–750 competitors either lack (analog out: only the OSSC Pro with a card, or a planned Morph Crosspoint) or charge for.
- Do **not** market masks, HDR, VRR, BFI, 4K or auto-phase. Say plainly on the product page that these are out of hardware scope (the fork's taxonomy rule).

---

## 16. Source index

**RetroTINK**
- [RT4K-EXP]: https://retrotink-llc.github.io/firmware/4k-experimental.html — RT4K experimental changelog v1.0.1 → v1.98.1 (2026-10-13).
- [RT4K-REL]: https://retrotink-llc.github.io/firmware/4k.html — release changelog and update procedure.
- [RT4K-SD]: https://retrotink-llc.github.io/firmware/4k-sdcards.html — SD card images.
- [RT5X-EXP]: https://retrotink-llc.github.io/firmware/5x-experimental.html — 5X experimental changelog (to 4.02, 2026-06-01).
- [RT-4KCE-SHOP]: https://www.retrotink.com/shop/retrotink-4k-ce
- [RT4K-BLOG]: https://www.retrotink.com/blog/introducing-the-retrotink-4k
- [RT-6X-BLOG]: https://www.retrotink.com/blog/introducing-the-retrotink-6x-ce
- [RRGB-SIMPLE]: https://retrorgb.com/?p=53443 — RT4K Simple Mode post, June 2026.
- [CM-RT4K]: https://consolemods.org/wiki/AV:RetroTINK-4K — official manual host; **not retrievable** (Cloudflare).
- [CM-RT5X]: https://consolemods.org/wiki/AV:RetroTINK-5X_Pro.
- [CM-RT5X-MAN]: https://consolemods.org/wiki/AV:RetroTINK-5X_Pro/manual
- [TE-5X]: https://www.timeextension.com/reviews/retrotink-5x-pro-retrotink-4ks-cheaper-sibling-is-still-worth-a-look
- [TE-5XV4]: https://www.timeextension.com/news/2026/05/ive-never-seen-a-legacy-product-have-cutting-edge-features-like-this-retrotink-5x-gets-quasi-4k-and-hdr10-support

**Community automation**
- [SVIRANT]: https://github.com/svirant/RT4k_HD15_serial_control
- [SVIRANT-DD]: https://github.com/svirant/DonutDongle — `Donut_Dongle.ino` lists the RT4K `remote …` command set.

**OSSC**
- [JHQ-OSSC]: https://junkerhq.net/xrgb/index.php?title=OSSC.
- [JHQ-OSSCPRO]: https://junkerhq.net/xrgb/index.php?title=OSSC_Pro — local copy `osscpro.wiki`.
- [JHQ-OPT]: https://junkerhq.net/xrgb/index.php?title=Optimal_timings
- [JHQ-OSSC-LAT]: https://junkerhq.net/xrgb/index.php?title=OSSC_latency_tester
- [JHQ-VOID]: https://junkerhq.net/xrgb/index.php?title=VoidScaler

**PixelFX and Framemeister**
- [PFX-M4K]: https://wiki.pixelfx.co/index.php/Morph_4K
- [PFX-M2K]: https://wiki.pixelfx.co/index.php/Morph_2K
- [PFX-AUTOLOAD]: https://docs.pixelfx.co/Morph4K-Preset-Autoload-cascaded.html
- [PFX-API]: https://docs.pixelfx.co/API-V0.html
- [VGP-MORPH]: https://videogameperfection.com/products/morph-video-scaler/
- [TE-M2K-NEWS]: https://www.timeextension.com/news/2026/04/built-to-extract-the-best-possible-image-from-your-consoles-pixel-fx-announces-the-usd200-morph-2k-upscaler — via search result; page 403.
- [JHQ-FM]: https://junkerhq.net/xrgb/index.php?title=XRGB-mini_FRAMEMEISTER

**Extron**
- [EXT-3G]: https://media.extron.com/public/download/files/userman/68-2310-50_A_DSC_3G-3G_A_SUG.pdf — Auto Memory / Auto-Image, via search snippet.
- [EXT-301]: https://media.extron.com/public/download/files/userman/68-2338-01_D_DSC_301_HD_UG.pdf — SIS, Executive mode, via search snippet.

**Shootout**
- [WP-SHEET]: https://docs.google.com/spreadsheets/d/16FBPBD3A1vN_khznTUO1bGrb3LxTIQXfwWfusPeNSsw — Wobbling Pixels shootout; summary in [market-shootout-2026.md](market-shootout-2026.md).
- [RRGB-SHOOT]: https://retrorgb.com/?p=52163

**gbs-control**
- [GBSC-REPO]: https://github.com/ramapcsx2/gbs-control — read at HEAD 2fb1fde (2026-07-22).
- [GBSC-README]: https://github.com/ramapcsx2/gbs-control/blob/master/README.md
- [GBSC-WIKI-OLED]: https://github.com/ramapcsx2/gbs-control/wiki/OLED-Add-On-Setup-Info
- [GBSC-TV5725H]: https://github.com/ramapcsx2/gbs-control/blob/master/tv5725.h
- [GBSC-ISSUES]: https://github.com/ramapcsx2/gbs-control/issues — e.g. /62, /126, /178, /216, /237, /286, /388, /509, /516.
- [GBSC-PRO]: source read locally (headers credit Karabanov Aleksandr; public URL unverified).
- [GH-PARANOID]: https://github.com/paranoidnela/gbs-control

**Owner's local fork and MiSTer**
- [FORK-PARITY]: the reference fork's `docs/retrotink-parity.md` (bench measurements 2026-09; digested in [gbs-control-lessons-videopath.md](gbs-control-lessons-videopath.md)).
- [FORK-TAXONOMY]: the reference fork's `docs/menu-taxonomy.md`
- [MISTER-INI]: https://github.com/MiSTer-devel/Main_MiSTer/blob/master/MiSTer.ini

**Hardware**
- [MS9288A]: https://atta.szlcsc.com/upload/public/pdf/source/20250210/96A200247BE50AB9E322BCD94220CBFA.pdf — MS9288A datasheet (Chinese); see [market-hardware-notes.md](market-hardware-notes.md).

[RT4K-EXP]: https://retrotink-llc.github.io/firmware/4k-experimental.html
[RT4K-REL]: https://retrotink-llc.github.io/firmware/4k.html
[RT4K-SD]: https://retrotink-llc.github.io/firmware/4k-sdcards.html
[RT5X-EXP]: https://retrotink-llc.github.io/firmware/5x-experimental.html
[RT-4KCE-SHOP]: https://www.retrotink.com/shop/retrotink-4k-ce
[RT4K-BLOG]: https://www.retrotink.com/blog/introducing-the-retrotink-4k
[RT-6X-BLOG]: https://www.retrotink.com/blog/introducing-the-retrotink-6x-ce
[RRGB-SIMPLE]: https://retrorgb.com/?p=53443
[CM-RT4K]: https://consolemods.org/wiki/AV:RetroTINK-4K
[CM-RT5X]: https://consolemods.org/wiki/AV:RetroTINK-5X_Pro
[CM-RT5X-MAN]: https://consolemods.org/wiki/AV:RetroTINK-5X_Pro/manual
[TE-5X]: https://www.timeextension.com/reviews/retrotink-5x-pro-retrotink-4ks-cheaper-sibling-is-still-worth-a-look
[TE-5XV4]: https://www.timeextension.com/news/2026/05/ive-never-seen-a-legacy-product-have-cutting-edge-features-like-this-retrotink-5x-gets-quasi-4k-and-hdr10-support
[SVIRANT]: https://github.com/svirant/RT4k_HD15_serial_control
[SVIRANT-DD]: https://github.com/svirant/DonutDongle
[JHQ-OSSC]: https://junkerhq.net/xrgb/index.php?title=OSSC
[JHQ-OSSCPRO]: https://junkerhq.net/xrgb/index.php?title=OSSC_Pro
[JHQ-OPT]: https://junkerhq.net/xrgb/index.php?title=Optimal_timings
[JHQ-OSSC-LAT]: https://junkerhq.net/xrgb/index.php?title=OSSC_latency_tester
[JHQ-VOID]: https://junkerhq.net/xrgb/index.php?title=VoidScaler
[PFX-M4K]: https://wiki.pixelfx.co/index.php/Morph_4K
[PFX-M2K]: https://wiki.pixelfx.co/index.php/Morph_2K
[PFX-AUTOLOAD]: https://docs.pixelfx.co/Morph4K-Preset-Autoload-cascaded.html
[PFX-API]: https://docs.pixelfx.co/API-V0.html
[VGP-MORPH]: https://videogameperfection.com/products/morph-video-scaler/
[TE-M2K-NEWS]: https://www.timeextension.com/news/2026/04/built-to-extract-the-best-possible-image-from-your-consoles-pixel-fx-announces-the-usd200-morph-2k-upscaler
[JHQ-FM]: https://junkerhq.net/xrgb/index.php?title=XRGB-mini_FRAMEMEISTER
[EXT-3G]: https://media.extron.com/public/download/files/userman/68-2310-50_A_DSC_3G-3G_A_SUG.pdf
[EXT-301]: https://media.extron.com/public/download/files/userman/68-2338-01_D_DSC_301_HD_UG.pdf
[WP-SHEET]: https://docs.google.com/spreadsheets/d/16FBPBD3A1vN_khznTUO1bGrb3LxTIQXfwWfusPeNSsw
[RRGB-SHOOT]: https://retrorgb.com/?p=52163
[GBSC-REPO]: https://github.com/ramapcsx2/gbs-control
[GBSC-README]: https://github.com/ramapcsx2/gbs-control/blob/master/README.md
[GBSC-WIKI-OLED]: https://github.com/ramapcsx2/gbs-control/wiki/OLED-Add-On-Setup-Info
[GBSC-TV5725H]: https://github.com/ramapcsx2/gbs-control/blob/master/tv5725.h
[GBSC-ISSUES]: https://github.com/ramapcsx2/gbs-control/issues
[GBSC-PRO]: #102-notable-forks
[GH-PARANOID]: https://github.com/paranoidnela/gbs-control
[FORK-PARITY]: #102-notable-forks
[FORK-TAXONOMY]: #102-notable-forks
[MISTER-INI]: https://github.com/MiSTer-devel/Main_MiSTer/blob/master/MiSTer.ini
[MS9288A]: https://atta.szlcsc.com/upload/public/pdf/source/20250210/96A200247BE50AB9E322BCD94220CBFA.pdf
