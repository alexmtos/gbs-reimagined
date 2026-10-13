#pragma once

#include <stdint.h>

namespace gbsr::hal {

// What the firmware needs to know about the PCB it runs on. One profile per supported board;
// the build selects it with -DGBSR_BOARD_<NAME>. See docs/hardware/board-gbsc.md.
struct BoardProfile {
    const char* name;

    // I2C bus shared by the scaler, the clock generator and the display.
    uint8_t pinSda;
    uint8_t pinScl;
    uint32_t i2cClockHz;

    // Rotary encoder. The push button sits on GPIO0, the ESP8266's boot strap: holding the knob
    // down through a reset enters the serial bootloader.
    uint8_t pinEncoderA;
    uint8_t pinEncoderB;
    uint8_t pinEncoderButton;

    // TV5725 test-bus output (selected vsync), timed by the frame lock and latency instruments.
    uint8_t pinVsyncProbe;

    uint8_t pinStatusLed;
    bool statusLedActiveLow;

    uint8_t addrScaler;
    uint8_t addrClockGen;
    uint8_t addrDisplay;
    uint32_t clockGenXtalHz;

    bool hasClockGen;
    bool hasDisplay;
    bool hasHdmiConverter; // MS9288A after the VGA output (not controllable, but it bounds modes)
};

// RetroScaler GBS-C: TV5725 + Si5351 + SSD1306 + encoder + MS9288A on one PCB, ESP-12F module.
inline constexpr BoardProfile kBoardGbsc{
    "RetroScaler GBS-C",
    /*pinSda*/ 4,
    /*pinScl*/ 5,
    /*i2cClockHz*/ 400'000,
    /*pinEncoderA*/ 14,
    /*pinEncoderB*/ 13,
    /*pinEncoderButton*/ 0,
    /*pinVsyncProbe*/ 12,
    /*pinStatusLed*/ 2,
    /*statusLedActiveLow*/ true,
    /*addrScaler*/ 0x17,
    /*addrClockGen*/ 0x60,
    /*addrDisplay*/ 0x3C,
    /*clockGenXtalHz*/ 25'000'000,
    /*hasClockGen*/ true,
    /*hasDisplay*/ true,
    /*hasHdmiConverter*/ true,
};

inline constexpr const BoardProfile& board() {
    return kBoardGbsc;
}

} // namespace gbsr::hal
