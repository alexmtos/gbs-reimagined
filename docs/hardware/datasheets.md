# Datasheets

The vendor documents for every chip on the board. They are **not committed** (redistribution
rights are unclear and the hosting pages are not permanent); `tools/datasheets.py` downloads them
into `docs/hardware/pdf/` and checks each against the SHA-256 pinned in
[datasheets.json](datasheets.json):

```sh
uv run tools/datasheets.py               # download what is missing, verify all
uv run tools/datasheets.py --from <dir>  # copy from a local folder instead
```

| File | Part | Document | Why it matters |
|---|---|---|---|
| `tv5725_registers_definition_v1.1.pdf` | Tvia TV5725 | RD-5725-1.1, 235 pp. | Every register, bit by bit; source of access and pages in `schema/registers/tv5725.yaml` |
| `tv5725_register_map.pdf` | Tvia TV5725 | RD-5725-1.1 map, 1 p. | Segment overview |
| `tv5725_datasheet.pdf` | Tvia TV5725 | DS-5725-3.2, 45 pp. | Pinout, block diagram, electrical limits (DAC 162 MHz) |
| `si5351_datasheet.pdf` | Skyworks Si5351 | Rev. 1.3 | Clock generator |
| `si5351_an619.pdf` | Skyworks Si5351 | AN619 | Register formulas behind `firmware/src/hal/si5351.cpp` |
| `ms9288a_datasheet_cn.pdf` | Macrosilicon MS9288A | rev B/0 2024, Chinese | HDMI converter limits; extract text with PyMuPDF (pdftotext drops the Chinese) |
| `ms9282_product_brief_en.pdf` | Macrosilicon MS9282 | brief, English | Family input limit "up to 1080p" |
| `ssd1306_datasheet.pdf` | Solomon Systech SSD1306 | Rev 1.1 | OLED controller |
| `esp8266ex_datasheet.pdf` | Espressif ESP8266EX | v7.1 | The MCU |
| `lm1881_datasheet.pdf` | TI LM1881 | Rev. G | Sync separator on RGBS |
| `at24c02d_datasheet.pdf` | Microchip AT24C02D | 2018 | EEPROM-class part at `0x50` |
| `ch340_datasheet.pdf` | WCH CH340 | v1D | USB serial |

## Reading the TV5725 register definition

`pdftotext -layout` gives usable text, but the bit tables come out misaligned: a description can
land beside the wrong bit. Read bit names from the header row and match descriptions by meaning,
or open the page (the register map lists the page of every register).
