# Reference: how the existing firmwares drive the TV5725 to a picture (mechanics)

> **Provenance.** Behaviour extracted on 2026-10-13 from two gbs-control code bases: **A**, a
> near-upstream tree ([JuergenLeber/gbs-control](https://github.com/JuergenLeber/gbs-control) master
> plus local fork patches) and **B**, a local modular rewrite of its control layer. It describes
> sequences, mechanisms and the constants the chip needs — not code. GBS Reimagined implements these
> behaviours in its own architecture ([video engine design](../design/video-engine.md)); this file is
> the evidence trail. Citations (`A.ino:N`, `B/x.cpp:N`) point into those trees.

This is a read-only extraction, made on 2026-10-13, for **GBS Reimagined**, a firmware for the same TV5725 scaler board that will be written from scratch. It describes behaviour, order and reasons. It is not code to copy. Every mechanism carries a citation. The constants are given because the chip needs them, and they are facts about the hardware, not code.

## How to read this

| tag | meaning |
|---|---|
| **A** | the near-upstream monolith (`gbs-control-upstream-peaking` tree) |
| **B** | the modular rewrite (`gbs-control` tree) |
| `A.ino:N` | line N of A's `gbs-control.ino` |
| `A/x.h:N`, `B/x.cpp:N` | line N of that file in A or B |
| `sS_OO[b:w]` | register field: segment S, byte offset OO (hex), bit offset b, width w (the encoding of `tv5725.h`) |

Caveats:

- **A is not pure upstream.** Its git log shows fork patches, among them peaking defaults, the Atari ST and CRTC-change fixes, the discrete VSync probe, the "source gone" blanking, keeping the DAC dark until the clamp locks, and a no-WiFi build (`git log` of A, top 15 commits). Wherever this report says "A does X", it means this tree. B was built from the same lineage and inherited those fixes; see section 13.
- **Line numbers were taken from the files as they stand today.** Fragments of five lines or fewer appear only where an exact constant or order matters.
- **Mode numbers** are used throughout. A calls the variable `rto->videoStandardInput`; B has an enum (`B/runtimestate.h:6-20`):

| # | B enum | meaning |
|---|---|---|
| 0 | VS_NONE | unknown / searching |
| 1 | VS_NTSC | 15 kHz 60 Hz (240p, 480i) |
| 2 | VS_PAL | 15 kHz 50 Hz (288p, 576i) |
| 3 | VS_EDTV_60 | 480p |
| 4 | VS_EDTV_50 | 576p |
| 5 | VS_HD_720P | 720p |
| 6 | VS_HD_1080I | 1080i |
| 7 | VS_HD_1080P | 1080p |
| 8 | VS_MED_RES | ~25 kHz "medium res" |
| 9 | VS_STABLE_UNKNOWN | stable but not recognised |
| 13 | VS_PC_BYPASS | graphic/VGA mode on YPbPr |
| 14 | VS_RGBHV_SCALED | RGBHV through the scaler |
| 15 | VS_RGBHV_BYPASS | RGBHV pass-through |

---

## 1. Bus access

### 1.1 Physical bus and devices

- **One I²C bus serves every device.** The ESP8266 (Wemos D1 mini) drives SDA=D2/GPIO4 and SCL=D1/GPIO5. The OLED is constructed on those same pins (`A.ino:36`, `B/gbs-control.ino:48`, pin comments `A.ino:164-166`).
- **Addresses (7-bit):**

| device | address | source |
|---|---|---|
| TV5725 | 0x17 | `GBS_ADDR`, `A/tv5725.h:6`, `B/tv5725.h:6` |
| Si5351 | 0x60 | `SIADDR`, `A/src/si5351mcu.h:58` |
| SSD1306 OLED | 0x3C | `A.ino:36` |
| board EEPROM | 0x50 | used only by debug commands, `A.ino:2753-2774`, `A.ino:8606-8630` |

- **Bus speed is 400 kHz, with the ESP's internal pull-ups off.** `startWire()` calls `Wire.begin()`, sets both pins to `OUTPUT_OPEN_DRAIN` so that the board's own pull-ups are used, then calls `Wire.setClock(400000)` (`A.ino:5996-6007`, `B/videopath.cpp:660-668`). A comment says 700 kHz also worked at a 160 MHz CPU clock, which both builds use (`platformio.ini`). `stopWire()` turns both pins into inputs (`A.ino:5989-5994`).
- **Bus recovery and power detection.**
  - `checkBoardPower()` writes 0x6A to the spare register `ADC_UNUSED_69` (s5_69), reads it back, then clears it. A mismatch means the chip has no power or I²C is gone (`A.ino:7512-7525`, `B/videopath.cpp:670-684`).
  - If that check fails at boot, the firmware unsticks the bus: up to 40 times it restarts Wire, reads, pulls SCL low for 12 µs and releases, until SDA reads high (`A.ino:8037-8071`, `B/gbs-control.ino:198-231`).
  - At runtime the power check runs at `noSyncCounter` 61 or 62 (`A.ino:9572-9582`).
  - Power return is detected by seeing SCL and SDA both high for 50 ms, waiting 350 ms more, then restarting Wire (`A.ino:9586-9607`, `B/gbs-control.ino:568-593`).

### 1.2 Segment select (register 0xF0) and the field abstraction

- **Banking.** The TV5725 exposes six 256-byte segments, 0 to 5, through one I²C address. The segment is selected by writing the segment number to byte 0xF0 (`SegByteOffset = 0xf0`, `A/tv5725.h:13-16`, `B/tv5725.h:13-16`).
- **The segment cache.** `tw::SegmentedSlave` keeps a cached current segment. It starts at `SegInitial = 0xFF` so that the first access always writes a select. Every field access calls `setSeg()`, which writes 0xF0 only when the segment changes (`A/tw.h:325-333`, `B/tw.h:296-308`).
- **Two caches.** The sketch keeps a second variable, `lastSegment`. `writeOneByte(0xF0, n)` does **not** touch the bus: it only sets `lastSegment`. The next `writeBytes(reg, …)` calls `GBS::write(lastSegment, …)`, and that call selects the segment through the tw cache (`A.ino:495-506`, `B/videopath.cpp:10-21`).
- **The cache must be forced to match the chip after a chip power-up.** If the chip power-cycles while the ESP keeps running, the cache can claim the right segment when the chip is not on it. At boot and on power recovery the code therefore does three things (`A.ino:7812-7817`, `A.ino:9593-9600`, `B/gbs-control.ino:136-141`, `B/gbs-control.ino:580-584`):
  1. a dummy segment-5 read (`SP_SOG_MODE`);
  2. `writeOneByte(0xF0,0)` followed by `writeOneByte(0x00,0)`, which forces a real select of segment 0 and writes to read-only status byte 0_00;
  3. a read of `STATUS_00`.
- **Field encoding.** A field is `UReg<seg, byteOffset, bitOffset, width>`, for example `VDS_VSCALE = UReg<0x03,0x17,4,10>` (`B/tv5725.h:360`). Fields may span bytes. Bits are packed little-endian, from the low byte up (`regDecode`/`regEncode`, `B/tw.h:108-161`). Widths up to 32 bits are supported.
- **Write rule.** A field that is byte-aligned with a width that is a multiple of 8 is written blind. Any other field is read-modify-write over exactly the bytes it covers (`regWrite`, `A/tw.h:179-190`, `B/tw.h:163-174`). Writing one sub-byte field therefore costs one I²C read and one I²C write.
- **`Tie<…>`.** `Tie` groups several fields in the same segment into one burst read and one burst write of the byte range they span; a static assert enforces the shared segment (`B/tw.h:251-279`, `B/tw.h:337-357`). The frame lock uses it to read VTOTAL and VS_ST together (`B/framesync.h:34`).
- **Raw bursts.** Raw access is `GBS::read/write(seg, offset, buf, n)` (`B/tw.h:359-381`). The firmware writes in 16-byte banks (program loads, zeroing) and reads in 16-byte banks for dumps (`B/videopath.cpp:691-700`).
- **Read transaction.** `rawRead` writes the register address and ends the transmission with a STOP, then calls `requestFrom(addr, n)`. There is no repeated start.
  - A copies every byte the Wire layer returns, with no bound against the buffer (`A/tw.h:66-80`).
  - B bounds the copy to the requested size (`B/tw.h:63-80`).
  - B also adds a write-observer hook, used by its journal (`B/tw.h:82-96`).

### 1.3 Si5351 (external pixel clock)

- **What it drives.** Output CLK0 feeds the TV5725 clock input pin (CKIN, "pin 40"). The pad is enabled by `PAD_CKIN_ENZ=0` (s0_49 bit 0). The chip uses the external clock as display clock when `PLL648_CONTROL_01` (s0_41) = 0x75 (`A.ino:389`, `A.ino:9513-9523`, `B/displayclock.cpp:261-276`).
- **Detection** (`A.ino:447-493`, `B/displayclock.cpp:177-223`), in order:
  1. skip if the user disabled it;
  2. probe address 0x60 for an ACK;
  3. read register 0 (device status) and require bit 7 (SYS_INIT) = 0, meaning ready;
  4. `Si.init(25 MHz)`: the crystal is assumed to be 25 MHz, a comment notes 27 MHz modules;
  5. write register 183 (XTAL_CL) = 0xD2 (10 pF load);
  6. set CLK0 drive to 6 mA, set 81 MHz, then **disable** the output.

  The output stays off until the firmware adopts it.
- **Boot order around detection.** The firmware sets `PAD_CKIN_ENZ=1` (pad off) *before* detection, "to prevent startup spike damage". Because the library calls `Wire.begin()`, the bus is restarted afterwards (`A.ino:8006-8012`, `B/gbs-control.ino:171-177`).
- **Library init.** `init()` clears bit 7 of register 149 (spread spectrum off) and disables all outputs (`A/src/si5351mcu.cpp:50-63`). B's copy of the library is identical apart from CRLF line endings.
- **Frequency math** (`setFreq`, `A/src/si5351mcu.cpp:85-270`):
  1. Output divider = 900 MHz / f. While it exceeds 900, double the R divider and halve the output divider. Round down to an even number (lines 98-108).
  2. Patch: divider 8 becomes 6 for f ≥ 100 MHz, and divider 10 becomes 8 for 75–90 MHz, "to keep fvco in a more stable region" (lines 112-118). The effect is that 81, 108 and 162 MHz all land on a VCO of exactly **648 MHz** (162 MHz uses the special divide-by-4 mode, line 210).
  3. fvco = divider × R × f (line 121).
  4. Feedback divider = fvco / xtal, with the fraction b/c where c = xtal >> 5 (lines 154-157).
  5. Write 8 bytes of PLL-A parameters at register 26 and 8 bytes of MultiSynth-0 at register 42 (lines 234-235).
  6. Reset the PLL (register 177 = 0xA0) **only when the output or R divider changed**. Otherwise only the PLL fraction is rewritten, which retunes without a click (lines 193-256, `reset` at 273-276).
  7. Output enable writes register 16+clk = 0x4C + drive; disable writes 0x80 (lines 315-350).
- **Workarounds in the firmware.**
  - Before 108 MHz the firmware first sets 87 MHz, and before 40.5 MHz it first sets 48.5 MHz, each followed by 1 ms. The comment reads "around 108MHz the library seems to double the clock" (`A.ino:376-387`, `B/displayclock.cpp:95-105`).
  - Retunes are walked in 1 kHz steps when the change is under 750 kHz, to avoid upsetting the sink (`setExternalClockGenFrequencySmooth`, `A/framesync.h:69-95`, `B/displayclock.cpp:26-60`). B also clamps the frequency to 20–162 MHz (`B/displayclock.h:6-7`, `B/displayclock.cpp:32-36`).

---

## 2. Boot to first picture

### 2.1 A, step by step (`setup()`, `A.ino:7765-8135`)

1. **Local hardware.** The firmware initialises the OLED, sets the encoder pins as `INPUT_PULLUP`, and attaches the encoder interrupts. It then builds the OLED menu and registers the OSD handlers (`A.ino:7767-7778`).
2. **Serial and network setup.** Serial runs at 115200. The hostname is loaded from LittleFS (`A.ino:7791-7810`).
3. **First I²C contact.** The firmware calls `startWire()`, then does the dummy accesses that align the segment cache, as described in 1.2 (`A.ino:7812-7817`).
4. **WiFi.** It turns WiFi sleep off, sets output power to 16 dBm, and starts the web server (`A.ino:7819-7831`).
5. **Defaults.** It loads the default options and the runtime defaults (`A.ino:7838-7891`). Among them:
   - phase SP = 16, phase ADC = 16, SOG level 5;
   - `syncWatcherEnabled`, `autoBestHtotalEnabled`;
   - `sourceDisconnected = true`, which is what starts input detection in `loop()`.
6. **LED and splash.** The LED goes on, then a splash shows for 1500 ms while WiFi is serviced (`A.ino:7893-7908`).
7. **Soft reset of the chip.** `RESET_CONTROL_0x46` and `0x47` are written to 0, which holds every block in reset. `PLLAD_VCORST=1` and `PLLAD_PDZ=0` turn the ADC PLL off (`A.ino:7909-7913`).
8. **Preferences.** They are read from `/preferencesv2.txt` (`A.ino:7916-8003`; format in 11.2).
9. **External clock.** The firmware sets `PAD_CKIN_ENZ=1`, detects the Si5351, restarts Wire, and does 3 dummy `STATUS_00` reads (`A.ino:8006-8012`).
10. **Wait.** It waits 1000 ms while servicing WiFi (`A.ino:8015-8019`).
11. **Power and bus check,** with the unstick procedure if needed. If the check still fails, `boardHasPower=false` and `syncWatcherEnabled=false` (`A.ino:8037-8071`).
12. **Bring-up when power is good.** This is the order the chip is brought up in (`A.ino:8073-8104`):
    1. retry clock-generator detection if it was not found;
    2. **`zeroAll()`**: put every block in reset, then write zero to all 256 bytes of segments 0–5 in 16-byte banks (`A.ino:524-552`);
    3. **`setResetParameters()`**: the baseline "idle / detect" state, see 2.3;
    4. **`prepareSyncProcessor()`**: the SP baseline, see section 3.6;
    5. read the chip ID (`CHIP_ID_PRODUCT`, `CHIP_ID_REVISION`, s0_0C/0D);
    6. **`calibrateAdcOffset()`** if it is enabled (on by default), see 4.2;
    7. `setResetParameters()` again.
13. **End of setup.** The LED goes off. No program (preset) has been loaded at this point. The first picture comes from `loop()`:
    1. **Input detection.** Every 500 ms, while `sourceDisconnected`, the firmware detects the input (`A.ino:9547-9568`; section 3).
    2. **Mode seen.** Once a source is found, `sourceDisconnected` is false and the sync watcher (every 20 ms) sees a mode that is not 0 while the current mode is 0.
    3. **Mode confirmed.** After 8 consecutive passes and 30 confirming reads, it calls `applyPresets(mode)` (`A.ino:6566-6659`).
    4. **Program load.** `applyPresets` calls `writeProgramArrayNew(preset)` and then `doPostPresetLoadSteps()` (section 4).
    5. **Late steps from `loop()` counters:** coast, clamp, best HTotal, phase, the "done stage" that turns DAC and sync back on and adopts the Si5351, and the frame lock (section 5.4).

### 2.2 B, step by step (`setup()`, `B/gbs-control.ino:80-274`)

The order is the same as A's. B differs in four places:

- **Preferences load before the bus starts.** B reads its binary prefs, user register overrides and profiles before `startWire()`, and sanitises the picture configuration (`B/gbs-control.ino:106-134`).
- **Runtime state is reset by one call,** `stateResetBoot()` (`B/state.cpp:48-67`; called at `B/gbs-control.ino:150`).
- **The hardware sequence is otherwise A's.** The reset at lines 166-169, CKIN off and Si5351 detection at 171-177, the power check at 198-231, and then `zeroAll`, `setResetParameters`, `prepareSyncProcessor`, chip ID, ADC calibration and `setResetParameters` (`B/gbs-control.ino:244-259`).
- **`loop()` is a scheduler, not one function.** It runs two tables of ticks (`B/gbs-control.ino:595-630`):
  - **first table:** OLED, network and the subsystem ticks. The subsystem ticks include the non-blocking `detectTick`, `postLoadTick` and `timingTick` (`B/gbs-control.ino:293-320`).
  - **second table:** frame lock; IRQ clear every 3000 ms; watcher every 20 ms; auto HTotal; coast; clamp; done stage; source detection every 500 ms; power check; power recovery.

  B also has a single program, not presets. `applyPresets()` always writes `baseProgram` and then calls `timingRequestDefaultMode()` (`B/program.cpp:994-1022`; see section 6.3).

### 2.3 Baseline state written by `setResetParameters()`

This is the state between programs and while searching for a source (`A.ino:804-917`, `B/program.cpp:210-288`), in order:

1. **Runtime flags.** Clear the runtime flags. SOG level becomes 5, `medResLineCount` becomes 0x33, and so on.
2. **Scratch registers.** Clear the scratch bytes 5_64–5_67 and the firmware's own option bits held in the program image: `GBS_PRESET_ID`, `GBS_PRESET_CUSTOM` and the options in s1_2B/2C.
3. **Minimal input-formatter timing.** `IF_VS_SEL=1`, `IF_VS_FLIP=1`, `IF_HSYNC_RST=0x3FF`, `IF_VB_ST=0`, `IF_VB_SP=2`.
4. **Frame lock.** `FrameSync::cleanup()`.
5. **Outputs off.** `OUT_SYNC_CNTRL=0` (no H/V sync to the pads) and `DAC_RGBS_PWDNZ=0` (DAC off).
6. **ADC on, for detection.**
   - `ADC_TA_05_CTRL=0x02`, `ADC_TEST_04=0x02`, `ADC_TEST_0C=0x12`;
   - `ADC_CLK_PA=0` (ADC clock comes from PLLAD);
   - `ADC_SOGEN=1`, `SP_SOG_MODE=1`;
   - `ADC_INPUT_SEL=1` (RGB);
   - `ADC_POWDZ=1`.

   Then latch the SOG level. The reason is in a comment: the ADC must stay enabled for SOG separation to "see" a source (`A.ino:1365-1367`).
7. **Blocks in reset, GPIO quiet.** Resets 0x46 = 0x47 = 0. `GPIO_CONTROL_00=0x67` and `GPIO_CONTROL_01=0`: regular GPIO, outputs disabled.
8. **Clocks.** `PLL648_CONTROL_01=0x00` (no display clock), `PAD_CKIN_ENZ=0`, `PAD_CKOUT_ENZ=1`, `IF_SEL_ADC_SYNC=1`, `PLLAD_VCORST=1`, `PLL_ADS=1` (input clock from the ADC), `PLL_CKIS=0` (PLL runs from the oscillator), `PLL_MS=2` (memory clock).
9. **Pads.** `PAD_CONTROL_00_0x48=0x2B` turns the digital inputs off and the debug-out pin on. `PAD_CONTROL_01_0x49=0x1F` sets s0_49 bits 0–4, so the external clock pad is off again until post-load enables it (`A.ino:3418`).
10. **Constant sections and ADC levels.** `loadHdBypassSection()` (1_30–1_5F), `loadPresetMdSection()` (1_60–1_83), then the default ADC gain and offset (4.2).
11. **Sync processor and PLLAD.** `SP_PRE_COAST=9`, `SP_POST_COAST=18`, `SP_NO_COAST_REG=0`, clamp window 32/48, `SP_SOG_SRC_SEL=0` (SOG from the ADC), `SP_EXT_SYNC_SEL=0` (HV input connected), `SP_NO_CLAMP_REG=1` (clamp off). For PLLAD: `PLLAD_ICP=0`, `PLLAD_FS=0`, `PLLAD_5_16=0x1F`, `PLLAD_MD=0x700`.
12. **PLL resets.** `resetPLL()`, 2 ms, `resetPLLAD()`, then hold both in reset: `PLL_VCORST=1` and `PLLAD_CONTROL_00_5x11=0x01`.
13. **Debug port.** `resetDebugPort()` (5.5).
14. **Release the blocks the detector needs.**
    - 0x46 = 0x41 releases IF and VDS;
    - 0x47 = 0x17 releases DEC, MODE, SYNC and INT;
    - interrupts are enabled (`INTERRUPT_CONTROL_01=0xFF`) and cleared (00 = 0xFF, then 0x00);
    - `PAD_SYNC_OUT_ENZ=0`: "sync output enabled, will be low (HC125 fix)" (`A.ino:910`).

Soft-reset bits:

| register | bit | block |
|---|---|---|
| 0x46 | 0 | IF |
| 0x46 | 1 | DEINT |
| 0x46 | 2 | MEM_FF |
| 0x46 | 3 | MEM |
| 0x46 | 4 | FIFO |
| 0x46 | 5 | OSD |
| 0x46 | 6 | VDS |
| 0x47 | 0 | DEC |
| 0x47 | 1 | MODE |
| 0x47 | 2 | SYNC |
| 0x47 | 3 | HDBYPS |
| 0x47 | 4 | INT |

Field definitions: `B/tv5725.h:171-182`, `B/tv5725.h:747-748`.

### 2.4 Program load: segments, offsets and sizes (`writeProgramArrayNew`)

A program ("preset") is a flat array of **432 bytes** (`A.ino:594-760`, `B/program.cpp:53-208`). It is consumed in this order:

| array index | chip range | bytes | special handling while writing |
|---|---|---|---|
| 0–31 | s0_40–s0_5F | 32 | **s0_46/s0_47 keep the reset state read before the load.** With the "HDMI sync fix", s0_44 bit 0 (DAC) is cleared and s0_49 bit 2 (sync out disabled) is set |
| 32–47 | s0_90–s0_9F | 16 | the OSD registers |
| 48–95 | s1_00–s1_2F | 48 | in the first bank: clear 1_00 bit 5 (`IF_VS_SEL`), set 1_01 bit 0 (`IF_VS_FLIP`), clear the upper nibble of 1_0C and all of 1_0D (`IF_INI_ST`=0) |
| — | s1_60–s1_83 | 36 | the constant MD section, unless skipped. Then `MD_SEL_VGA60` = !csync and `MD_HD1250P_CNTRL` = the remembered med-res line count |
| — | s2_00–s2_3F | 64 | the constant deinterlacer section |
| 96–223 | s3_00–s3_7F | 128 | then s3_80–s3_8F are **written to 0** ("blank out VDS PIP registers, otherwise they can end up uninitialized") |
| 224–319 | s4_00–s4_5F | 96 | — |
| 320–431 | s5_00–s5_6F | 112 | see below |

Patches inside segment 5:

| index | register | patch |
|---|---|---|
| 322 | 5_02 | bit 6 = input select taken from the current input |
| 323 | 5_03 | clamps: YPbPr gets R/B mid and G bottom; RGB gets all bottom |
| 352 | 5_20 | forced to 0x02 (only `SP_SOG_P_ATO`) |
| 375 | 5_37 | `SP_H_PULSE_IGNOR` = 0x6B for SD, otherwise 0x02 |
| 382 | 5_3E | bit 5 `SP_DIS_SUB_COAST`=1 |
| 407 | 5_57 | bit 0 `SP_NO_CLAMP_REG`=1, so the clamp is held off until its position is measured |

The program never touches s0_00–s0_3F (status), s1_30–s1_5F (HD bypass) or s1_84–s1_FF. Segment-load functions: `A.ino:554-590`, `B/videopath.cpp:58-94`. The MD section deliberately ends at 0x83 (`A.ino:589`).

Before the load:

- `FrameSync::cleanup()`;
- mode 15 becomes 0;
- `outModeHdBypass=0`;
- `inputIsYpBpR` is re-derived from `ADC_INPUT_SEL` (`A.ino:604-618`).

After the load, if scaling RGBHV is valid, `GBS_OPTION_SCALING_RGBHV=1` and the mode becomes 3 (`A.ino:755-759`). B also notes the program's VSCALE for later geometry (`B/program.cpp:202-207`).

**State stored inside the chip image.** The firmware keeps its own state in otherwise unused registers that are part of the program image, so custom presets saved to flash carry it:

| field | location | meaning |
|---|---|---|
| `GBS_PRESET_ID` | s1_2B[0:7] | output id: 0x01 = 960p, 0x02 = 1024, 0x03 = 720p, 0x04 = 480p, 0x05 = 1080p, 0x06 = downscale; PAL adds 0x10; 0x21 = HD bypass, 0x22 = RGBHV bypass (`A.ino:211-216`, `A.ino:4177-4192`) |
| `GBS_PRESET_CUSTOM` | s1_2B bit 7 | program is a custom preset |
| `GBS_OPTION_SCANLINES_ENABLED`, `…SCALING_RGBHV`, `…PALFORCED60_ENABLED`, `GBS_RUNTIME_FTL_ADJUSTED` | s1_2C bits | option and runtime flags |
| `GBS_PRESET_DISPLAY_CLOCK` | s1_2D | the internal display-clock code saved while the Si5351 is in use |

Field locations: `A/tv5725.h:254-261`. Scratch bytes in s5_60–s5_69 are also used: debug-view backups, the "sync fix" temporaries, and the power-check byte.

### 2.5 The ofw_* dumps and the constant sections

- **`ofw_RGBS.h` and `ofw_ypbpr.h` are register dumps of the original GBS-8200 MCU firmware ("OFW")** for an RGBS input and a YPbPr input. Each has 496 values. They cover s0_40–5F and s0_90–9F (48), s1_00–2F (48), **s2_00–3F (64)**, s3_00–7F (128), s4_00–5F (96) and s5_00–6F (112), counted from the per-line comments.
- **Neither is loaded by any code path.** A includes `ofw_RGBS.h` (`A.ino:18`) but never references the array. `ofw_ypbpr.h` is not included at all, and B has neither. They are reference material only. `A/rgbhv.h` (an old RGBHV program) is not included either.
- **The three constant sections** are written separately because the 432-byte program does not cover them, and because they should not vary with the output resolution:
  - **MD section** (s1_60–s1_83, 36 bytes). Mode-detect lock and unlock counts and the per-standard H/V detection windows. The comments show tuned values: 1_60 = 0xB6 "H: unlock 5, lock 22", 1_61 = 0x84, 1_7F "changed to ~352" for 25 kHz, 1_80/81 = 0xFF (user mode off), 1_82 = 0x05, 1_83 = 0x0C "MD_UNSTABLE_LOCK_VALUE" (`A/presetMdSection.h:1-37`). It is loaded by `setResetParameters` and by every program load.
  - **Deinterlacer section** (s2_00–s2_3F, 64 bytes). The motion-adaptive deinterlacer defaults, with comments about the motion thresholds chosen against particular PSX games (`A/presetDeinterlacerSection.h:1-66`). Custom presets do not contain segment 2 (`A.ino:3788`).
  - **HD-bypass section** (s1_30–s1_5F, 48 bytes). The HD bypass gains and offsets and the default `HD_HSYNC_RST`, `HD_INI_ST`, HB/HS/VB/VS positions (`A/presetHdBypassSection.h:1-50`). It is loaded by `setResetParameters`, by HD bypass and by RGBHV bypass (`A.ino:882`, `A.ino:5208`, `A.ino:5503`).
- **B's copies hold the same values**, without the comments (verified by diff).

---

## 3. Input detection and switching

### 3.1 Input mux

`ADC_INPUT_SEL` (s5_02[6:2]) chooses the input:

| value | input | source |
|---|---|---|
| 1 | RGB: VGA jack, 5-pin RGBS header, 8-pin header (one electrical input) | `A.ino:1597-1601` |
| 0 | YPbPr on the RCA jacks | `A.ino:1597-1601` |
| 2 | R2/G2/B2, an unconnected input used only to isolate the ADC during offset calibration | `A.ino:7551` |

B exposes all three values as a console command (`B/commands.cpp:25-38`). Sync can arrive as SOG/CSync through the ADC's sync slicer (`ADC_SOGEN`, `SP_SOG_SRC_SEL=0`), or as discrete H and V on the HV pins (`SP_EXT_SYNC_SEL=0` connects them, 1 disconnects them; `SP_SOG_MODE`=1 selects the SOG path) (`A.ino:5543-5576`).

### 3.2 A's detection routine (`detectAndSwitchToActiveInput`, `A.ino:1602-1812`)

Called every 500 ms while the source is disconnected (`A.ino:9547-9557`):

1. **Dwell.** For up to 450 ms, every 10 ms, test `getStatus16SpHsStable()`. This needs STATUS_16 bit 1 (SP HS active), plus bit 0 = 0 (active-low) when the current mode is NTSC or PAL (`A.ino:4691-4716`). On timeout, flip the input mux, wait 200 ms, and return "nothing". The new input is examined on the next call (`A.ino:1805-1808`).
2. **RGB input.** Set the SOG level to 13. Then:
   - **VSync test.** Wait up to 360 ms for STATUS_16 bit 3 (VS active), "good up to 5_34 = 0x0b". If it never comes, run `probeDiscreteVSync()`, because some HV sources never raise that bit (3.4).
   - **VSync present** (RGBHV, or CSync on the HS pin): `MD_SEL_VGA60=1`, wait up to 400 ms for HS active, then `SP_H_PROTECT=1` and 120 ms.
   - **CSync-on-HS test.** Pretend CSync and measure the field rate three times on the SP test bus. If at least two readings are above 40 Hz, it is CSync on HS: `SP_PRE_COAST=0x10` ("EGA 364") and `syncTypeCsync=true`.
   - **25 kHz test.** Step `MD_HD1250P_CNTRL` up 16 times at 30 ms intervals, looking for mode 8. If found, remember the line count and return the RGBS path.
   - **Otherwise RGBHV.** Set mode 15 and apply RGBHV bypass immediately, return 3 (`A.ino:1619-1705`).
   - **No VSync: RGBS / CSync / SOG.** `syncTypeCsync=true`, `MD_SEL_VGA60=0` ("EDTV60 more likely"). For up to 6000 ms, every 2 ms:
     - return as soon as `getVideoMode()` is above 0 and not 8;
     - every 150 cycles, step the SOG level by +2 (from 1 to 2, wrapping at 15 to 1);
     - on mode 8, set SOG to 13 and keep the med-res count;
     - cycle `MD_HD1250P_CNTRL` from 0x33 to 0x3C.

     On timeout, return RGB anyway and "let later stage deal with it" (`A.ino:1712-1761`).
   - **HS present but neither path resolved.** `SP_SOG_MODE=1`, reset the SP and MD, wait 40 ms (`A.ino:1763-1766`).
3. **YPbPr input.** `MD_SEL_VGA60=0`. For up to 6000 ms, every 2 ms, wait for any mode. Every 180 cycles step SOG by +2, wrapping at 16. On timeout set SOG to 14 and return YPbPr (`A.ino:1767-1798`).
4. **Activity lost during the tests.** SOG becomes 2 and the mux flips (`A.ino:1800-1806`).
5. **`inputAndSyncDetect()` acts on the result** (`A.ino:1814-1858`):
   - **0:** if the SP test bus shows no sync at all (`TEST_BUS > 0x180` means sync present, `A.ino:4656-4681`), enter low power once (`goLowPowerWithInputDetection`, `A.ino:1368-1379`);
   - **1 or 2:** set `inputIsYpBpR`, clear `sourceDisconnected`, call `resetDebugPort()` and apply the RGB or YUV patches;
   - **3:** RGBHV, already applied.
6. **Between calls, the SOG slicer walks down.** `loop()` moves the slicer one step per call from 6 down to 2, then wraps back to 6 (`A.ino:9558-9566`).

### 3.3 B's detection (`B/detect.cpp:11-348`)

B uses the same states, as a non-blocking state machine with A's constants (lines 38-51):

| state | does |
|---|---|
| SCAN | looks once after 10 ms; if HS is not stable, flips the mux and waits 200 ms, with **no 450 ms dwell** |
| RGB_VSYNC, RGB_HSYNC, CSYNC_SETTLE, MEDRES | A's RGB path with VSync: 360 ms VS wait, 400 ms HS wait, 120 ms settle, 16 med-res steps of 30 ms |
| RGB_NOVSYNC | A's RGBS walk: 6000 ms window, SOG walk every 150 cycles |
| COMP | A's YPbPr walk: SOG walk every 180 cycles |
| LOST | flip mux, 200 ms |
| RGBHV_SETTLE | 100 ms |

Entry points:

- **`tickSourceDetect`** (every 500 ms) starts the machine and walks the SOG level down between runs (`B/gbs-control.ino:528-550`).
- **The watcher and the post-load stay idle** while detection is busy (`B/watch.cpp:1251`).

### 3.4 Discrete-VSync probe (fork addition, both builds)

`probeDiscreteVSync()` (`A.ino:1172-1252`, `B/measure.cpp:6-69`):

1. Connect the HV input and turn SOG mode off, then wait 260 ms for the counters to settle.
2. Take up to 9 samples of `STATUS_SYNC_PROC_VTOTAL`, 20 ms apart. Stop early on three zeros, because zero is the "no H lock" reading.
3. Take the median.
4. Decide "found" if VS active is flagged, or if the median lies between 200 and 1300 lines and at least two thirds of the samples are within ±2 of it.
5. Restore the HV input and SOG mode settings.
6. Set `vsyncFlagUnreliable` when the line count found the vertical and the flag did not.

Why it exists: the "CSync" verdict gates the HV input off, so a source misjudged as CSync can never be measured again. The comment calls this "manufactured by the verdict" (`A.ino:1152-1171`). It is re-run from the watcher (section 5).

### 3.5 Standard classification (`getVideoMode`, `A.ino:4513-4653`; `B/mode.cpp:41-168`)

| test | result |
|---|---|
| mode ≥ 14 already | STATUS_16 bit 1 or bit 3 set means the mode stays, otherwise 0 |
| STATUS_00 & 0x07 = 0x07 (V, H, HV OK) and bit 7 (SD) set | bit 3 → 1 (NTSC interlace), bit 5 → 2 (PAL interlace), bit 4 → 3 (480p), bit 6 → 4 (576p) |
| same, STATUS_03 bit 4 (720) | 5 |
| current mode 4 and STATUS_04 == 0x80 | stays 4 |
| STATUS_04 bit 5 (HD) with & 0x61 = 0x61 and `VPERIOD_IF` < 1160 | 6 (1080i; the guard is against 576p misdetected as 1080i, h 215 / v 1249) |
| STATUS_04 bits 5 and 4 | 7 (1080p); with bit 2 also set → 8 (the "HD2376_1250P" flag reused for 25 kHz) |
| STATUS_05 & 0x0C = 0 (H and V not unstable), STATUS_00 == 0x07, STATUS_03 bit 1 (PC graphic) | 13 on YPbPr, 15 on RGB (graphic modes go to RGBHV handling) |
| same but no graphic bit | the MD's XGA/SXGA/SVGA/VGA H-count targets are jittered by a random −2…+1 to coax a lock, because "the horizontal counter target in MD is very strict" |
| STATUS_00 & 0x2F = 0x07, SP HS active, VTOTAL steady within ±1 across 2 reads 2 ms apart | `notRecognizedCounter`++; at 255 → **9** |

B adds a **user-defined mode**. When `uopt->modeUserAs` is set and STATUS_05 bit 0 (`STATUS_IF_INP_USER`) is high, `modeRead()` returns the configured standard. The MD user H/V counts in s1_80/81 are written from preferences (`B/mode.cpp:111-114`, `B/mode.cpp:153-158`). B also moves the VGA nudge out of `modeRead()` into `getVideoMode()` (`B/mode.cpp:97-109`, `B/mode.cpp:160-168`).

### 3.6 Sync-processor baseline (`prepareSyncProcessor`, `A.ino:1254-1340`; `B/videopath.cpp:268-351`)

This is called at boot, at every low-power entry and in post-load. It writes raw bytes in segment 5. The SP H-detect thresholds:

| register | value |
|---|---|
| 5_21 | 0x18 |
| 5_22 | 0x0F |
| 5_23 | 0x00 |
| 5_24/25 | 0x0040 |
| 5_26/27 | 0x0004 |
| 5_2A | 0x0F |

V-detect thresholds, timers, and the rest of the baseline:

- **V-detect, effective only with HV input:** 5_2D = 0x03, 5_2E = 0x00, 5_2F = 0x02, 5_31 = 0x2F.
- **Timers:** `SP_H_TIMER_VAL` (5_33) = 0x3A, `SP_V_TIMER_VAL` (5_34) = 0x06.
- **`SP_DLT_REG` (5_35):** 0x70 when the mode is unknown, 0xC0 for modes 1–4, 0xA0 for 5–6, 0x70 for 7 and above.
- **`SP_H_PULSE_IGNOR` (5_37):** 0x6B for SD, otherwise 0x02.
- **`SP_H_TOTAL_EQ_THD` (5_3A) = 3,** with the comment "make sure this is stored in the presets as well".
- **CSync VS extraction:** start 4, stop 1 (5_3B/3F/40).
- **`SP_CS_HS_ST/SP`:** 0x10 and 0x00.
- **Retime HS:** 5_49–5_4C = 0x00, 0x00, 0x44, 0x00.
- **5_51–5_54:** 0x02, 0, 0, 0.
- **Clamp and coast, unless in RGBHV:** auto clamp allowed, clamp source 27 MHz, clamp off, SOG mode on, H coast window 0x10–0x100, sub-coast on, `SP_H_PROTECT=1` "for detection", no auto coast.
- **`SP_HS_REG=1`,** no SP H/V inversion.
- **5_58–5_5D:** 0x05, 0x00, 0x01, 0x00, 0x03, 0x02. The comment on 5_5D reads "how long should clamp stay off".

---

## 4. Post-program-load steps in order

### 4.1 A's `doPostPresetLoadSteps()` (`A.ino:3372-4240`)

The steps run in this order:

1. **DAC off for the whole sequence** (`A.ino:3376-3383`). Reason given: with the clamp unlocked, black drifts upward, so the picture "comes up dim and grows brighter before snapping right".
2. **Auto-gain bookkeeping.** Keep the stored gain; for a custom preset, adopt the gain it carried (`A.ino:3386-3397`).
3. **Identify what was loaded.** If the mode is unknown, read it. Read `presetID` and `isCustomPreset` back from the chip (`A.ino:3403-3412`).
4. **Clear scratch** 5_64–5_67 and **enable the external clock pad** (`PAD_CKIN_ENZ=0`) (`A.ino:3414-3418`).
5. **Sync processor.** `prepareSyncProcessor()` for non-custom programs. Mode 14 gets the RGBHV-scaled SP set-up, which differs for HV and CSync (`A.ino:3420-3459`).
6. `SP_H_PROTECT=0`, `SP_COAST_INV_REG=0`, then `updateSpDynamic(0)` (`A.ino:3461-3466`).
7. **Clamp held off** (`SP_NO_CLAMP_REG=1`) and sync out prepared (`OUT_SYNC_CNTRL=1`) (`A.ino:3468-3469`).
8. **ADC auto-offset unit configured** (by line, delay 0, abs-diff step, test on, full range), but not enabled (`A.ino:3472-3476`).
9. **YUV or RGB patches** (8.4) (`A.ino:3478-3482`).
10. **HD bypass.** `OUT_SYNC_SEL=1` (sync from the HD bypass) and auto HTotal off. Otherwise auto HTotal on (`A.ino:3484-3489`).
11. **Phase and SOG.** Read the ADC phase from the program; SP phase = 8. SOG level 14 for YPbPr, 13 for RGB, then `setAndUpdateSogLevel`, which latches the phases and PLLAD and clears IRQs (`A.ino:3491-3500`, `A.ino:1344-1354`).
12. **Defaults and counters.** Default ADC gain and offset for non-custom programs; GPIO; reset the clamp, coast, phase and stability counters (`A.ino:3502-3520`).
13. **Per-standard input-formatter and ADC tuning** for non-custom programs (`A.ino:3526-3765`):
    - **all modes:**
      - modes 3, 4, 8, 9: `IF_LD_RAM_BYPS=1` (no line doubler);
      - `IF_INI_ST=0` (a fixed value instead of a computed one);
      - IF H interpolator on (`IF_HS_INT_LPF_BYPS=0`, "smoother scrolling … reduces jailbars"), interpolator rather than LPF;
      - non-linear phase shift bypassed;
      - `IF_LD_WRST_SEL=1` ("fixes output position regardless of 1_24");
      - SP retime HS stop = 0.93 × `PLLAD_MD`;
      - peaking and step gain (8.3);
      - oversampling 2× prepared;
      - optional full height for 1080p50.
    - **modes 1 and 2:** ADC filter 3 (40 MHz), `PLLAD_KS=2`, oversampling 4×, `IF_SEL_WEN=0`. For YPbPr: IF tap11 on, Y delay 2, VDS V/Y delays. The downscale preset has its own IF and deinterlacer scaler set.
    - **modes 3, 4, 8, 9 (EDTV):** ADC filter 3, `KS=1`, CSync VS 14/11, `IF_HB_SP=0` "fixes colors", oversampling 2×, `IF_HS_DEC_FACTOR=0`, `IF_LD_SEL_PROV=1`, `IF_PRGRSV_CNTRL=1`, `IF_SEL_WEN=1`, delays (Y delay 3, "ps2 test on one board clearly says 3"). Mode 9 with more than 650 lines: `KS=0` and VSCALE bypassed.
    - **per output id:** `IF_HB_ST`, `IF_HBIN_ST/SP`, `IF_HB_ST2/SP2` capture-window constants, described as "magic number", "blue bar", "green bar" (`A.ino:3677-3721`).
    - **720p:** ADC filter 1 (110 MHz), progressive IF, `INPUT_FORMATTER_02=0x74`, Y delay 3.
    - **1080i/p:** the same, plus `KS=0`.
    - **mode 8:** measure the H-PLL rate 8 times. A low rate gives `PLLAD_FS=0`; then `ICP=6`, ADC filter 1, VSCALE chosen per output id.
14. `resetDebugPort()` (`A.ino:3771`).
15. **CSync with an empty SP test bus** (`TEST_BUS_2F`==0, checked twice 4 ms apart): `optimizeSogLevel()`, and skip auto HTotal (`A.ino:3773-3783`).
16. **`latchPLLAD()`**, "besthtotal reliable with this" (`A.ino:3785`).
17. **Custom program:** derive the oversampling ratio from the DEC bypass bits, and put the internal display clock back if the saved one was external (`A.ino:3787-3808`).
18. **PAL forced to 60 Hz:** shift `IF_VB` by 56 lines, once (`A.ino:3810-3823`).
19. **ADC test registers.** Auto gain on: start at 0x48 or reuse the stored gain, with `DEC_TEST_ENABLE=1`. Load the calibrated offsets. IF auto-offset off ("not reliable yet") (`A.ino:3827-3865`).
20. **Options:** line filter (`VDS_D_RAM_BYPS`), peaking (`VDS_PK_Y_H_BYPS`), `VDS_TAP6_BYPS=0`, step response (`VDS_UV_STEP_BYPS`, never for 1080p) (`A.ino:3872-3903`).
21. **Clock and lock state.** `externalClockGenResetClock()` maps the program's clock code to an Si5351 frequency. Then OSD `Menu::init()`, `FrameSync::cleanup()`, `syncLockFailIgnore=16`, and **`VDS_SYNC_EN=0`, `VDS_FLOCK_EN=0`**: the chip's own frame lock is not used (`A.ino:3906-3915`).
22. **Auto HTotal, modes 1–4 only** (not bypass, not scaled RGBHV, not skipped in step 15):
    1. `updateCoastPosition(0)`, reset the NoHsync and SogBad IRQs, wait about 80 ms;
    2. then up to 4 tries:
       - if SOG is bad, `optimizeSogLevel()`;
       - else if HS is stable twice, the mode matches, and the field rate is in band (58.6–61.4 Hz for 1/3, 49.1–51.1 Hz for 2/4), run `runAutoBestHTotal()`.

    Other modes get `updateCoastPosition` and `updateClampPosition` directly (`A.ino:3917-3969`).
23. **SNES line count.** When `VPERIOD_IF`==523, shift `IF_VB` by +4, because the SNES "has less total lines and a slight offset" (`A.ino:3979-3987`).
24. **Mirror and frame registers.** `VDS_EXT_HB/VB` = the display blank values; `VDS_VSYN_SIZE1/2` = VTOTAL+2; `VDS_FRAME_RST=4`; `VDS_FRAME_NO=1`; `VDS_FR_SELECT=1` (`A.ino:3990-4000`).
25. **"noise starts here!":** `resetDigital()`, `resetPLLAD()`, `PLLAD_LEN=1` (`A.ino:4002-4006`).
26. **Fixed engine settings, non-custom programs** (`A.ino:4008-4032`):
    - `VDS_IN_DREG_BYPS=0`;
    - `PLLAD_R/S=3`;
    - `PLL_R=1`, `PLL_S=2` (lock-detector skew);
    - `DEC_IDREG_EN=1`;
    - `DEC_WEN_MODE=1` ("keeps ADC phase consistent. around 4 lock positions vs totally random");
    - memory: `CAP_SAFE_GUARD_EN=0` ("does more harm than good"), `PB_CUT_REFRESH=1`, `RFF_LREQ_CUT=0`, `CAP_REQ_OVER=0`, `CAP_STATUS_SEL=1` (capture requests at FIFO 50 %), `PB_REQ_SEL=3`, `RFF_WFF_OFFSET=0`;
    - EDTV: `PB_CAP_OFFSET = PB_FETCH_NUM + 4` ("green bar").
27. **`ResetSDRAM()`** unless in bypass (`A.ino:4034-4036`).
28. **Re-latch and sync configuration.** Call `setAndUpdateSogLevel` again; `IF_VS_SEL=0` with `IF_VS_FLIP=1` (the "VCR" IF sync) except for downscale; clamp clock 27 MHz with window 32/48; sync out on; DAC SVM settings `DAC_RGBS_SPD=0`, `S0ENZ=0`, `S1EN=1` ("less jailbars") (`A.ino:4038-4056`).
29. **Sync-processor follow-up.** `SP_H_PROTECT=0`. HD: sub-coast disabled. **CSync: `SP_EXT_SYNC_SEL=1`, which disconnects the HV input until the next reset.** Re-arm coast and clamp (`A.ino:4060-4071`).
30. **HD bypass returns here:** IRQs and unfreeze (`A.ino:4073-4080`).
31. **Wait for the source.** Up to 2002 ms for HS stable, then up to about 1505 ms for a mode, calling `updateSpDynamic` every 4 ms. If 1500 ms or more were needed and SOG ≥ 7: `optimizeSogLevel()` and 300 ms (`A.ino:4086-4110`).
32. **Clamp, then picture.** Up to 25 × `updateClampPosition()`, 20 ms apart. Once it is set, `SP_NO_CLAMP_REG=0` (clamp locked) and **only then DAC on** (`A.ino:4112-4128`).
33. **Finish.** `updateSpDynamic`; SOG re-latch; IRQ enable and clear; `OutputComponentOrVGA()`; `applyPresetDoneStage=1`, or 10 or 11 for a "start in bypass" preference; **`CAPTURE_ENABLE=1`** (unfreeze); the frame-lock notice (`A.ino:4130-4174`).

### 4.2 Calibration and gain helpers

- **`setAdcParametersGainAndOffset()`:** offsets 0x40, gains 0x7B (`A.ino:1047-1060`). Higher "gain" values *dim* the picture; the register is really a range (`A.ino:1035`).
- **`calibrateAdcOffset()`,** at boot (`A.ino:7547-7653`, `B/tune.cpp:877-980`):
  1. Set up an isolated ADC: debug pin off, display clock 162 MHz (0xA5), input 2 (unconnected), DEC matrix bypassed, DEC test on, 5_03 = 0x31 (bottom clamps, 40 MHz filter), clamp window 0, 5_56 = 0x05, 5_57 = 0x80, 5_00 = 0x02, test bus 0x0B (decimator), `resetDigital()`, gains 0x7F.
  2. For G, then R, then B: start the offset at 0x3D and raise it by 1 whenever `TEST_BUS & 0x7FFF` reads 7 or more three times running. Stop after 1500 hits below 7, or after 800 ms without a change. While one channel is calibrated, the other channels' offsets sit at 0x7F, and `DEC_TEST_SEL` selects the channel (1, 2, 3).
  3. Any offset reaching 0x52 or more counts as a failure, and all three revert to 0x40.
- **`runAutoGain()`,** the running auto gain (`A.ino:5648-5707`, `B/tune.cpp:831-875`):
  - **When it runs:** from the watcher tick at stable count > 90, with the clamp set, `noSync`==0 and the H-PLL locked. The debug pin is off, `DEC_TEST_SEL=1` (luma/G) and the test bus is 0x0B (`A.ino:9431-9448`).
  - **Step:** if `TEST_BUS_2F` reads 0x7F (G clipped) twice while sync is stable and STATUS_00 is unchanged, raise all three gains by 2 (less gain).
  - **Rate:** 61 samples per call within 30 s of the last change, otherwise 8.
  - **Start value:** 0x48 (`AUTO_GAIN_INIT`, `A.ino:3370`).

### 4.3 B's post-load (`B/program.cpp:477-937` + `B/program.cpp:303-475`)

B keeps steps 1–30 in the same order, with these differences:

- one base program for everything, so no custom-preset branches;
- the auto-gain restore happens at the top;
- step 13 uses only the EDTV, 720p, 1080 and med-res cases, with A's 1080p output values (`IF_HB_SP2 0xB0`/`ST2 0x4BC` for 480p, `VSCALE 400` for med-res) (`B/program.cpp:598-716`);
- `ifCaptureNoteAnchor()` remembers the program's capture window centre (`B/program.cpp:718`, `B/geometry.cpp:363-390`);
- peaking gains differ (8.3);
- `osdHardwareInit()` replaces `Menu::init()` (`B/program.cpp:777`);
- the scratch reset includes `ignoreModeChangeUntil=0` (`B/program.cpp:780`).

**Steps 31–33 become a non-blocking state machine.** `postLoadStart()` begins it, `postLoadTick()` advances it:

| phase | does |
|---|---|
| WAIT_SYNC | up to 2002 ms, `updateSpDynamic` every 4 ms |
| WAIT_MODE | up to 1505 ms |
| SOG_SETTLE | optional `optimizeSogLevel` + 300 ms |
| CLAMP | 25 tries, 20 ms apart |
| FINISH | — |

Sources: `B/program.cpp:23-50`, `B/program.cpp:434-475`.

`postLoadFinish()` repeats A's ending: clamp lock, then DAC on, SP dynamic, SOG latch, IRQs, `OutputComponentOrVGA`, `applyPresetDoneStage=1`, unfreeze. It then adds geometry and picture bookkeeping:

- `vscaleForSource()`;
- `timingNoteNominalWindow()`;
- `aspectForgetNatural()`;
- `ifCentreCapture()`;
- `pictureCaptureBase()`;
- `modeUserApply()`;
- `scanlineCaptureBase()`;
- `scanlineSync()`.

Sources: `B/program.cpp:318-407`.

### 4.4 Helper mechanisms used by post-load and the watcher

- **`resetDigital()`** (`A.ino:1972-1996`, `B/videopath.cpp:402-423`):
  1. 0x47 = 0x17 (DEC, MODE, SYNC, INT on).
  2. In bypass: 0x46 = 0, 0x47 = 0x1F, then return.
  3. Otherwise 0x46 = 0x41 (IF and VDS stay on; DEINT, MEM_FF, MEM, FIFO and OSD are reset). If the HD bypass was running, 0x47 = 0x1F.
  4. Then 0x46 = 0x7F.
- **`resetPLLAD()`:** `VCORST=1`, `PDZ=1`, latch, `VCORST=0`, 1 ms, latch (`A.ino:1929-1939`).
- **`latchPLLAD()`:** `PLLAD_LAT` 0, 128 µs, 1 (`A.ino:1941-1946`).
- **`resetPLL()`:** `PLL_VCORST` 1, 1 ms, 0, 1 ms (`A.ino:1948-1956`).
- **`ResetSDRAM()`:** `SDRAM_RESET_CONTROL=0x02`, `SDRAM_RESET_SIGNAL` 1 then 0, `SDRAM_RESET_CONTROL=0x82` (`A.ino:1958-1968`).
- **SP and MD resets:** `SFTRST_SYNC_RSTZ` low for 10 µs; `SFTRST_MODE_RSTZ` low for 1 ms ("needed") (`A.ino:1998-2014`).
- **`nudgeMD()`:** toggle `MD_VS_FLIP` twice, which "can fix MD not noticing a line count update" (`A.ino:4844-4848`).
- **Oversampling, `setOverSampleRatio(r)`** (`A.ino:4718-4805`, `B/videopath.cpp:491-571`). `PLLAD_KS` is the post-divider; `PLLAD_CKOS` gives the ADC clock relative to it:

  | ratio | ADC_CLK_ICLK2X | ADC_CLK_ICLK1X | DEC1 | DEC2 | allowed when |
  |---|---|---|---|---|---|
  | 1× | 0 | 0 | bypass | bypass | always |
  | 2× | 0 | 1 | bypass | on | KS ≥ 1 |
  | 4× | 1 | 1 | on | on | KS ≥ 2 |

  An impossible ratio falls back to 1×. EDTV and med-res (not bypass) get an extra +1 on CKOS at 2× ("to avoid a 2x-scaled image").
- **Phase units:** `PA_SP_S`, `PA_ADC_S` (5 bits each), each with a latch bit pulsed 0→1 (`A.ino:4830-4842`). `togglePhaseAdjustUnits()` pulses `PA_SP_BYPSZ` and then `PA_ADC_BYPSZ`, 0→1, 2 ms each (`A.ino:4807-4815`).
- **`optimizePhaseSP()`** (`A.ino:1381-1498`):
  1. Skip unless `STATUS_SYNC_PROC_HTOTAL` is within ±8 of `PLLAD_MD`, meaning the H-PLL is locked.
  2. If SOG ≤ 2, use the fixed phases 16/16.
  3. Otherwise step the SP phase through 34 values. At each step, count how many of 20 HTOTAL reads differ from `PLLAD_MD`, smoothed over 3 neighbouring phases.
  4. Choose SP phase = worst phase + 16, that is, opposite the worst.
  5. Require at least 17 clean steps, otherwise the run is invalid.
  6. ADC phase = 16, shifted +16 for 720p–1080p at 2× or for SD/EDTV at 4×.
- **`optimizeSogLevel()`** (CSync only, `A.ino:1500-1595`):
  1. Start at 13 (RGB) or 14 (YPbPr).
  2. At each level, look for HS active in 60 of the reads within 60 ms. Then require the SP vsync on the test bus (`TEST_BUS_2F` > 0) and HS for 50 consecutive reads.
  3. Otherwise step down by 1, with 8 ms to settle. Below 2, give up at 13.
- **Coast window, `updateCoastPosition()`** (`A.ino:5002-5083`):
  1. Average 8 readings of `HPERIOD_IF`, which is line time / 4 in 9 bits. All must be within ±2 and HS stable. Multiply by 4.
  2. Clip: a value of 2040 or more is taken as 1716. A value of 240 or less with VTOTAL ≤ 322 means low resolution at low rate: use 2000, and for 15 kHz CSync lower `ICP` to 5 and `FS` to 0.
  3. Manual coast: start 0x10, stop 0.968 × line. Auto coast: 0.0562 / 0.155 of the line, with `SP_HCST_AUTO_EN`.
  4. In `loop()`, for SD, `SP_DIS_SUB_COAST=0` and `SP_H_PROTECT=0` follow once the coast is set (`A.ino:9465-9484`).
- **Clamp window, `updateClampPosition()`** (`A.ino:5085-5191`):
  1. Requires a recognised mode, or CSync SD with HS stable (fork fix).
  2. RGB: manual clamp (`SP_CLAMP_MANUAL=1`). YPbPr: auto.
  3. Average 16 line lengths: `HPERIOD_IF` for CSync, `STATUS_SYNC_PROC_HTOTAL` for HV. All must agree within ±2.
  4. Start = 1 + len × (0.032 for CSync, 0.010 for HV). Stop = 2 + len × (0.174 / 0.058). YPbPr start multipliers are 0.089 / 0.032. In YPbPr HD bypass for SD, both move +0x60 and the HD blank level is raised (GY = 5).
  5. Write only if either edge moved by more than 1.
- **`updateSpDynamic(checkMode)`** (`A.ino:4850-5000`) sets per-mode SP parameters:
  - **mode unknown:** alternate `SP_DLT_REG` between 0x30 and 0xC0. When searching (`noSyncCounter%16`), also reset the H pulse ignore, coast window and H timer, and use coast inversion for CSync.
  - **SD 1–2:** pre/post coast 7/3, DLT 0xC0, H timer 0x28. For CSync, compute `SP_H_PULSE_IGNOR` from `HPERIOD_IF` × 0.081 + `PLLAD_MD` × (HS width / line ratio) × 0.38, limited to 0x90 and snapped to 0x1A inside 0x1A–0x42.
  - **EDTV:** 7/6, 0xA0, ignore 0x0E.
  - **720p:** 7/7, 0x30, 0x08.
  - **1080:** 9/18, 0x70, 0x06.
  - **13 and above:** for HV, coasts 0, ignore 0xFF and DLT 0 ("cancel out SOG decoding"); for CSync, 4/7, 0x70, 0x02.

---

## 5. Runtime sync watcher

`runSyncWatcher()` runs every 20 ms while the source is connected and the watcher is enabled (`A.ino:9427-9429`; `A.ino:6046-7510`). B splits it into staged functions with non-blocking "holds" that replace A's `delay()`s, and skips passes while the post-load or detection is busy (`B/watch.cpp:1248-1279`, holds `B/watch.cpp:60-175`). B's call order:

1. HD-bypass line count;
2. PC-bypass mode;
3. SOG guard;
4. sync-loss debounce;
5. classify;
6. CRTC hold;
7. sync lost;
8. follow mode;
9. RGBHV;
10. low power.

### 5.1 What it reads each pass

- `getVideoMode()` and `getStatus16SpHsStable()` (`A.ino:6071-6072`).
- **Sync-loss debounce:** for modes 1–13, three consecutive bad reads are needed before a genuine sync loss. This rides out the HS-active flicker on dark content, which used to trigger phase re-optimisation and "pixels jumping" (`A.ino:6190-6216`, `B/watch.cpp:298-315`).

### 5.2 Special monitors (most are fork additions)

- **HD-bypass SD line tracking.** Every 765 ms, if VTOTAL is steady within ±3 and has changed, set CSync VS start = lines − 9. Outside 230–340 lines, set it to 1 and nudge the MD (`A.ino:6074-6111`).
- **Mode 13 is kept** when the mode reads 0 but SOG is not bad (`A.ino:6113-6119`).
- **SOG guard (CSync RGB).** Inside 3 s windows that open on `SOG_BAD` or `SOG_SW`, count "bad HS" events where `STATUS_SYNC_PROC_HLOW_LEN` still changes, meaning the source is alive. At 17 events, SOG −1 (or `optimizeSogLevel` if more than 40 and the level is already below 2), followed by `updateSpDynamic`. After an active window, a level of 8 or more drops by one more and the phase is redone (`A.ino:6121-6188`, `B/watch.cpp:232-296`).
- **CSync misclassification retry.** With CSync, mode 0 and sync present, run the discrete-VSync probe every 150 passes (about 3 s), or every 1500 if the source was classified before. If it finds a vertical, switch to RGBHV with mode 15 (`A.ino:6218-6274`).
- **CRTC hold.** Mode 0 with sync active and `HTOTAL` < 100 for 5 passes means the 12-bit counter overflowed on a non-standard line length. All recovery stops until the mode returns, then the phase is reset and `FrameSync` is reset without recalculation. "Do NOT freeze" and "Do NOT funnel into genuineSyncLoss" (`A.ino:6276-6316`).

### 5.3 Reaction schedule on genuine sync loss (`noSyncCounter`, one step per 20 ms)

From `A.ino:6322-6563` and `B/watch.cpp:384-514`:

| count | action |
|---|---|
| 1 | freeze capture (`CAPTURE_ENABLE=0`) and return |
| 2 | if SD and SOG ≤ 1, SOG +1 |
| ≤3, or HS inactive | freeze |
| 8 | coast window defaults 0x10/0x100; SD: pre/post coast 9/9, and halve an H pulse ignore of 0x33 or more (HV) — "can early detect mode changes" |
| every 27 | `updateSpDynamic(1)`, which "needs to be first … to support SD > HDTV detection" |
| every 32 | unfreeze if HS is active, otherwise freeze |
| 34 (YPbPr) | clamp off |
| 38 | `nudgeMD()` |
| CSync, > 47, every 16 | toggle `SP_H_PROTECT` |
| ≥ 200, mode 0, HS active, once | apply the **fallback program** (`applyPresets(0)` → 480p after re-probing the input) and then probe for discrete VSync (`A.ino:6403-6441`, `A.ino:4284-4318`) |
| every 150 | at 150 and every 900: print status and probe discrete VSync (found → teardown). Then: coast/protect off, coast defaults, neutral clamp 32/48, `updateSpDynamic(1)`, nudge MD, 80 ms. HLOW_LEN activity test over 128 reads: alive → `optimizeSogLevel` (or SOG 0 every 450); dead → SOG 5. **Two consecutive "dead" probes → DAC off and sync out off**, so the display can sleep and no frozen garbage is shown. Reset SP and MD |
| every 413 | flip the input mux. If HS appears within 210 ms → teardown (to re-detect), otherwise flip back |
| ≥ 0x07FE | teardown: DAC off, `goLowPowerWithInputDetection()` (`setResetParameters` + `prepareSyncProcessor`), back to input detection (`A.ino:7498-7509`) |

### 5.4 Format change, and the "stable" branch

- **Format change** (`A.ino:6565-6659`, `B/watch.cpp:516-597`):
  1. A detected mode that is not 0 and differs from the current one (or the current one is 0) increments `newVideoModeCounter` and zeroes the stable counter.
  2. At 3: freeze, coast defaults, and if the mode now reads 0, `updateSpDynamic(1)` ("check ntsc to 480p and back").
  3. At 8: read the mode 30 times; all must equal the detected one (13 counts as stable).
  4. Then set `syncTypeCsync` from `SP_SOG_MODE` and set **`useHdmiSyncFix`** if the change crosses the 60/50 families or comes from mode 0, so the program load drops DAC and sync to make the HDMI converter re-lock.
  5. Then `applyPresets(new)`, or HD bypass for a pass-through preference.
  6. If the readings were not stable: unfreeze, and when coming from mode 0, set `noSyncCounter=0x5FF`.
  7. B ignores mode changes while `ignoreModeChangeUntil` is set by timing operations (`B/watch.cpp:522-524`, `B/watch.cpp:1281-1284`).
- **Stable branch.** It is entered when HS is stable and the mode is unchanged. It is also entered for CSync SD with no genuine loss, because the IF mode bits stay 0 for some CSync SD sources (`A.ino:6660-6673`). Per pass, `continousStableCounter`++ up to 255:

  | count | action |
  |---|---|
  | on entry after a loss of ≥150 passes | re-arm coast and phase, `FrameSync::reset`, mark a full restore |
  | 1 | unfreeze |
  | 2 | `updateSpDynamic(0)` (+ `optimizeSogLevel` on full restore); unfreeze again; re-enable DAC and sync out if they were blanked |
  | 4 | LED on |
  | 10–60, every 10 | `optimizePhaseSP()` until it succeeds |
  | 45 | clear 5_67; force a new clamp measurement |
  | every 31 | `updateSpDynamic(0)` |
  | 160 | reset the SOG-bad IRQ |
  | ≥ 3, SD 1–2, not bypass | the 240p/480i logic below |

  Sources: `A.ino:6671-6876`, `B/watch.cpp:599-754`. B at stable ≥ 3 also applies the stored picture configuration once and auto-loads a profile assigned to the standard (`B/watch.cpp:669-673`).
- **Loop-level users of the counters:**

  | when | action |
  |---|---|
  | stable ≥ 10, coast set, ≥ 500 ms since the last lock, every 5th count, H-PLL locked (`HTOTAL` within ±3 of `PLLAD_MD`) | auto best HTotal |
  | stable ≥ 7 with the mode confirmed (or CSync SD) | coast |
  | stable ≥ 4 | clamp, then unlock `SP_NO_CLAMP_REG` |
  | **done stage**, stable 36–44 (or any time when the watcher is off) | DAC on and sync out on (the "2nd chance"). If an Si5351 is present and the mode is not 14: save the internal clock code to s1_2D, enable CLK0, wait 800 µs, set `PLL648_CONTROL_01=0x75`, then `externalClockGenSyncInOutRate()` |
  | done stage missed (stable > 35) | the "3rd chance": DAC and sync on, then rate sync |

  Sources: `A.ino:9451-9540`; B: `B/gbs-control.ino:432-526`. After a watchdog or exception reset, B also queues an output "re-present" at the first done stage (`B/gbs-control.ino:508-514`).

### 5.5 240p ↔ 480i, without a program reload

Both are mode 1 (or mode 2 for 288p/576i). The watcher tells them apart by **the parity of `VPERIOD_IF`** (`A.ino:6750-6864`, `B/watch.cpp:675-751`):

| VPERIOD_IF | treated as | action |
|---|---|---|
| 522/524/526, 622/624/626 (even) | interlaced | enable the motion-adaptive deinterlacer after 2 consecutive readings (8 for CSync, "unreliable VPERIOD_IF"); scanlines off first |
| 521/523/525, 623/625/627 (odd) | progressive | disable it after the same count |

- **Glitch filter:** any change of `VPERIOD_IF` resets both counters.
- **Retiming after a toggle:** each toggle on an HV source arms `timingAdjustDelay=11`. After 11 fields of the armed parity, the firmware calls `FrameSync::reset` (when the frame lock is enabled), sets `GBS_RUNTIME_FTL_ADJUSTED=1`, then calls `externalClockGenSyncInOutRate()`.
- **A's "bob" option** (`deintMode=1`) keeps the motion-adaptive deinterlacer off and toggles scanlines instead (`A.ino:6828-6840`).
- **Scanlines** are allowed only on progressive content and while the motion-adaptive deinterlacer is inactive (`A.ino:6866-6873`, `B/scanlines.cpp:48-54`).

### 5.6 RGBHV (modes 14 and 15) branch

From `A.ino:6878-7496` and `B/watch.cpp:757-1233`:

- **Scaling decision** (A only with `preferScalingRgbhv`; B always):
  1. In mode 15, after stable ≥ 2, if the source has 535 lines or fewer and VTOTAL holds within ±3 over 30 reads 10 ms apart, set `isValidForScalingRGBHV`.
  2. Prepare the SP for HV or CSync. For HV: `ADC_5_00=0x10`, `PLL_IS=0`, `PLL_VCORST=1`, wait 320 ms ("this provides a clock for IF and test bus readings").
  3. Measure the field rate and classify:

     | condition | treated as |
     |---|---|
     | under 280 lines | NTSC-like (1) |
     | under 380 lines | PAL-like (2) |
     | rate 44–53.8 Hz | EDTV50 (4) |
     | otherwise | EDTV60 (3) |

  4. Apply that program with `SCALING_RGBHV=1`, `IF_INI_ST=16`, SOG auto polarity, and CSync VS 2/0. ICP of 6 or more drops to 5. Adopt the external clock and sync the rates.
  5. For EDTV-like sources, apply IF capture patches; a source at 69 Hz or more gets VSCALE −57.
- **Band supervision in mode 14.** The source and the loaded program are each sorted into the bands under 280, under 380, and the rest. A band mismatch confirmed over 30 reads leads to a discrete-VSync check: if the probe disagrees by more than 8 lines, rebuild from mode 15. Otherwise drop to 15 and let classification re-run. More than 535 lines reverts to bypass.
- **Stability.**
  - CSync: SOG not bad and STATUS_00 bit 2 set.
  - HV: STATUS_16 & 0x0A (only bit 1 if the VS flag is unreliable).
  - Unstable for more than 200 (CSync) or 300 (HV) passes: `setResetParameters`, `prepareSyncProcessor`, reset the SP.
  - Mode 14 also watches its settled line count (more than ±8 for 25 passes means interrupted), verifies once per program that the vertical comes from the source, and re-detects after an interruption.
- **Every 900 ms** (`A.ino:7350-7495`, `B/watch.cpp:1116-1230`):
  - mode 15: `updateHVSyncEdge()` swaps `HD_HS_ST/SP` (and VS) to follow the polarity reported in STATUS_16, and sets `SP_HS2PLL_INV_REG` (`A.ino:1062-1150`);
  - HV: SOG bad in 4 consecutive checks → discrete probe → switch to CSync only if no vertical is found;
  - measure the H-PLL rate with `getPllRate()` and classify it into 5 bands. On a change in mode 15, set KS/FS/ICP, latch, and try 4× oversampling;
  - mode 14: scanlines if the line doubler is on;
  - force a new clamp measurement.

H-PLL rate bands:

| rate | KS | FS | ICP |
|---|---|---|---|
| < 1030 | 2 | 0 | 6 |
| < 2300 | 1 | 0 | 6 |
| < 3200 | 1 | 1 | 6 |
| < 3800 | 0 | 0 | 6 |
| ≥ 3800 | 0 | 1 | 6 |

### 5.7 Interrupt housekeeping

- **Every 3 s:** `INTERRUPT_CONTROL_00` is written 0xFE then 0x00. This clears every latched IRQ except SOG-bad (`A.ino:9405-9411`, `B/gbs-control.ino:396-403`).
- **Individual clears:** each IRQ is cleared by pulsing its reset bit, `INT_CONTROL_RST_SOGBAD`, `…SOGSWITCH` or `…NOHSYNC` (`A.ino:786-802`).
- **Status flags read:** STATUS_0F bit 0 (SOG bad), bit 1 (SOG switch), bit 4 (no sync) (`B/tv5725.h:109-114`).

---

## 6. Output timing

### 6.1 A: fixed programs per output resolution

Values decoded from the program arrays by the scratch script `presettable.py`. "Clock" is the internal display PLL code in s0_41 and its frequency. "Hz" is clock / (H × V):

| program | id | clock | HTOTAL | VTOTAL | HSCALE | VSCALE | PLLAD_MD | Hz |
|---|---|---|---|---|---|---|---|---|
| ntsc_240p (1280x960) | 0x01 | 0xA5 162 | 2704 | 1000 | 512 | 512 | 2345 | 59.91 |
| ntsc_1280x1024 | 0x02 | 0xA5 162 | 2535 | 1066 | 512 | 485 | 2345 | 59.95 |
| ntsc_1280x720 | 0x03 | 0x85 108 | 2402 | 750 | 512 | 683 | 2345 | 59.95 |
| ntsc_720x480 | 0x04 | 0x65 81 | 2573 | 525 | 512 | byp | 2345 | 59.96 |
| ntsc_1920x1080 | 0x05 | 0x85 108 | 1601 | 1125 | byp | 455 | 2558 | 59.96 |
| ntsc_downscale | 0x06 | 0x25 40.5 | 2558 | 263 | 512 | byp | 2559 | 60.2 |
| pal_240p (1280x960) | 0x11 | 0x85 108 | 2166 | 1000 | 607 | 585 | 2269 | 49.86 |
| pal_1280x1024 | 0x12 | 0x85 108 | 2032 | 1066 | 585 | 512 | 2269 | 49.86 |
| pal_1280x720 | 0x13 | 0x65 81 | 2165 | 750 | 626 | 819 | 2341 | 49.89 |
| pal_768x576 | 0x14 | 0x65 81 | 2599 | 625 | 512 | 1023 (not bypassed) | 2345 | 49.87 |
| pal_1920x1080 | 0x15 | 0x65 81 | 1444 | 1125 | byp | 512 | 2553 | 49.86 |
| pal_downscale | 0x16 | 0x25 40.5 | 2578 | 313 | 512 | byp | 2553 | 50.19 |

`byp` means the scale bypass bit is set, with the value at 1023.

- **Clock codes.** The mapping from code to frequency is 0x25 = 40.5, 0x45 = 54, 0x55 = 64.8, 0x65 = 81, 0x85 = 108, 0x95 = 129.6 and 0xA5 = 162 MHz; 0x35 = clock unused (bypass); 0x75 = external CKIN; 0 = no program loaded (`A.ino:351-374`). Every frequency is 648 MHz divided by an integer. The sub-fields of s0_41 are `PLL_VS27`, `PLL_VS4`, `PLL_2XV` and `PLL_4XV` (`B/tv5725.h:742-745`).
- **Every program runs slightly slow** of 60 or 50 Hz. Auto best HTotal then trims HTOTAL to the measured source, and the frame lock adds 2 lines when needed (section 7).
- **Program selection** (`applyPresets`, `A.ino:4243-4491`):

  | preference value | program |
  |---|---|
  | 0 | 960 |
  | 1 | 480/576 |
  | 2 | custom from flash |
  | 3 | 720 |
  | 4 | 1024 |
  | 5 | 1080 |
  | 6 | downscale |
  | 10 | bypass |

  Further rules:
  - modes 1/3/8/9/14 use the NTSC programs and modes 2/4 the PAL ones;
  - "match preset source": NTSC with a 1024 preference gets 960, and PAL with a 960 preference gets 1024;
  - PAL-force-60 maps 2→1 and 4→3;
  - modes 5/6/7/13 go to HD bypass and mode 15 to RGBHV bypass;
  - leaving bypass or mode 0 releases the IF, VDS and DEC resets early and adds a 400 ms wait "so that autobesthtotal works first attempt".

### 6.2 Scaling and timing registers (VDS, segment 3)

- **Totals.** `VDS_HSYNC_RST` s3_01[0:12] is H total; `VDS_VSYNC_RST` s3_02[4:11] is V total.
  - A writes the total value (`set_htotal`, `A.ino:2687-2709`).
  - **B treats the register as total − 1:** `timingApply` writes `htotal-1` and `timingReadCurrent` adds 1 (`B/timing.cpp:394-395`, `B/timing.cpp:442`, `B/timing.cpp:452`).
- **Sync positions:** `VDS_HS_ST/SP` (s3_0A/0B), `VDS_VS_ST/SP` (s3_0D/0E). A negative sync is expressed as ST > SP (`A.ino:2719-2725`, `A.ino:3135-3145`).
- **Two blanking windows.** `VDS_HB_ST/SP` and `VB_ST/SP` are the "memory" blank, the fetch window (s3_04/05/07/08). `VDS_DIS_HB_ST/SP` and `DIS_VB_ST/SP` are the "display" blank, where the DAC shows active video (s3_10/11/13/14). The DIS stop is where the picture starts and the DIS start is where it ends. Shifting the picture moves the memory window against the display window (`shiftHorizontal`, `A.ino:2016-2055`).
- **Scale factors.** `VDS_HSCALE` s3_16[0:10] and `VDS_VSCALE` s3_17[4:10] hold 1024 × source size / output size, so 512 means 2× and 683 means 1.5×. Bypass bits are `VDS_HSCALE_BYPS` s3_00 bit 4 and `VDS_VSCALE_BYPS` bit 5; 1023 with bypass means 1:1. The source vertical size is in *IF output lines*: for 15 kHz the line doubler has already doubled them. Manual limits: HSCALE between 256 and 1023 (`A.ino:2132-2244`), VSCALE between 128 and 1023 (`A.ino:2374-2408`).
- **Rules A follows for the other registers:**
  - `VDS_EXT_HB/VB` (s3_6D–72) are mirrored from the display blank;
  - `VDS_VSYN_SIZE1/2` (s3_20/22) = V total + 2;
  - `VDS_FRAME_RST` = 4, `FRAME_NO` = 1, `FR_SELECT` = 1 (`A.ino:3990-4000`).
- **Changing H total at runtime** (`applyBestHTotal`, `A.ino:2985-3233`): move all four blank edges by half the difference, round them to even, clamp any overflow to fixed fractions of the new total, and fix HS if it would spill past the display-blank stop. Write only between fields: wait for `STATUS_VDS_FIELD` 1→0, then 0→1, with timeouts.
- **Manual helpers in A:**
  - `set_vtotal`: VB = 1.6 % of total, display VB 2 below total, VS 1..5, swapped for totals under 530 or near 795/806 (`A.ino:2711-2738`);
  - the `w` console command sets individual positions (`A.ino:9107-9190`);
  - `snapToIntegralFrameRate` steps HTOTAL until the output is closest to 50 or 60 Hz (`A.ino:5837-5919`).
- **Input-formatter capture window (segment 1).** The VDS reads from what the IF captured:
  - `IF_HB_ST2`/`IF_HB_SP2` (1_18/1_1A): horizontal capture start/stop in ADC samples;
  - `IF_HBIN_ST/SP` (1_24/1_26), `IF_HB_ST/SP` (1_10/1_12): line-doubler and deinterlace offsets;
  - `IF_HSYNC_RST`/`IF_LINE_SP` (1_0E/1_22): line length;
  - `IF_VB_ST/SP` (1_1C/1_1E): vertical capture start; moving it moves the picture vertically (`shiftVerticalUpIF`, `A.ino:2466-2510`).

  A has a formula version (`setIfHblankParameters`, `A.ino:2776-2822`) that is commented out in favour of the per-program constants.

### 6.3 B: one base program plus computed timing

- **`baseProgram` is A's `ntsc_1920x1080` with three bytes changed** (diffed with the scratch script `mech/basediff.py`):
  - s3_26 bit 6 set: `VDS_D_RAM_BYPS`, line filter off;
  - s3_2B bit 7 set: `VDS_UV_STEP_BYPS`;
  - s3_4E bit 0 set: `VDS_PK_Y_H_BYPS`, H peaking off.

  It is used for every scaled input (`B/program.cpp:994-997`).
- **Default mode** (`B/timing.cpp:839-855`). After the program loads, `timingTick()` measures the source field rate, sets **pixel clock = source Hz × (HTOTAL+1) × (VTOTAL+1)** on the Si5351 (within 20–162 MHz), and resets the path. Then `defaultModeFinish()` (`B/timing.cpp:664-680`):
  1. writes the FIFO line flags (`PB_MAST_FLAG_REG` 45, `PB_GENERAL_FLAG_REG` 0, `RFF_MASTER_FLAG` 36, `RFF_GENERAL_FLAG` 52);
  2. calls `applySourceAspect()` and `ifCentreCapture()`;
  3. applies the user overrides;
  4. queues a re-centre after 4 s;
  5. queues an output re-present.
- **Requested modes** (`timingCompute`, `B/timing.cpp:205-283`):
  - **CVT-RB style:** H blank 160, H sync 32, V front porch 3, V sync 4–7 by aspect (10 if unknown), back porch at least 6, V blank at least 460 µs (`B/timing.cpp:40-66`, `B/timing.cpp:85-127`);
  - **CVT:** duty-cycle H blank, V blank at least 20 (`B/timing.cpp:129-163`);
  - **manual:** totals supplied.

  The clock is rounded down to 250 kHz. Limits: HTOTAL ≤ 4095; VTOTAL ≤ 2047 and refused above 1600 ("what the bench probed"); clock 20–162 MHz ("ceiling of the TV5725's own DAC"). A warning appears below 104.5 MHz, where "the bench's converter went black".

  Windows (`deriveWindows`, `B/timing.cpp:165-194`):
  - HS at 8;
  - the display window is centred in the blank;
  - the memory blank sits 4 pixels outside the display blank;
  - VS starts at line 1;
  - VB = VS stop + 1, rounded to even, with VB_SP = VB_ST + 2.
- **Scale** (`timingComputeScale`, `B/timing.cpp:318-386`):
  - **Clock floor:** if the output pixel clock is below **1.2 × the input sample rate** (source HTOTAL × VTOTAL × rate), H total is widened. "This engine refuses below 1.20x the input's."
  - **Scale values:** HSCALE = source width × 1024 / new window, where the source width comes from the measured natural width or the old scale × old window. VSCALE = source lines (480/576 for SD) × 1024 / new window. 1023 or more means bypass.
- **Applying a mode** (`timingTick`, `B/timing.cpp:833-983`):
  1. Hold mode watching and arm a 20 s register snapshot for automatic revert.
  2. Write the base program, then run the post-load.
  3. `timingApply`: optionally wait for blanking (VERT_COUNT wraps), then write in order the H set, the V set, `VSYN_SIZE`, EXT and the scale (`B/timing.cpp:419-490`).
  4. Path reset: `resetDigital`, `resetPLLAD`, `PLLAD_LEN`, `ResetSDRAM`, SOG latch (`B/program.cpp:1474-1486`).
  5. Repeat the whole program/post-load/apply sequence for "passes" (default 2) with 4 s gaps.
  6. Ramp the pixel clock in 250 kHz steps, 3 per tick.
  7. Re-centre after 4 s.

  Integer scaling (multiplier 1–4 vertically, horizontal stretch 100–400 %) rewrites only the windows and scales (`B/timing.cpp:502-634`).
- **Geometry** (`B/geometry.cpp`):
  - `applySourceAspect()` (`B/geometry.cpp:442-553`): from the captured natural width, compute HSCALE for 4:3 or full width with zoom, centre the window, and apply trims, keeping the memory and EXT edges at their program offsets from the display edges.
  - `ifCentreCapture()` (`B/geometry.cpp:555-583`): position `IF_HB_SP2` and `IF_VB_ST/SP` so that the active-video centre lands in the middle of the output window. It uses a capture anchor corrected by the settled HS low width.
  - `vscaleForSource()`: the program's VSCALE × (480 or 576) / 480 (`B/geometry.cpp:326-337`).

### 6.4 Clock path

| path | how | used by |
|---|---|---|
| internal PLL648 | code in s0_41 (6.1); `PLL_CKIS=0` (oscillator reference), `PLL_MS` (memory clock 2 or 7 in programs) | every A program at load; B when no Si5351 is present |
| external Si5351 | CLK0 → CKIN, `PAD_CKIN_ENZ=0`, s0_41 = 0x75 | A after the done stage (`A.ino:9513-9527`); B at the done stage and by watchdog |
| ADC sampling PLL (PLLAD) | `PLLAD_MD` (s5_12[0:12]) = samples per input line, the feedback divider locked to HS; `PLLAD_KS` post-divider (0: > 87 MHz … 3: < 23 MHz); `PLLAD_CKOS`, `ICP`, `FS` gain, `R/S` | always |

- **Rate matching on the external clock.** `externalClockGenSyncInOutRate()` measures the source field rate (sfr) and the output frame rate (ofr), both required to be 47–86 Hz. It stores the ratio clock/ofr for the frequency lock, then sets **clock × sfr / ofr** smoothly (`A.ino:396-445`, `B/displayclock.cpp:113-175`).
- **Re-arming on each program load.** `externalClockGenResetClock()` converts the program's internal code into the Si5351 frequency and clears the stored ratio (`A.ino:344-394`).
- **B's display-clock watchdog.** If the chip has stayed on the internal clock for 10 s while an Si5351 exists, it adopts the external clock. Once the external clock has been back for 2 s, it re-presents the output: DAC off and sync off for 3 s, DAC on, sync off 1 s, sync on, so the sink re-acquires (`B/timing.cpp:733-831`).

### 6.5 Bypass modes

**HD bypass** (`setOutModeHdBypass`, `A.ino:5196-5491`; `B/program.cpp:1024-1297`). Used for modes 5/6/7/13, or as pass-through by preference.

1. Load the HD-bypass section and clear the debug view.
2. Resets 0x46 = 0x47 = 0, phase units enabled, `GBS_PRESET_ID=0x21`; run the post-load, which returns early for bypass.
3. Then:
   - `OUT_SYNC_SEL=1` (sync from the HD bypass);
   - `PLL_CKIS=0`, `PLL_DIVBY2Z=0`, `PAD_OSC_CNTRL=1` ("wave pattern in 720p source, this fixed it");
   - s0_41 = 0x35, `PLL_LEN=1`;
   - DAC channel enables, `PAD_TRI_ENZ=1` (tri-state the memory bus to cut noise), `PLL_MS=2`, `MEM_PAD_CLK_INVERT=0`;
   - 0x47 = 0x1F;
   - **`DAC_RGBS_BYPS2DAC=1`** ("the real use of HDBypass"), `SP_HS_LOOP_SEL=1`;
   - **`PB_BYPASS=1`**;
   - `PLLAD_MD=2345`, `KS=2`, oversampling 2×, `ICP=5`, `FS=1`.
4. Colour: YPbPr gets `HD_MATRIX_BYPS=0` and `HD_DYN_BYPS=0` (convert to RGB in the bypass); RGB bypasses both. `HD_SEL_BLK_IN=0`.
5. Per-mode `HD_*` H/V blank and sync positions and CSync VS start/stop:
   - **SD:** sync comes from the SP (`OUT_SYNC_SEL=2`) with `SP_HS2PLL_INV_REG=1` (lock to the falling HS edge), `SP_CS_P_SWAP`, MD flips, `HD_HSYNC_RST = MD/2 + 8`;
   - **720p:** `MD=2474`, `KS=0`, 1×;
   - **1080p:** `MD=2749`;
   - **mode 13:** `MD=512`, KS by line count.
6. Phases: SP 8, ADC 24. SOG latch.
7. Wait up to 2002/1502 ms for HS and mode, `updateSpDynamic`, DAC and sync on, 200 ms, `optimizePhaseSP`.

**RGBHV bypass** (`bypassModeSwitch_RGBHV`, `A.ino:5493-5646`; `B/program.cpp:1299-1435`).

1. DAC and sync off. Load the HD section, reset the clock, apply the RGB patches.
2. Clocks: `PLL_CKIS=0`, `DIVBY2Z=0`, **`PLL_ADS=0`**, `PLL_MS=2`, tri-state, s0_41 = 0x35, `PLL_LEN=1`.
3. Data and sync path: **`DAC_RGBS_ADC2DAC=1`** (ADC straight to the DAC), `OUT_SYNC_SEL=1`, `SFTRST_HDBYPS_RSTZ=1`, `HD_INI_ST=0`, HD matrix and dynamic bypassed, sync input filters on (`PAD_SYNC1/2_IN_ENZ=0`), SOG auto polarity.
4. Sync type:
   - **HV:** SOG off, HV connected, no coast, ignore 0xFF, auto H/V polarity, `SP_HS_LOOP_SEL=1`;
   - **CSync:** SOG on, HV disconnected, coast 4/7, `H_PROTECT`.
5. Clamp manual, sub-coast off, `KS=1`, oversampling 2×, DEC matrix bypassed, ADC filter 0 (150 MHz), `ICP=4`, `FS=0`, **`PLLAD_MD=1856`** ("allows lower res to detect").
6. Resets: `resetDigital`, `resetSyncProcessor`, `ResetSDRAM`, `resetPLLAD`, `togglePhaseAdjustUnits`, then `PLLAD_LEN`.
7. DAC and sync on, phase latches, auto gain, `GBS_PRESET_ID=0x22`.

The H-PLL is retuned later by the watcher's band logic (5.6).

**Output format** (A only, `OutputComponentOrVGA`, `A.ino:919-942`): component output sets `VDS_SYNC_LEV=0x80`, `VDS_CONVT_BYPS=1` (YUV out) and `OUT_SYNC_CNTRL=0`. VGA output sets 0 / 0 / 1. B always outputs RGB with sync (`B/videopath.cpp:114-126`).

---

## 7. Frame lock (`framesync.h`)

### 7.1 Measurement hardware and ISR

- **Signal path.** The TV5725 debug-out pad is enabled by `PAD_BOUT_EN` and drives one selected internal signal to the ESP pin **D6/GPIO12** (`DEBUG_IN_PIN`, `A.ino:164`). The firmware chooses the signal with the test bus:

  | settings | signal |
  |---|---|
  | `TEST_BUS_SEL=0` with `IF_TEST_SEL=3` | input vertical period |
  | `TEST_BUS_SEL=2` with `VDS_TEST_BUS_SEL=0` | output VBlank |
  | `TEST_BUS_SEL=0xA` with `TEST_BUS_SP_SEL=0x0F` | SP vsync after SOG separation |
  | `TEST_BUS_SP_SEL` 0x6B (CSync) or 0x09 (HV) | H-PLL rate |

  Sources: `A.ino:3235-3368`, `A/framesync.h:165`, `A.ino:2740-2751`.
- **Period capture** (`MeasurePeriod`, `A/framesync.h:32-67`, `B/framesync.cpp:3-42`):
  1. Attach a RISING interrupt on D6.
  2. The first ISR stores the CPU cycle counter (`rsr ccount`) as the start, detaches itself, and attaches the second ISR.
  3. The second ISR stores the stop and detaches.
  4. Both ISRs live in IRAM and run with interrupts masked.
  5. The caller polls for the stop. Period = stop − start in CPU cycles; at 160 MHz, Hz = 160e6 / period.
- **A's poll** (`A/framesync.h:120-148`, `A/framesync.h:376-404`): disable the watchdog and loop up to 3,000,000 times. After the first edge, `delay(7)` and **switch WiFi to light sleep**, presumably to cut interrupt jitter; the code does not say why. Restore no-sleep afterwards.
- **B's poll:** a 100 ms cycle-count deadline, with an explicit detach (`B/framesync.h:46-72`, `sampleTimeoutMs`, `B/framesync.h:366`).

### 7.2 Best HTotal: the coarse lock without an Si5351

From `findBestHTotal`/`init`, `A/framesync.h:198-322`; caller `runAutoBestHTotal`, `A.ino:2862-2983`.

1. **Measure.** Measure the input frame period, switch the test bus to VDS, and measure the output period. The phase is the delay from input to output start, modulo the input period (`A/framesync.h:152-182`).
2. **Compute.** If the two periods differ by more than 4 cycles, best HTOTAL = HTOTAL × inPeriod / outPeriod, computed in 64-bit. Otherwise keep it. The intent, in the comment: get the output frame "as close to the input as possible while still being less. Increasing the vertical frame size slightly should then push the output frame time to being larger" (`A/framesync.h:310-314`).
3. **Guards** (`A.ino:2869-2951`): 64 stable HS checks before, SOG not bad, mode unchanged; 16 and 24 more stable checks after; a result above 4095 is refused unless forced.
4. **Apply** with `applyBestHTotal`. A difference above 6 % of HTOTAL (16 % for med-res and scaled RGBHV) is "large": retry up to 8 times, then give up and disable auto HTotal.
5. **On a failed run,** reset the frame lock and decrement `syncLockFailIgnore` from 16. At 0, force DAC and sync on and stop trying (`A.ino:2956-2972`).

### 7.3 Phase lock by VTOTAL, without an Si5351 (`runVsync`)

From `A/framesync.h:411-481` and `B/framesync.h:269-337`:

- **Constants:** target phase **90°** (output trails input by a quarter frame); correction **2 lines**; lock interval **100 × 16.70 ms = 1670 ms** (`A.ino:334-342`, `B/framesync.h:362-369`).
- **Algorithm, a two-state controller:**
  1. Skip the first 2 calls after `init`.
  2. Measure the period and the phase.
  3. If phase > target, correction = 0. Otherwise correction = +2 lines (a longer frame lets the output fall behind).
  4. Apply only the change from the last correction: `VDS_VSYNC_RST += delta`, and in **method 0** also `VDS_VS_ST += delta` (sync stays at the frame end). **Method 1** changes VTOTAL only.
  5. Write VS_ST after `STATUS_VDS_FIELD` goes 1→0, then VSYNC_RST after it goes 0→1, with timeouts (`A/framesync.h:466-472`).
- **`reset()`** undoes the outstanding correction in the same field-timed way and clears the ready flag, which makes `runAutoBestHTotal` re-measure (`A/framesync.h:252-298`).
- **Precondition:** `VDS_VS_ST` must be at least 1 in every program. The firmware does not enforce it and only warns: "make sure to have all presets follow the rule" (`A.ino:778-782`).
- **Loop conditions** (`A.ino:9358-9403`, `B/gbs-control.ino:356-394`): enabled; source connected; auto HTotal on; ready; interval elapsed; stable > 20; `noSync`==0; H-PLL locked (`HTOTAL` within ±3 of `PLLAD_MD`). The test bus is forced to 0 for the measurement and restored afterwards. 16 consecutive failures cause a reset.

### 7.4 Frequency lock with an Si5351

- **A** (`runFrequency`, `A/framesync.h:503-672`):
  1. Requires the stored clock/fps ratio, `PAD_CKIN_ENZ=0`, s0_41 = 0x75, not bypass, and ready.
  2. Measure the input fps twice: through `vsyncPeriodAndPhase`, then `getPulseTicks`. Both must be 47–86 Hz and agree within 0.5 fps and 0.833 %. Two attempts are allowed.
  3. Latency error in frames = (phase − target) / CPU Hz × fps.
  4. Correction = 0.0038 × error, where 0.0038 is 2/525, "the difference between SNES and Wii 240p". Clamp to ±0.06 % against the input fps, because some LCDs lose sync on 0.1 % steps.
  5. Clamp again to ±0.06 % against the previous output fps.
  6. New clock = ratio × fps_out, applied with the 1 kHz smoothing.

  It runs at the same 1670 ms interval. If the ratio is missing, the rate sync is retried every 5 s (`A.ino:9373-9386`).
- **B has no `runFrequency`.** With an Si5351 present, B only re-establishes the rate ratio (`externalClockGenSyncInOutRate`) when it is missing, every 5 s (`B/gbs-control.ino:372-376`), after deinterlace toggles, and from the default-mode clock. There is no continuous phase correction.

---

## 8. Deinterlacer, scanlines, peaking and sharpness, colour

### 8.1 Motion-adaptive deinterlacer

**Enable** (`A.ino:5778-5812`, `B/videopath.cpp:603-631`):

1. Freeze capture.
2. `DEINT_00=0x19` (angular bypassed), `MADPT_Y_MI_OFFSET=0`, `MADPT_Y_MI_DET_BYPS=0` (automatic motion index).
3. `MADPT_VTAP2_COEFF` = 6 for NTSC, 4 for PAL.
4. Reader FIFO: `RFF_ADR_ADD_2=1`, `RFF_REQ_SEL=3`, `RFF_FETCH_NUM=0x80`, `RFF_WFF_OFFSET=0x100`, `RFF_YUV_DEINTERLACE=0`, `WFF_FF_STA_INV=0`.
5. **`WFF_ENABLE=1` before `RFF_ENABLE=1`.**
6. Unfreeze, wait 60 ms, then `MAPDT_VT_SEL_PRGV=0`.

**Disable** (`A.ino:5814-5835`):

1. `VT_SEL_PRGV=1`, `DEINT_00=0xFF`.
2. `RFF_FETCH_NUM=1` and `RFF_WFF_OFFSET=1` (the "RFF disable fix"), 2 ms.
3. WFF and RFF off. The comment warns this "can cause mem reset requirement, procedure above should fix it".
4. `WFF_FF_STA_INV=1`, MI offset 0x7F, MI detect bypassed.

Supporting pieces:

- **The defaults** come from the 64-byte segment-2 section (2.5).
- **Line doubler:** the input formatter does the line doubling for 15 kHz (`IF_LD_RAM_BYPS=0`), and EDTV bypasses it (`A.ino:3527-3530`).
- **Downscale** uses the deinterlacer's vertical scaler: `MADPT_Y/UV_VSCALE_BYPS=0`, `MADPT_VSCALE_DEC_FACTOR=1`, `MADPT_PD_RAM_BYPS=0`, `MADPT_SEL_PHASE_INI` (`A.ino:3606-3631`).

### 8.2 Scanlines (the motion-index mixer used as a line darkener)

**A, enable** (`A.ino:5709-5748`):

1. UV deinterlacer on: `MADPT_UVDLY_PD_SP/ST=0`, `MADPT_EN_UV_DEINT=1`, `MADPT_UV_MI_DET_BYPS=1`, `MADPT_UV_MI_OFFSET` = strength, `MADPT_MO_ADP_UV_EN=1`.
2. `DIAG_BOB_PLDY_RAM_BYPS=0`, `MADPT_PD_RAM_BYPS=0`, `RFF_YUV_DEINTERLACE=1`, `MADPT_Y_MI_DET_BYPS=1`.
3. White level: `VDS_WLEV_GAIN=0x08` and `VDS_W_LEV_BYPS=0`, which compensate brightness.
4. `MADPT_VIIR_COEF=0x08` with VIIR on, `MADPT_Y_MI_OFFSET` = strength.
5. `RFF_LINE_FLIP=1` ("clears potential garbage in rff buffer"), `MAPDT_VT_SEL_PRGV=0`.
6. Set flag s1_2C bit 0.

A's disable restores fixed values (`A.ino:5750-5776`). Strength is written directly as the MI offset: default 0x30; the OSD steps it by 0x10, and 0x50 means off (`A/OSDManager.cpp:43-68`).

**B** (`B/scanlines.cpp:56-186`):

- B snapshots the program's values of every touched field, restores them on disable, and keeps the Y gain and offset.
- Offset = 0x7F − strength, so a larger value means darker.
- Active only for SD 15 kHz progressive (odd `VPERIOD_IF`) with the motion-adaptive deinterlacer off.

### 8.3 Peaking, sharpness, filters

**A, at post-load** (`A.ino:3549-3558`, `A.ino:3872-3903`):

- peaking cores `VDS_PK_LB_CORE`/`LH_CORE` = 0 (1 counters source jailbars);
- **compression `VDS_PK_LB_CMP`/`LH_CMP` = 31** ("0 cancels peaking entirely");
- **gains `VDS_PK_LB_GAIN`/`LH_GAIN` = 0x14**;
- `VDS_PK_VL_HL_SEL` and `VL_HH_SEL` = 0;
- `VDS_STEP_GAIN=1` (step response);
- H peaking on/off through `VDS_PK_Y_H_BYPS` (default on);
- line filter `VDS_D_RAM_BYPS` (default off);
- UV step `VDS_UV_STEP_BYPS` (not for 1080p);
- `VDS_TAP6_BYPS=0`.

The "debug view" command raises the gains to 0x3F and adds +0x24 to the Y offsets (`A.ino:8469-8507`).

**B:** gains **0x16 (LB) / 0x0A (LH)**, CMP untouched, H peaking bypassed in the base program (`B/program.cpp:615-622`). User overrides go through `pictureApply()`, together with the colour controls in 8.4 (`B/picture.cpp:115-169`).

### 8.4 Colour path and controls

- **RGB input** (`applyRGBPatches`, `A.ino:1009-1030`, `B/videopath.cpp:144-158`):
  - ADC clamps to ground on R, G and B (`ADC_RYSEL_R/G/B=0`);
  - `DEC_MATRIX_BYPS=0`: the decimator converts RGB to YUV;
  - `IF_MATRIX_BYPS=1`;
  - VDS Y gain 0x80 (unity), UCOS 0x1C, VCOS 0x29 ("0x27 when using IF matrix, 0x29 for DEC matrix"), offsets 0.
- **YPbPr input** (`applyYuvPatches`, `A.ino:976-1006`, `B/videopath.cpp:128-142`):
  - R and B clamp to mid level, G to ground;
  - DEC matrix bypassed;
  - Y offset 0xFE, U/V offset 0x03, same gains.
- **Clamp patches are also applied during the program load** (5_03, 2.4).
- **A's controls:**
  - brightness `VDS_Y_OFST` (signed, step 8);
  - "contrast" = the ADC gain, inverted, step 8 (OSD, `A/OSDManager.cpp:13-28`, `A/OSDManager.cpp:151-174`);
  - component colour mixing (`A.ino:944-951`).
- **B's PictureConfig** (`B/picture.cpp:115-169`): brightness (`VDS_Y_OFST`), contrast (`VDS_Y_GAIN`), U/V gain (`VDS_UCOS/VCOS_GAIN`), black-level expansion (`VDS_BLEV_LEVEL/GAIN/BYPS`), and the peaking gains, coring and bypass.
- **B's "phosphor" recipes** use colour-tag corrector C1 or C2: U window 0x80..0x7F, Y threshold 0xFF, slopes, gain 7 or 12 (`B/picture.cpp:81-113`).
- **B's base-value rule:** every value can be "keep", which falls back to the value captured after the program load (`B/picture.cpp:55-73`).

---

## 9. OSD (the TV5725's built-in icon OSD)

- **What it is.** The chip has a hardware menu with 8 icon slots and one bar graph; there are no fonts or text. Every register is in s0_90–s0_98 (`B/tv5725.h:593-613`). They are listed below.
- **Write protocol.** Each update is bracketed: `OSD_COMMAND_FINISH=0`, then the writes, then `OSD_COMMAND_FINISH=1`. The OSD is held in reset (`OSD_SW_RESET=1`) while off (`A/OSDManager.h:203-232`). It is also part of the soft reset in 0x46 bit 5.
- **Initialisation** (`Menu::init`, `A/osd.h:150-169`, identical to B's `osdHardwareInit`, `B/OSDManager.cpp:134-152`; run at every post-load):
  1. reset on;
  2. colours: font white on black, border blue, selection green on magenta;
  3. RGB format;
  4. position (500 >> 3, 400 >> 3);
  5. vertical style;
  6. 3× zoom;
  7. display and menu off;
  8. reset off.
- **`OSDManager`** (`A/OSDManager.h:59-279`, `A/OSDManager.cpp:13-189`):
  - **positioning:** relative to the VDS display window, centred horizontally and placed above the bottom edge, with zoom 5×/4× (480p 7×/2×, 1080p 3×/8× by a `preset` member that **nothing ever sets**, `A/OSDManager.h:115`, `A/OSDManager.h:168-202`);
  - **navigation:** cursor moves over the registered icons;
  - **"enter":** shows the bar for the item (`OSD_MENU_MOD_SEL`, bar length 128, foreground = value);
  - **handlers:** brightness, contrast, move X/Y (the shift functions) and scale X/Y. Scanlines and line filter exist but are disabled as "confusing" (`A/OSDManager.cpp:176-189`).
- **How it is reached.** Only from the OLED menu's "OSD" item. It freezes the OLED menu, forwards the encoder as LEFT/RIGHT/ENTER, and times out after `OSD_TIMEOUT` = 8000 ms (`A/OLEDMenuImplementation.cpp:343-401`, `A/OLEDMenuConfig.h:21-23`).
- **Limits:** the 8 predefined glyphs; one bar; positions in 8-pixel units; 3-bit colours; 1–8× zoom.

Register map, s0_90–s0_98:

| register | fields |
|---|---|
| 0_90 | `OSD_SW_RESET`, `HORIZONTAL_ZOOM` (3 bits, 1–8×), `VERTICAL_ZOOM` (2 bits), `DISP_EN`, `MENU_EN` |
| 0_91 | `MENU_ICON_SEL` and `MENU_MOD_SEL`, 4-bit icon codes: 1 brightness, 2 contrast, 3 hue, 4 volume, 8 up/down, 9 left/right, A vertical size, B horizontal size (`A/OSDManager.h:62-71`) |
| 0_92–0_93 | 3-bit colours for bar font fore/back, border, selection fore/back; `COMMAND_FINISH` |
| 0_94 | `DISP_STYLE` (vertical/horizontal), `YCBCR_RGB_FORMAT` |
| 0_95–0_96 | start position in units of 8 pixels/lines |
| 0_97–0_98 | `BAR_LENGTH`, `BAR_FOREGROUND_VALUE` |

---

## 10. OLED menu and rotary encoder

- **Display.** SSD1306, 128×64, at 0x3C on the shared bus. It is flipped vertically at init (`A.ino:7767-7768`). Refresh is every 50 ms, the screensaver starts after 180 s, and the status bar is ¼ of the height (`A/OLEDMenuConfig.h:9-28`, tick `A/OLEDMenuManager.cpp:359-407`).
- **Pins** (`A.ino:37-39`, `B/board.h:22-24`):
  - CLK = D5/GPIO14;
  - DATA = D7/GPIO13;
  - switch = D3/GPIO0, which must be high at boot or the ESP fails to boot.

  All three are `INPUT_PULLUP`. Both builds read the pins fast, straight from the GPIO_IN register (`A.ino:176`, `B/board.h:35`).
- **Rotate ISR** (FALLING edge on CLK; `A.ino:7728-7752`, `B/OLEDMenuImplementation.cpp:881-905`):
  1. Ignore edges within **150 ms** (A) / **75 ms** (B) of the last one.
  2. Direction = the DATA level.
  3. A direction reversal within **120 ms** (A) / **60 ms** (B) of the last accepted step is dropped as a misread, and nav becomes IDLE.
  4. Otherwise set the volatile `oledNav` and increment `rotaryIsrID`.
- **Push ISR** (FALLING edge on the switch): 500 ms lockout, then ENTER (`A.ino:7753-7762`).
- **Consumption.** `loop()` ticks the menu with `oledNav`, and resets it to IDLE only if `rotaryIsrID` did not change during the tick, so no event is lost (`A.ino:8376-8384`, `B/gbs-control.ino:346-354`).
- **Direction flags.** `REVERSE_ROTARY_ENCODER_FOR_OLED_MENU` and `…FOR_OSD` are 0 in A and 1 in B (`A/OLEDMenuConfig.h:21-22`, `B/OLEDMenuConfig.h:20-21`). B also accepts navigation from the web API (`B/OLEDMenuImplementation.cpp:918-934`).
- **Menu contents.**
  - **A:** OSD, resolutions, presets, misc/reset, current settings, WiFi (`A/OLEDMenuImplementation.cpp:402+`).
  - **B:** input, timing (8 computed modes), profiles, tuning field groups (input, sync, deinterlace, picture, geometry), pause, OSD, sensors, network, reset (`B/OLEDMenuImplementation.cpp:828-878`, modes `B/api_contract.gen.h:120-130`).

---

## 11. Web/API and preference persistence (brief)

### 11.1 A

- **Network services.**
  - `AsyncWebServer` on port 80 serves a gzipped single page (`A.ino:10246-10253`).
  - A WebSocket on port 81 pushes status about once a second and mirrors the serial log. It stands down below 12 kB of free heap and disconnects below 8 kB (`A.ino:253-324`, `A.ino:8319-8346`).
  - WiFi is managed by PersWiFiManager: AP "gbscontrol"/"qqqqqqqq", mDNS, optional OTA (`A.ino:127-162`).
- **Control.** `/sc?<char>` injects a serial command character and `/uc?<char>` a "user" command character, refused below 10 kB of free heap (`A.ino:10255-10286`). Resolution changes (`/uc` f/g/h/p/s/L) set `useHdmiSyncFix`, call `applyPresets`, then save the preferences (`A.ino:9767-9801`). Other routes handle slots, files on the flash, WiFi and the hostname (`A.ino:10303-10660`).
- **Preferences.** `/preferencesv2.txt` holds 19 ASCII bytes, each value + '0', with the slot letter in position 3 (`A.ino:11019-11050`; load `A.ino:7920-8001`). **Bug:** `scanlineStrength` (default 0x30) is stored as value + '0', and the load-time range check assigns its fallback to `enableCalibrationADC` instead (`A.ino:7993-7995`).
- **Custom presets.**
  - Files `/preset_<std>.<slot>` (ntsc, pal, ntsc_480p, pal_576p, ntsc_720p, ntsc_1080p, medium_res, vga_upscale, unknown) hold the 432 program values as CSV (`A.ino:10799-10960`). Slot metadata lives in `/slots.bin`.
  - Saving marks `GBS_PRESET_CUSTOM`, removes scanlines and undoes the frame-lock correction first (`A.ino:10952-10960`).

### 11.2 B

- **REST JSON API** under `/api/…` (`B/api.cpp:341+`, `B/network.cpp:250-278`):
  - registers by name (`/api/get`, `/api/set`, `/api/find`, `/api/regs`, `/api/dump`), through the generated registry of 958 fields with flags (read-only, per-mode, risk) (`B/registry.h`, `B/registry.gen.h:10-30`);
  - timing compute/apply/integer;
  - profiles, journal, captures, command runner, board files and firmware.
- **Preferences.** `/prefs.bin`: magic 0x4F534247, version 1, count, then 5 bytes per entry (FNV-1a hash of the name, value). Unknown hashes are ignored and values above the maximum fall back to the default. Writes are deferred by 3 s, and the side effects run one per tick (`B/prefs.cpp:18-262`, `B/prefs.h:6-8`).
- **Other stored state.**
  - user register overrides in `/user.cfg` (up to 64 entries, keyed by field-name hash; `B/userconfig.cpp:6-8`, apply at 210-219);
  - 8 profiles in `/p/<n>` with per-standard auto-assignment in `/p/assign.bin` (`B/profile.cpp:11-43`, `B/profile.h:6-8`).
- **Writing files.** `storeWriteAll` deletes the old file and writes the new one; the write is not atomic (`B/store.cpp:3-18`).
- **Revert.** Risky timing changes snapshot all registry fields and restore them unless confirmed within 20 s (`B/revert.cpp`, `B/timing.cpp:978`).

---

## 12. Checklist of register fields touched by the video path

- **How it was made.** The list comes from scanning every `GBS::<FIELD>` use in the root `.ino/.cpp/.h` files of A and B. Excluded are `tv5725.h`, `tw.h`, the web UI blobs and B's generated registry. The scan ran on 2026-10-13 with the earlier attempt's `scripts/fields.py` (output in `mech/fields.json`) and a line-number variant of `sec12.py` (`mech/sec12b.py`).
- **Coverage.** 409 fields are named; 393 are used by A and 403 by B. Field definitions agree between A and B wherever both define a field.
- **Entry format:** `NAME sS_OO[bit:width] <file><line>`. `B123` means `B/tv5725.h:123`, and `A255` means `A/tv5725.h:255` (the field is defined only in A).
- **Markers:**
  - † used only by A;
  - ‡ used only by B;
  - ° read-only in practice (never written by either build).

The firmware also writes outside named fields:

- **Bulk writes.**
  - `zeroAll`: s0–s5, 0x00–0xFF.
  - The program image: s0_40–5F, s0_90–9F, s1_00–2F, s3_00–7F, s4_00–5F, s5_00–6F.
  - The constant sections: s1_30–5F (HD bypass), s1_60–83 (MD), s2_00–3F (deinterlacer).
  - Zeroing of s3_80–8F (PIP).
- **Raw single-byte writes in segment 5** from `prepareSyncProcessor`: 5_21, 5_22, 5_23, 5_24, 5_25, 5_26, 5_27, 5_2A, 5_2D, 5_2E, 5_2F, 5_31, 5_33, 5_34, 5_49, 5_4A, 5_4B, 5_4C, 5_51, 5_52, 5_53, 5_54, 5_58, 5_59, 5_5A, 5_5B, 5_5C, 5_5D (`A.ino:1256-1339`).
- **The segment-select byte 0xF0** (all segments).
- **B's registry** can additionally read and write any of its 958 named fields by name, from the API and the OLED field menus. Those uses are runtime data and do not appear in the scan.

- **0.1 Status, chip ID, period counters (s0_00–s0_2F, read-only)** (36): STATUS_00 s0_00[0:8] B34°; STATUS_03 s0_03[0:8] B59°; STATUS_04 s0_04[0:8] B68°; STATUS_05 s0_05[0:8] B77°; STATUS_IF_INP_USER s0_05[0:1] B78‡°; HPERIOD_IF s0_06[0:9] B84°; VPERIOD_IF s0_07[1:11] B85°; STATUS_MISC_PLL648_LOCK s0_09[6:1] B87‡°; STATUS_MISC_PLLAD_LOCK s0_09[7:1] B88°; CHIP_ID_PRODUCT s0_0C[0:8] B97°; CHIP_ID_REVISION s0_0D[0:8] B98°; STATUS_0F s0_0F[0:8] B109°; STATUS_INT_SOG_BAD s0_0F[0:1] B110°; STATUS_INT_SOG_SW s0_0F[1:1] B111°; STATUS_INT_INP_NO_SYNC s0_0F[4:1] B114°; STATUS_VDS_FIELD s0_11[0:1] B122°; STATUS_VDS_OUT_BLANK s0_11[1:1] B123‡°; STATUS_VDS_VERT_COUNT s0_11[4:11] B124°; STATUS_MEM_FF_WFF_FIFO_FULL s0_13[0:1] B126‡°; STATUS_MEM_FF_WFF_FIFO_EMPTY s0_13[1:1] B127‡°; STATUS_MEM_FF_RFF_FIFO_FULL s0_13[2:1] B128‡°; STATUS_MEM_FF_RFF_FIFO_EMPTY s0_13[3:1] B129‡°; STATUS_MEM_FF_CAP_FIFO_FULL s0_13[4:1] B130‡°; STATUS_MEM_FF_CAP_FIFO_EMPTY s0_13[5:1] B131‡°; STATUS_MEM_FF_PLY_FIFO_FULL s0_13[6:1] B132‡°; STATUS_MEM_FF_PLY_FIFO_EMPTY s0_13[7:1] B133‡°; STATUS_16 s0_16[0:8] B138°; STATUS_SYNC_PROC_HSPOL s0_16[0:1] B139°; STATUS_SYNC_PROC_HSACT s0_16[1:1] B140°; STATUS_SYNC_PROC_VSPOL s0_16[2:1] B141°; STATUS_SYNC_PROC_VSACT s0_16[3:1] B142°; STATUS_SYNC_PROC_HTOTAL s0_17[0:12] B143°; STATUS_SYNC_PROC_HLOW_LEN s0_19[0:12] B144°; STATUS_SYNC_PROC_VTOTAL s0_1B[0:11] B145°; TEST_BUS s0_2E[0:16] B149°; TEST_BUS_2F s0_2F[0:8] B151°.
- **0.2 Display clock PLL (PLL648) and clock pads** (14): PLL_CKIS s0_40[0:1] B153; PLL_DIVBY2Z s0_40[1:1] B154; PLL_IS s0_40[2:1] B155; PLL_ADS s0_40[3:1] B156; PLL_MS s0_40[4:3] B157; PLL648_CONTROL_01 s0_41[0:8] B158; PLL648_CONTROL_03 s0_43[0:8] B159; PLL_R s0_43[0:2] B160; PLL_S s0_43[2:2] B161; PLL_LEN s0_43[4:1] B162; PLL_VCORST s0_43[5:1] B163; PAD_CKIN_ENZ s0_49[0:1] B188; PAD_CKOUT_ENZ s0_49[1:1] B189; PAD_OSC_CNTRL s0_4A[0:3] B192.
- **0.3 DAC and output sync** (13): DAC_RGBS_PWDNZ s0_44[0:1] B164; DAC_RGBS_R0ENZ s0_44[2:1] B165; DAC_RGBS_G0ENZ s0_44[5:1] B166; DAC_RGBS_B0ENZ s0_45[0:1] B167; DAC_RGBS_SPD s0_45[2:1] B168; DAC_RGBS_S0ENZ s0_45[3:1] B169; DAC_RGBS_S1EN s0_45[4:1] B170; PAD_SYNC_OUT_ENZ s0_49[2:1] B190; PAD_TRI_ENZ s0_49[4:1] B191; DAC_RGBS_BYPS2DAC s0_4B[1:1] B193; DAC_RGBS_ADC2DAC s0_4B[2:1] B194; OUT_SYNC_CNTRL s0_4F[5:1] B197; OUT_SYNC_SEL s0_4F[6:2] B198.
- **0.4 Soft resets** (8): RESET_CONTROL_0x46 s0_46[0:8] B171; SFTRST_IF_RSTZ s0_46[0:1] B172; SFTRST_VDS_RSTZ s0_46[6:1] B176; RESET_CONTROL_0x47 s0_47[0:8] B177; SFTRST_DEC_RSTZ s0_47[0:1] B178; SFTRST_MODE_RSTZ s0_47[1:1] B179; SFTRST_SYNC_RSTZ s0_47[2:1] B180; SFTRST_HDBYPS_RSTZ s0_47[3:1] B181.
- **0.5 Pads, GPIO, debug pin, test bus** (9): PAD_BOUT_EN s0_48[0:1] B184; PAD_CONTROL_00_0x48 s0_48[0:8] B183; PAD_SYNC1_IN_ENZ s0_48[6:1] B185; PAD_SYNC2_IN_ENZ s0_48[7:1] B186; PAD_CONTROL_01_0x49 s0_49[0:8] B187; TEST_BUS_SEL s0_4D[0:5] B195; TEST_BUS_EN s0_4D[5:1] B196; GPIO_CONTROL_00 s0_52[0:8] B200; GPIO_CONTROL_01 s0_53[0:8] B201.
- **0.6 Interrupt status/reset** (5): INTERRUPT_CONTROL_00 s0_58[0:8] B202; INT_CONTROL_RST_SOGBAD s0_58[0:1] B203; INT_CONTROL_RST_SOGSWITCH s0_58[1:1] B204; INT_CONTROL_RST_NOHSYNC s0_58[4:1] B205; INTERRUPT_CONTROL_01 s0_59[0:8] B206.
- **0.7 On-screen display (s0_90–s0_98)** (19): OSD_SW_RESET s0_90[0:1] B593; OSD_HORIZONTAL_ZOOM s0_90[1:3] B594; OSD_VERTICAL_ZOOM s0_90[4:2] B595; OSD_DISP_EN s0_90[6:1] B596; OSD_MENU_EN s0_90[7:1] B597; OSD_MENU_ICON_SEL s0_91[0:4] B598; OSD_MENU_MOD_SEL s0_91[4:4] B599; OSD_MENU_BAR_FONT_FORCOR s0_92[0:3] B600; OSD_MENU_BAR_FONT_BGCOR s0_92[3:3] B601; OSD_MENU_BAR_BORD_COR s0_92[6:3] B602; OSD_MENU_SEL_FORCOR s0_93[1:3] B603; OSD_MENU_SEL_BGCOR s0_93[4:3] B604; OSD_COMMAND_FINISH s0_93[7:1] B605; OSD_MENU_DISP_STYLE s0_94[0:1] B606; OSD_YCBCR_RGB_FORMAT s0_94[2:1] B607; OSD_MENU_HORI_START s0_95[0:8] B610; OSD_MENU_VER_START s0_96[0:8] B611; OSD_BAR_LENGTH s0_97[0:8] B612; OSD_BAR_FOREGROUND_VALUE s0_98[0:8] B613.
- **1.1 Input formatter IF (s1_00–s1_2A)** (33): IF_MATRIX_BYPS s1_00[1:1] B209; IF_VS_SEL s1_00[5:1] B213; IF_PRGRSV_CNTRL s1_00[6:1] B214; IF_VS_FLIP s1_01[0:1] B216; IF_SEL_WEN s1_02[0:1] B219; INPUT_FORMATTER_02 s1_02[0:8] B218; IF_HS_SEL_LPF s1_02[1:1] B220; IF_HS_INT_LPF_BYPS s1_02[2:1] B221; IF_HS_PSHIFT_BYPS s1_02[3:1] B222; IF_HS_TAP11_BYPS s1_02[4:1] B223; IF_HS_Y_PDELAY s1_02[5:2] B224; IF_HS_DEC_FACTOR s1_0B[4:2] B225; IF_LD_SEL_PROV s1_0B[7:1] B227; IF_LD_RAM_BYPS s1_0C[0:1] B228; IF_INI_ST s1_0C[5:11] B230; IF_HSYNC_RST s1_0E[0:11] B231; IF_HB_ST s1_10[0:11] B232; IF_HB_SP s1_12[0:11] B233; IF_HB_ST2 s1_18[0:11] B235; IF_HB_SP2 s1_1A[0:11] B236; IF_VB_ST s1_1C[0:11] B237; IF_VB_SP s1_1E[0:11] B238; IF_LINE_SP s1_22[0:12] B240†; IF_HBIN_ST s1_24[0:12] B241; IF_HBIN_SP s1_26[0:12] B242; IF_LD_WRST_SEL s1_28[1:1] B243; IF_SEL_ADC_SYNC s1_28[2:1] B244; IF_TEST_EN s1_28[3:1] B245; IF_TEST_SEL s1_28[4:4] B246; IF_AUTO_OFST_EN s1_29[0:1] B247; IF_AUTO_OFST_PRD s1_29[1:1] B248; IF_AUTO_OFST_U_RANGE s1_2A[0:4] B249; IF_AUTO_OFST_V_RANGE s1_2A[4:4] B250.
- **1.2 Firmware scratch bytes stored in the program (s1_2B–s1_2D)** (7): GBS_PRESET_ID s1_2B[0:7] B251; GBS_PRESET_CUSTOM s1_2B[7:1] A255†; GBS_OPTION_SCANLINES_ENABLED s1_2C[0:1] A256†; GBS_OPTION_SCALING_RGBHV s1_2C[1:1] B252; GBS_OPTION_PALFORCED60_ENABLED s1_2C[2:1] A258†; GBS_RUNTIME_FTL_ADJUSTED s1_2C[4:1] B253; GBS_PRESET_DISPLAY_CLOCK s1_2D[0:8] B254.
- **1.3 HD-bypass path (s1_30–s1_55)** (19): HD_MATRIX_BYPS s1_30[1:1] B256; HD_DYN_BYPS s1_30[2:1] B257; HD_SEL_BLK_IN s1_30[3:1] B258; HD_Y_OFFSET s1_32[0:8] B259†; HD_U_OFFSET s1_34[0:8] B260†; HD_V_OFFSET s1_36[0:8] B261†; HD_HSYNC_RST s1_37[0:11] B262; HD_INI_ST s1_39[0:11] B263; HD_HB_ST s1_3B[0:12] B264; HD_HB_SP s1_3D[0:12] B265; HD_HS_ST s1_3F[0:12] B266; HD_HS_SP s1_41[0:12] B267; HD_VB_ST s1_43[0:12] B268; HD_VB_SP s1_45[0:12] B269; HD_VS_ST s1_47[0:12] B270; HD_VS_SP s1_49[0:12] B271; HD_BLK_GY_DATA s1_53[0:8] B272; HD_BLK_BU_DATA s1_54[0:8] B273; HD_BLK_RV_DATA s1_55[0:8] B274.
- **1.4 Mode detect MD (s1_60–s1_83)** (18): MD_HS_FLIP s1_63[6:1] B280; MD_VS_FLIP s1_63[7:1] B281; MD_SEL_VGA60 s1_65[7:1] B283; MD_VGA_75HZ_CNTRL s1_66[0:8] B284; MD_VGA_85HZ_CNTRL s1_67[0:8] B285; MD_SVGA_60HZ_CNTRL s1_6A[0:8] B286; MD_SVGA_75HZ_CNTRL s1_6B[0:8] B287; MD_SVGA_85HZ_CNTRL s1_6C[0:8] B288; MD_XGA_60HZ_CNTRL s1_6E[0:8] B289; MD_XGA_70HZ_CNTRL s1_6F[0:7] B290; MD_XGA_75HZ_CNTRL s1_70[0:7] B291; MD_XGA_85HZ_CNTRL s1_71[0:7] B292; MD_SXGA_60HZ_CNTRL s1_73[0:7] B293; MD_SXGA_75HZ_CNTRL s1_74[0:7] B294; MD_SXGA_85HZ_CNTRL s1_75[0:7] B295; MD_HD1250P_CNTRL s1_7F[0:7] B296; MD_USER_DEF_VCNTRL s1_80[0:8] B838‡; MD_USER_DEF_HCNTRL s1_81[0:8] B839‡.
- **2.1 Deinterlacer / motion-adaptive (s2_00–s2_3F)** (20): DEINT_00 s2_00[0:8] B298; DIAG_BOB_PLDY_RAM_BYPS s2_00[7:1] B299; MADPT_Y_VSCALE_BYPS s2_02[6:1] B300†; MADPT_UV_VSCALE_BYPS s2_02[7:1] B301†; MADPT_Y_MI_DET_BYPS s2_0A[7:1] B303; MADPT_Y_MI_OFFSET s2_0B[0:7] B304; MADPT_MO_ADP_UV_EN s2_16[5:1] B307; MAPDT_VT_SEL_PRGV s2_16[7:1] B309; MADPT_Y_DELAY_UV_DELAY s2_17[0:8] B310; MADPT_VTAP2_COEFF s2_19[4:4] B317; MADPT_PD_RAM_BYPS s2_24[2:1] B320; MADPT_VIIR_BYPS s2_26[6:1] B321; MADPT_VIIR_COEF s2_27[0:7] B322; MADPT_VSCALE_DEC_FACTOR s2_31[0:2] B323†; MADPT_SEL_PHASE_INI s2_31[2:1] B324†; MADPT_UVDLY_PD_SP s2_39[0:4] B329; MADPT_UVDLY_PD_ST s2_39[4:4] B330; MADPT_EN_UV_DEINT s2_3A[0:1] B331; MADPT_UV_MI_DET_BYPS s2_3A[7:1] B334; MADPT_UV_MI_OFFSET s2_3B[0:7] B335.
- **3.1 VDS output timing, scaling, frame control (s3_00–s3_23, s3_6D–s3_72)** (29): VDS_SYNC_EN s3_00[0:1] B337; VDS_HSCALE_BYPS s3_00[4:1] B341; VDS_VSCALE_BYPS s3_00[5:1] B342; VDS_HSYNC_RST s3_01[0:12] B345; VDS_VSYNC_RST s3_02[4:11] B346; VDS_HB_ST s3_04[0:12] B347; VDS_HB_SP s3_05[4:12] B348; VDS_VB_ST s3_07[0:11] B349; VDS_VB_SP s3_08[4:11] B350; VDS_HS_ST s3_0A[0:12] B351; VDS_HS_SP s3_0B[4:12] B352; VDS_VS_ST s3_0D[0:11] B353; VDS_VS_SP s3_0E[4:11] B354; VDS_DIS_HB_ST s3_10[0:12] B355; VDS_DIS_HB_SP s3_11[4:12] B356; VDS_DIS_VB_ST s3_13[0:11] B357; VDS_DIS_VB_SP s3_14[4:11] B358; VDS_HSCALE s3_16[0:10] B359; VDS_VSCALE s3_17[4:10] B360; VDS_FRAME_RST s3_19[0:10] B361; VDS_FLOCK_EN s3_1A[4:1] B362; VDS_FR_SELECT s3_1B[0:32] B367; VDS_FRAME_NO s3_1F[0:4] B368; VDS_VSYN_SIZE1 s3_20[0:11] B371; VDS_VSYN_SIZE2 s3_22[0:11] B372; VDS_EXT_HB_ST s3_6D[0:12] B506; VDS_EXT_HB_SP s3_6E[4:12] B507; VDS_EXT_VB_ST s3_70[0:11] B508; VDS_EXT_VB_SP s3_71[4:11] B509.
- **3.2 VDS filters and enhancement (line filter, tap6, step, peaking, black/white level, colour tags)** (34): VDS_V_DELAY s3_24[2:1] B376; VDS_TAP6_BYPS s3_24[3:1] B377†; VDS_Y_DELAY s3_24[4:2] B378; VDS_D_RAM_BYPS s3_26[6:1] B381†; VDS_BLEV_LEVEL s3_28[0:8] B385‡; VDS_BLEV_GAIN s3_29[0:8] B386‡; VDS_BLEV_BYPS s3_2A[0:1] B387‡; VDS_STEP_GAIN s3_2B[0:4] B390; VDS_UV_STEP_BYPS s3_2B[7:1] B392†; VDS_PK_VL_HL_SEL s3_43[0:1] B432; VDS_PK_VL_HH_SEL s3_43[1:1] B433; VDS_PK_LB_CORE s3_44[0:3] B436; VDS_PK_LB_CMP s3_44[3:5] B437†; VDS_PK_LB_GAIN s3_45[0:6] B438; VDS_PK_LH_CORE s3_46[0:3] B439; VDS_PK_LH_CMP s3_46[3:5] B440†; VDS_PK_LH_GAIN s3_47[0:6] B441; VDS_PK_Y_H_BYPS s3_4E[0:1] B451; VDS_W_LEV_BYPS s3_56[7:1] B477; VDS_WLEV_GAIN s3_58[0:8] B479; VDS_C1_TAG_LOW_SLOPE s3_60[4:10] B490‡; VDS_C1_TAG_HIGH_SLOPE s3_61[6:10] B491‡; VDS_C1_GAIN s3_63[0:4] B492‡; VDS_C1_U_LOW s3_63[4:8] B493‡; VDS_C1_U_HIGH s3_64[4:8] B494‡; VDS_C1_BYPS s3_65[4:1] B495‡; VDS_C1_Y_THRESH s3_65[5:8] B496‡; VDS_C2_TAG_LOW_SLOPE s3_66[5:10] B498‡; VDS_C2_TAG_HIGH_SLOPE s3_67[7:10] B499‡; VDS_C2_GAIN s3_69[1:4] B500‡; VDS_C2_U_LOW s3_69[5:8] B501‡; VDS_C2_U_HIGH s3_6A[5:8] B502‡; VDS_C2_BYPS s3_6B[5:1] B503‡; VDS_C2_Y_THRESH s3_6B[6:8] B504‡.
- **3.3 VDS colour conversion and output levels (s3_35–s3_3E, s3_40)** (10): VDS_Y_GAIN s3_35[0:8] B411; VDS_UCOS_GAIN s3_36[0:8] B412; VDS_VCOS_GAIN s3_37[0:8] B413; VDS_USIN_GAIN s3_38[0:8] B414†°; VDS_Y_OFST s3_3A[0:8] B416; VDS_U_OFST s3_3B[0:8] B417; VDS_V_OFST s3_3C[0:8] B418; VDS_SYNC_LEV s3_3D[0:9] B420; VDS_CONVT_BYPS s3_3E[3:1] B421; VDS_IN_DREG_BYPS s3_40[2:1] B427.
- **3.4 VDS test bus** (2): VDS_TEST_BUS_SEL s3_50[0:4] B457; VDS_TEST_EN s3_50[4:1] B458.
- **4.1 SDRAM controller, capture, playback, FIFOs (s4)** (27): SDRAM_RESET_CONTROL s4_00[0:8] B539; SDRAM_RESET_SIGNAL s4_00[4:1] B540; MEM_PAD_CLK_INVERT s4_13[0:1] B545; CAPTURE_ENABLE s4_21[0:1] B552; CAP_SAFE_GUARD_EN s4_21[5:1] B554; CAP_REQ_OVER s4_22[0:1] B555; CAP_STATUS_SEL s4_22[1:1] B556; PB_CUT_REFRESH s4_2B[0:1] B560; PB_REQ_SEL s4_2B[1:2] B561; PB_BYPASS s4_2B[3:1] B562; PB_MAST_FLAG_REG s4_2C[0:8] B565‡; PB_GENERAL_FLAG_REG s4_2D[0:8] B566‡; PB_CAP_OFFSET s4_37[0:10] B567; PB_FETCH_NUM s4_39[0:10] B568°; WFF_ENABLE s4_42[0:1] B569; WFF_FF_STA_INV s4_42[2:1] B570; RFF_ADR_ADD_2 s4_4D[4:1] B580; RFF_REQ_SEL s4_4D[5:2] B581; RFF_ENABLE s4_4D[7:1] B582; RFF_MASTER_FLAG s4_4E[0:8] B583‡; RFF_GENERAL_FLAG s4_4F[0:6] B1003‡; RFF_LINE_FLIP s4_50[5:1] B584; RFF_YUV_DEINTERLACE s4_50[6:1] B585; RFF_LREQ_CUT s4_50[7:1] B586; RFF_WFF_OFFSET s4_57[0:10] B589; RFF_FETCH_NUM s4_59[0:10] B590; MEM_FF_TOP_FF_SEL s4_5B[7:1] B591.
- **5.1 ADC front end, input mux, clamps, gain/offset (s5_00–s5_0F)** (27): ADC_5_00 s5_00[0:8] B615; ADC_CLK_PA s5_00[0:2] B616; ADC_CLK_ICLK2X s5_00[3:1] B617; ADC_CLK_ICLK1X s5_00[4:1] B618; ADC_SOGEN s5_02[0:1] B619; ADC_SOGCTRL s5_02[1:5] B620; ADC_INPUT_SEL s5_02[6:2] B621; ADC_5_03 s5_03[0:8] B622; ADC_POWDZ s5_03[0:1] B623; ADC_RYSEL_R s5_03[1:1] B624; ADC_RYSEL_G s5_03[2:1] B625; ADC_RYSEL_B s5_03[3:1] B626; ADC_FLTR s5_03[4:2] B627; ADC_TEST_04 s5_04[0:8] B628; ADC_TA_05_CTRL s5_05[0:8] B633; ADC_ROFCTRL s5_06[0:8] B634; ADC_GOFCTRL s5_07[0:8] B635; ADC_BOFCTRL s5_08[0:8] B636; ADC_RGCTRL s5_09[0:8] B637; ADC_GGCTRL s5_0A[0:8] B638; ADC_BGCTRL s5_0B[0:8] B639; ADC_TEST_0C s5_0C[0:8] B640; ADC_AUTO_OFST_PRD s5_0E[1:1] B645; ADC_AUTO_OFST_DELAY s5_0E[2:2] B646; ADC_AUTO_OFST_STEP s5_0E[4:2] B647; ADC_AUTO_OFST_TEST s5_0E[7:1] B648; ADC_AUTO_OFST_RANGE_REG s5_0F[0:8] B649.
- **5.2 ADC sampling PLL (PLLAD, s5_11–s5_17)** (13): PLLAD_CONTROL_00_5x11 s5_11[0:8] B650; PLLAD_VCORST s5_11[0:1] B651; PLLAD_LEN s5_11[1:1] B652; PLLAD_PDZ s5_11[4:1] B655; PLLAD_FS s5_11[5:1] B656; PLLAD_LAT s5_11[7:1] B658; PLLAD_MD s5_12[0:12] B659; PLLAD_5_16 s5_16[0:8] B660; PLLAD_R s5_16[0:2] B661; PLLAD_S s5_16[2:2] B662; PLLAD_KS s5_16[4:2] B663; PLLAD_CKOS s5_16[6:2] B664; PLLAD_ICP s5_17[0:3] B665.
- **5.3 Phase adjust (s5_18–s5_19)** (6): PA_ADC_BYPSZ s5_18[0:1] B666; PA_ADC_S s5_18[1:5] B667; PA_ADC_LAT s5_18[7:1] B669; PA_SP_BYPSZ s5_19[0:1] B670; PA_SP_S s5_19[1:5] B671; PA_SP_LAT s5_19[7:1] B673.
- **5.4 Decimation / oversampling (s5_1E–s5_1F)** (7): DEC_WEN_MODE s5_1E[7:1] B674; DEC1_BYPS s5_1F[0:1] B676; DEC2_BYPS s5_1F[1:1] B677; DEC_MATRIX_BYPS s5_1F[2:1] B678; DEC_TEST_ENABLE s5_1F[3:1] B679; DEC_TEST_SEL s5_1F[4:3] B680; DEC_IDREG_EN s5_1F[7:1] B681.
- **5.5 Sync processor SP (s5_20–s5_57)** (44): SP_SOG_SRC_SEL s5_20[0:1] B682; SP_SOG_P_ATO s5_20[1:1] B683; SP_EXT_SYNC_SEL s5_20[3:1] B685; SP_JITTER_SYNC s5_20[4:1] B686; SP_H_TIMER_VAL s5_33[0:8] B688; SP_DLT_REG s5_35[0:12] B689; SP_H_PULSE_IGNOR s5_37[0:8] B690; SP_PRE_COAST s5_38[0:8] B691; SP_POST_COAST s5_39[0:8] B692; SP_H_TOTAL_EQ_THD s5_3A[0:8] B693; SP_SDCS_VSST_REG_H s5_3B[0:3] B694; SP_SDCS_VSSP_REG_H s5_3B[4:3] B695; SP_CS_0x3E s5_3E[0:8] B696°; SP_CS_P_SWAP s5_3E[0:1] B697; SP_H_COAST s5_3E[2:1] B699; SP_H_PROTECT s5_3E[4:1] B700; SP_DIS_SUB_COAST s5_3E[5:1] B701; SP_SDCS_VSST_REG_L s5_3F[0:8] B702; SP_SDCS_VSSP_REG_L s5_40[0:8] B703; SP_CS_CLP_ST s5_41[0:12] B704; SP_CS_CLP_SP s5_43[0:12] B705; SP_CS_HS_ST s5_45[0:12] B706; SP_CS_HS_SP s5_47[0:12] B707; SP_RT_HS_ST s5_49[0:12] B708; SP_RT_HS_SP s5_4B[0:12] B709; SP_H_CST_ST s5_4D[0:12] B710; SP_H_CST_SP s5_4F[0:12] B711; SP_HS_POL_ATO s5_55[4:1] B712; SP_VS_POL_ATO s5_55[6:1] B713; SP_HCST_AUTO_EN s5_55[7:1] B714; SP_5_56 s5_56[0:8] B715; SP_SOG_MODE s5_56[0:1] B716; SP_HS2PLL_INV_REG s5_56[1:1] B717; SP_CLAMP_MANUAL s5_56[2:1] B718; SP_CLP_SRC_SEL s5_56[3:1] B719; SP_SYNC_BYPS s5_56[4:1] B720; SP_HS_PROC_INV_REG s5_56[5:1] B721; SP_VS_PROC_INV_REG s5_56[6:1] B722; SP_5_57 s5_57[0:8] B724; SP_NO_CLAMP_REG s5_57[0:1] B725; SP_COAST_INV_REG s5_57[1:1] B726; SP_NO_COAST_REG s5_57[2:1] B727; SP_HS_LOOP_SEL s5_57[6:1] B728; SP_HS_REG s5_57[7:1] B729.
- **5.6 Spare bytes used as firmware scratch / SP test select (s5_60–s5_69)** (9): ADC_UNUSED_60 s5_60[0:8] B730†; ADC_UNUSED_61 s5_61[0:8] B731†; ADC_UNUSED_62 s5_62[0:8] B732; TEST_BUS_SP_SEL s5_63[0:8] B733; ADC_UNUSED_64 s5_64[0:8] B734; ADC_UNUSED_65 s5_65[0:8] B735; ADC_UNUSED_66 s5_66[0:8] B736; ADC_UNUSED_67 s5_67[0:16] B737; ADC_UNUSED_69 s5_69[0:8] B738.

---

## 13. Differences between A and B worth knowing

| # | topic | A | B | where |
|---|---|---|---|---|
| 1 | Program source | 12 fixed programs (NTSC/PAL × 960/1024/720/480-576/1080/downscale) plus custom programs from flash | **one** base program (A's ntsc_1920x1080 with line filter, UV step and H peaking bypassed) plus computed timing (CVT-RB, CVT, manual, integer scaling) and geometry | `A.ino:4405-4464`; `B/program.cpp:994-997`, `B/timing.cpp:205-386` |
| 2 | Output rate without frame lock | program clock (internal PLL648), HTOTAL trimmed by auto best HTotal | pixel clock set to source Hz × totals on the Si5351 right after load ("default mode") | `A.ino:3917-3958`; `B/timing.cpp:839-855` |
| 3 | Frame lock with Si5351 | continuous phase/frequency lock (`runFrequency`, ±0.06 % clamps) | **removed**: the rate ratio is only re-established when missing; no continuous phase correction | `A/framesync.h:503-672`, `A.ino:9373-9386`; `B/gbs-control.ino:372-376` |
| 4 | Frame lock without Si5351 | the same VTOTAL ±2 two-state controller and best HTotal | same | `A/framesync.h:411-481`; `B/framesync.h:269-337` |
| 5 | Period sampling | busy loop of up to 3 M iterations, WDT disabled, WiFi light sleep while waiting | 100 ms deadline, explicit detach | `A/framesync.h:376-404`; `B/framesync.h:46-72` |
| 6 | Blocking behaviour | detection, post-load waits and watcher back-offs use `delay()` with `handleWiFi()` inside | state machines (`detectTick`, `postLoadTick`, `watcherHold`) plus `SchedSequence` guards; the scheduler measures each tick | `A.ino:1602-1812`, `A.ino:4086-4128`; `B/detect.cpp`, `B/program.cpp:434-475`, `B/watch.cpp:60-175`, `B/sched.h` |
| 7 | Input dwell | 450 ms looking for HS on the current input before switching | single check after 10 ms, then switch | `A.ino:1606-1611`; `B/detect.cpp:161-170` |
| 8 | HTOTAL register semantics | writes the total into `VDS_HSYNC_RST` | writes total − 1 and reads back +1 | `A.ino:2702`; `B/timing.cpp:394-395`, `B/timing.cpp:442` |
| 9 | Peaking defaults | CMP 31, gains 0x14/0x14, H peaking on by default | gains 0x16/0x0A, CMP untouched, H peaking bypassed in the base program; user picture config | `A.ino:3549-3556`, `A.ino:3878-3882`; `B/program.cpp:615-620`, `B/picture.cpp:115-169` |
| 10 | Scanline strength | value written directly as the MI offset (0 = strongest) | offset = 0x7F − strength; snapshot and restore of all touched fields | `A.ino:5728-5740`; `B/scanlines.cpp:43-46`, `B/scanlines.cpp:91-158` |
| 11 | Deinterlace options | motion-adaptive or "bob" (`deintMode`), auto toggle optional | motion-adaptive always auto-toggled | `A.ino:6758-6840`; `B/watch.cpp:681-751` |
| 12 | RGBHV scaling | only with `preferScalingRgbhv` | always | `A.ino:6881`; `B/watch.cpp` RGBHV branch from line 757 |
| 13 | Options dropped in B | component output, PAL→60, match-preset-source, full height, line filter, step response, custom slots, start-in-bypass | replaced by profiles, user register overrides, geometry (aspect, trims, zoom), picture config, user-defined MD mode | `A/options.h:16-39`; `B/useroptions.h:6-24`, `B/prefs.cpp:36-71` |
| 14 | Done stage | DAC/sync on and external clock adopted at stable 36–44 | same, plus an output re-present after a WDT or exception reset, plus the display-clock watchdog (adopt after 10 s on the internal clock) | `A.ino:9497-9540`; `B/gbs-control.ino:486-526`, `B/timing.cpp:733-777` |
| 15 | Bus layer | `rawRead` copies without a bound | bounded copy, write observer, `currentSegment()` | `A/tw.h:66-80`; `B/tw.h:63-96`, `B/tw.h:312` |
| 16 | OSD | the `OSDManager` plus a legacy `osd.h` `Menu` (buttons build only); `Menu::init` at every post-load | `OSDManager` only; `osdHardwareInit` does the same writes | `A.ino:3909`, `A/osd.h:150-169`; `B/program.cpp:777`, `B/OSDManager.cpp:134-152` |
| 17 | Encoder | 150 ms debounce, 120 ms reversal filter, not reversed | 75 ms / 60 ms, reversed by default | `A.ino:7735-7744`, `A/OLEDMenuConfig.h:21-22`; `B/OLEDMenuImplementation.cpp:889-895`, `B/OLEDMenuConfig.h:20-21` |
| 18 | Preferences | ASCII `/preferencesv2.txt`, with the scanline-strength bug | hashed binary `/prefs.bin`, deferred writes | `A.ino:7920-8001`, `A.ino:11019-11050`; `B/prefs.cpp:111-175` |
| 19 | Web | WebSocket status on :81 plus single-char commands `/sc`, `/uc` | JSON REST `/api/*` with a register registry, revert snapshots, journal | `A.ino:10223-10715`; `B/api.cpp`, `B/network.cpp` |
| 20 | `useHdmiSyncFix` | also set by web resolution changes | set only by the watcher's format-change rule | `A.ino:9792`; `B/watch.cpp:567-574` |
| 21 | Clock safety | Si5351 retuned to whatever sfr/ofr yields | refuses a ratio outside 20–162 MHz, clamps smooth retunes | `A.ino:396-445`; `B/displayclock.cpp:32-36`, `B/displayclock.cpp:148-158` |
| 22 | User-defined mode | no | MD user H/V counts (s1_80/81) and STATUS_05 bit 0 mapped to a chosen standard | `B/mode.cpp:111-114`, `B/mode.cpp:153-158` |
| 23 | Mode-watch holds | none | `ignoreModeChangeUntil` suppresses format changes while timing work runs | `B/watch.cpp:522-524`, `B/watch.cpp:1281-1284` |

**What both share unchanged, and is therefore the most-tested behaviour.** The bus abstraction; the program layout and its load-time patches; `setResetParameters`; `prepareSyncProcessor`; the SOG/phase/clamp/coast and auto-gain algorithms (`B/tune.cpp` is a line-for-line port of A's functions); classification; the sync-watcher counter schedule; HD and RGBHV bypass; the Si5351 library; the motion-adaptive deinterlacer toggles; the OSD register protocol.

**Things that look like latent bugs or loose ends in the sources:**

- `OSDManager.preset` is never set, so the OSD zoom is always 5×/4× (`A/OSDManager.h:115`).
- A's preferences fallback for `scanlineStrength` lands on `enableCalibrationADC` (`A.ino:7993-7995`).
- The two builds disagree about whether `VDS_HSYNC_RST` holds the total or total − 1 (row 8).
- `setIfHblankParameters` is computed but commented out (`A.ino:3532`).
- `ofw_*` and `rgbhv.h` are dead weight (2.5).
- Only CLK0 of the Si5351 is used, and the crystal is assumed to be 25 MHz (`A.ino:485`).
