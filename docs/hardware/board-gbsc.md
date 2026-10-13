# RetroScaler GBS-C — board reference

For firmware developers. Everything the firmware can and cannot reach on the one board GBS Reimagined
supports. Facts marked *(ref)* come from the reference project's bench notes
([research](../research/gbs-control-lessons-platform.md)) and are still to be re-verified here;
the rest come from vendor documents ([datasheets](datasheets.md)) or the product photos.

## What is on the PCB

The GBS-C is RetroScaler's all-in-one version of the GBS-8200: the stock scaler design plus a
microcontroller, a clock generator, a display and an HDMI converter on one board.

| Part | Role | On the ESP's I2C bus | Firmware reach |
|---|---|---|---|
| Tvia TrueView5725 (TV5725-AP) | The scaler: triple 8-bit ADC, sync processor, deinterlacer, scaler, OSD, DAC | `0x17` | Full: 558 registers in 6 segments ([register map](../reference/register-map.md)) |
| SDRAM, "2 MB" (16 Mbit) | Frame buffer for the deinterlacer and frame-rate conversion | — | Through the TV5725 memory controller only; part not identified |
| Skyworks Si5351A | Output pixel clock: CLK0 → TV5725 pin 40, 25 MHz crystal | `0x60` | Full ([driver](../../firmware/src/hal/si5351.h)) |
| ESP8266EX (ESP-12F-style module, 4 MB flash) | Runs this firmware; WiFi | host | — |
| WCH CH340 | USB serial on the micro-USB jack: console and USB flashing | — | — |
| SSD1306 OLED, 128×64 | Local display | `0x3C` | Full |
| Rotary encoder with push button | Local input | GPIO | Full |
| Macrosilicon MS9288A | VGA → HDMI: re-digitises the TV5725's analogue RGB with its own ADCs and MCU | no | None. It bounds the output: ≤ 165 MHz sampling, 1080p60 output, "up to 1080p" input (family brief) |
| TI LM1881 | Sync separator in front of the RGBS input | no | None |
| Myson MTV230MV | The stock controller, still fitted | no | None; superseded by the ESP |
| AT24C02-class EEPROM | The stock controller's settings memory | `0x50` | Answers, reads `0xFF`; not used (ownership unclear) *(ref)* |

## Signal chain

```text
SCART / RGBS (via LM1881) ─┐
VGA (RGBHV) ───────────────┼─► TV5725 ADC inputs ─► sync proc ─► deinterlacer/SDRAM ─► scaler ─► DAC ─┬─► VGA out
Component (YPbPr, SOG) ────┘                                                     ▲               ├─► MS9288A ─► HDMI out
                                                              Si5351 pixel clock ┘               └─► RGBS/YPbPr out (Saturn mini-DIN)
```

The TV5725 has **three** analogue input sets selected by `ADC_INPUT_SEL`; two carry a
sync-on-green slicer, the third does not. Which physical jack lands on which set is not documented
by the vendor and must be confirmed on the bench (open item B-01 in the
[roadmap](../roadmap.md)).

## Hardware controls the firmware cannot see

These are wired in the analogue path. The firmware cannot read their position, so the interfaces
tell the user to check them when a symptom matches. The vendor publishes only their labels; the
"function" column is **unverified** until traced on the board (open item B-02 in the
[roadmap](../roadmap.md)).

| Control (label on the board) | Presumed function — to verify | When it would matter |
|---|---|---|
| HDMI slide switch (ON/OFF) | Enables the MS9288A HDMI path | No HDMI picture while VGA out works |
| RGBHV / RGBS slide switch | Sync routing for the VGA-connector input | RGBS sources through the VGA jack |
| Compatibility mode slide switch | Unknown | — |
| "RGBS Sync" latching button | Routes the RGBS sync input | RGBS sources with separate sync |
| "Sync on Green" latching button | Routes sync-on-green | Component (YPbPr) and RGsB sources |
| R / G / B knobs | Analogue level trims | Colour balance |
| Power latching button | Board power | — |

## ESP8266 pins

| GPIO | D1-mini name | Use | Constraint |
|---|---|---|---|
| 4 | D2 | I2C SDA | shared bus, 400 kHz |
| 5 | D1 | I2C SCL | |
| 14 | D5 | Encoder A (CLK) | interrupt |
| 13 | D7 | Encoder B (DATA) | |
| 0 | D3 | Encoder push button | **boot strap**: held low at reset enters the serial bootloader |
| 12 | D6 | TV5725 test-bus input (vsync timing for frame lock / latency) | interrupt |
| 2 | D4 | Status LED, active low | must be high at boot |
| 15 | D8 | free | must be low at boot |
| 16 | D0 | free | no interrupt |
| A0 | — | free (or internal VCC sense) | |
| 1 / 3 | TX / RX | serial console, 115 200 baud | |

Pin assignments come from the reference firmware that runs on this board *(ref)*.

## Clocks

- The Si5351 drives the display only while `PLL648_CONTROL_01` = `0x75`; every other code selects
  an internal rate (`0xA5` = 162 MHz) and the output then free-runs instead of following the
  source *(ref)*.
- DAC ceiling: 162 MHz (DS-5725-3.2, Table 19). The MS9288A samples up to 165 MHz.
- An ESP reset does **not** reset the TV5725, the Si5351 or the SDRAM; the firmware must
  initialise them deterministically from any prior state *(ref)*.

## Power

5 V, 2 A barrel jack. A weak supply (thin USB cable, laptop port) is the most common cause of a
board that half-works *(ref)*. The board has its own power button; the ESP can stay powered from
USB while the scaler is off, which the firmware reports as "scaler not answering".
