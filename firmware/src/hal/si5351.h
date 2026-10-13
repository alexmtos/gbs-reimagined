#pragma once

#include <stdint.h>

#include "hal/i2c_bus.h"

namespace gbsr::hal {

// Divider settings for one output frequency, in the encoding of Skyworks AN619.
//
// Strategy: the MultiSynth divider is an even integer (lowest jitter, and the only legal choice
// above 150 MHz, where it must be 4) and the PLL feedback divider is fractional. Fine frequency
// changes therefore move only the PLL fraction, which the PLL tracks without a reset.
struct Si5351Plan {
    uint32_t outputHz = 0;  // requested frequency
    uint32_t actualHz = 0;  // frequency the dividers produce (rounded)
    uint32_t vcoHz = 0;     // PLL VCO frequency, 600..900 MHz
    uint16_t msDivider = 0; // MultiSynth integer divider (4, 6, 8 ... 2048)
    uint32_t pllA = 0;      // PLL feedback a + b/c
    uint32_t pllB = 0;
    uint32_t pllC = 0;
    bool valid = false;
};

// Pure computation, testable on the host. `xtalHz` is the crystal (25 MHz on the GBS-C).
Si5351Plan si5351Plan(uint32_t xtalHz, uint32_t outputHz);

// Encodes a + b/c as the three AN619 parameters P1, P2, P3.
void si5351Params(uint32_t a, uint32_t b, uint32_t c, uint32_t& p1, uint32_t& p2, uint32_t& p3);

// Driver for the Si5351A on the GBS-C: one output (CLK0) feeding the TV5725 display clock pin.
class Si5351 {
public:
    static constexpr uint8_t kAddress = 0x60;
    static constexpr uint32_t kMinHz = 1'000'000;
    static constexpr uint32_t kMaxHz = 200'000'000;

    enum class Drive : uint8_t { mA2 = 0, mA4 = 1, mA6 = 2, mA8 = 3 };

    explicit Si5351(I2cBus& bus, uint32_t xtalHz = 25'000'000) : bus_(bus), xtalHz_(xtalHz) {}

    // True when the device answers and has finished its power-on initialisation.
    bool probe();

    // Disables every output, applies the crystal load capacitance and powers down CLK1/CLK2.
    bool begin(Drive drive = Drive::mA6);

    // Programs CLK0. When the MultiSynth divider changes the PLL is reset, which glitches the
    // clock; within the same divider only the PLL fraction is rewritten (no reset).
    bool setFrequency(uint32_t hz);

    bool setOutputEnabled(bool enabled);

    uint32_t frequency() const { return plan_.actualHz; }
    bool outputEnabled() const { return enabled_; }
    const Si5351Plan& plan() const { return plan_; }

private:
    bool writePll(const Si5351Plan& plan);
    bool writeMultisynth(const Si5351Plan& plan);
    bool writeRegs(uint8_t first, const uint8_t* values, uint8_t count);

    I2cBus& bus_;
    uint32_t xtalHz_;
    Drive drive_ = Drive::mA6;
    Si5351Plan plan_{};
    bool enabled_ = false;
};

} // namespace gbsr::hal
