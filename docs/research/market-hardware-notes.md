### 15.1 Hardware reality check (feasibility basis)

The feasibility ratings below are based on the hardware facts in the brief plus these verified details:

- **TV5725 register map.** gbs-control's [`tv5725.h`](https://github.com/ramapcsx2/gbs-control/blob/master/tv5725.h) exposes the controls behind every OSSC-style sampling and sync option:
  - pre-ADC filter `ADC_FLTR` (2 bits);
  - 5-bit SOG slicer threshold `ADC_SOGCTRL`;
  - per-channel ADC gain `ADC_R/G/BGCTRL` and offset `ADC_R/G/BOFCTRL`, plus auto-offset `ADC_AUTO_OFST_*`;
  - 12-bit sampling-PLL divider `PLLAD_MD`, i.e. the "H. samplerate" / total pixels per line;
  - 5-bit sampling phase `PA_ADC_S`, i.e. 32 phase steps;
  - clamp window `SP_CS_CLP_ST/SP` and manual clamp `SP_CLAMP_MANUAL`;
  - PLL coast `SP_PRE_COAST`/`SP_POST_COAST`;
  - sync-pulse thresholds and jitter handling `SP_SYNC_PD_THD`, `SP_JITTER_SYNC`, `SP_H_PULSE_IGNOR`;
  - input selection `ADC_INPUT_SEL`;
  - 2x ADC clock option `ADC_CLK_ICLK2X`.
- **Status and measurement registers.** These give input-format flags for NTSC/PAL interlaced and progressive, VGA/SVGA/XGA/SXGA at several refresh rates, 720p50/60, 1080i/p and "USER". They also measure H total, V total, H-sync width, sync polarities, H/V period, SOG good/bad/switch and PLL lock status, plus a **pull-down (film cadence) detect flag** `STATUS_DEINT_PULLDN`. Together these provide everything an OSSC-style status line or a RetroTINK-style info screen needs, and a robust "input signature" for auto-loading profiles.
- **Processing blocks in the same map:**
  - motion-adaptive deinterlacer (`MADPT_*`) and bob path;
  - H/V scaler (`VDS_HSCALE/VSCALE`);
  - multi-band **peaking** (`VDS_PK_*`) and **step-response / transient** (`VDS_STEP_*`) controls;
  - black-level expansion (`VDS_BLEV_*`), white level, **noise reduction** (`VDS_NR_*`), skin-tone and blue-stretch blocks;
  - Y gain and U/V cos/sin gains, which give contrast, saturation and hue;
  - Y/U/V offsets (brightness);
  - a hardware frame-lock enable bit (`VDS_FLOCK_EN`);
  - a PIP window with its own gain and offset (`PIP_*`);
  - ADC→DAC bypass paths (`DAC_RGBS_ADC2DAC`, `DAC_RGBS_BYPS2DAC`) used for pass-through.
- **The built-in OSD is not a free-text character generator.** gbs-control's register map documents only:
  - an icon-and-bar menu of **8 fixed icons**: Brightness, Contrast, Hue, Sound, Up/Down, Left/Right, Vertical size, Horizontal size;
  - a value bar (`OSD_BAR_LENGTH`, `OSD_BAR_FOREGROUND_VALUE`);
  - **8 colors (3-bit)** for foreground, background, border and selection;
  - H zoom 1–8x and V zoom 1–4x;
  - vertical or horizontal layout;
  - position.

  No font RAM or text registers are documented ([tv5725.h, OSD section](https://github.com/ramapcsx2/gbs-control/blob/master/tv5725.h)). So a RetroTINK-style text menu tree **on the TV is probably not achievable with the native OSD**. A "monitor-style" quick-adjust overlay (icon + slider bar) is. **UNVERIFIED:** whether undocumented registers allow custom glyphs; this needs hardware experiments or the full Tvia datasheet.
- **MS9288A (HDMI path).** The Macrosilicon datasheet ([LCSC-hosted PDF, Chinese](https://atta.szlcsc.com/upload/public/pdf/source/20250210/96A200247BE50AB9E322BCD94220CBFA.pdf)) describes a VGA/YPbPr→HDMI converter with:
  - three **10-bit ADCs up to 165 MSPS** and **automatic phase/gain/offset adjustment**;
  - SOG support, VGA/YPbPr auto-detection and "automatic mode detection";
  - a **stereo analog audio ADC (24-bit, 48 kHz)** for embedding audio;
  - 165 MHz TMDS, 24-bit color, **max 1080p60**, hot-plug and optional HDCP via EEPROM;
  - its own "video enhancement" (black/white enhancement, color-transient improvement, dynamic peaking, brightness/saturation/contrast/hue);
  - an **I²C slave interface** and a built-in MCU and firmware.

  Consequences:
  - **HDMI output is a second, independent digitization.** "Pixel-perfect"/optimal sampling and integer scaling can be ruined by the MS9288A's own sampling phase and its mode table. Non-standard timings, including frame-locked 59.8 Hz/50.0x Hz modes and odd totals, may not be recognized. **Treat VGA out as the reference-quality path, and HDMI as "standard VESA/CEA timings only" until proven otherwise on real hardware.**
  - **UNVERIFIED:** whether the RetroScaler GBS-C wires the MS9288A's I²C to the ESP8266. If it does, its enhancement processing could be disabled and its state read for diagnostics.
  - A hobbyist report says the MS9288A needs separate H and V sync and won't lock on RGBS (composite sync) ([Parallax forum](https://forums.parallax.com/discussion/177903/converting-component-vga-audio-to-hdmi-on-p2), via search snippet; page not directly retrievable). TV5725 output with separate H/V sync is therefore mandatory on the HDMI path.
- **ESP8266.** Max 160 MHz; **under 50 kB of RAM is left for heap and data** in station mode; external SPI flash up to 16 MB; **I²C is bit-banged in software**; OTA and an IR-remote interface are supported ([Espressif datasheet](https://documentation.espressif.com/0a-esp8266ex_datasheet_en.pdf)). Consequences:
  - The web UI must be a small static bundle (gzip from LittleFS) talking to a compact JSON/WebSocket API.
  - Large tables such as profile databases or mask PNGs are out of scope.
  - Keep one active WebSocket client.
- **Si5351.** Fractional MultiSynth synthesizer covering 2.5 kHz–200 MHz on up to 8 outputs ([datasheet copy](https://cdn.sparkfun.com/assets/3/e/a/9/a/Si5351-datasheet.pdf)). It can generate arbitrary output pixel clocks. This is the key enabler for **clock-based frame lock**, which avoids changing line or frame totals, and for exact custom refresh rates.
