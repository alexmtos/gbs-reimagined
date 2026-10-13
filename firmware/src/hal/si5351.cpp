#include "hal/si5351.h"

namespace gbsr::hal {

namespace {

// Register map, Skyworks Si5351 datasheet and AN619.
constexpr uint8_t kRegStatus = 0;
constexpr uint8_t kRegOutputEnable = 3; // bit n = 1 disables CLKn
constexpr uint8_t kRegClk0Control = 16;
constexpr uint8_t kRegClk1Control = 17;
constexpr uint8_t kRegClk2Control = 18;
constexpr uint8_t kRegPllA = 26; // eight bytes, PLLA feedback multisynth
constexpr uint8_t kRegMs0 = 42;  // eight bytes, MultiSynth0
constexpr uint8_t kRegSpreadSpectrum = 149;
constexpr uint8_t kRegPllReset = 177;
constexpr uint8_t kRegCrystalLoad = 183;

constexpr uint8_t kStatusSysInit = 0x80; // device still initialising
constexpr uint8_t kClkPowerDown = 0x80;
constexpr uint8_t kClkIntegerMode = 0x40;
constexpr uint8_t kClkSourceMultisynth = 0x0C;
constexpr uint8_t kPllAReset = 0x20;
constexpr uint8_t kCrystalLoad8pF = 0x92; // bits 7:6 = 10 (8 pF), low bits must read 010010
constexpr uint32_t kFractionDenominator = 1'048'575; // 2^20 - 1, the finest legal c

constexpr uint32_t kVcoMinHz = 600'000'000;
constexpr uint32_t kVcoMaxHz = 900'000'000;

} // namespace

void si5351Params(uint32_t a, uint32_t b, uint32_t c, uint32_t& p1, uint32_t& p2, uint32_t& p3) {
    const uint32_t floorTerm = static_cast<uint32_t>((128ULL * b) / c);
    p1 = 128 * a + floorTerm - 512;
    p2 = 128 * b - c * floorTerm;
    p3 = c;
}

Si5351Plan si5351Plan(uint32_t xtalHz, uint32_t outputHz) {
    Si5351Plan plan;
    plan.outputHz = outputHz;
    if (xtalHz == 0 || outputHz < Si5351::kMinHz || outputHz > Si5351::kMaxHz) {
        return plan;
    }

    // Largest even divider that keeps the VCO at or below its maximum; above 150 MHz the only
    // legal MultiSynth setting is the divide-by-4 mode.
    uint32_t divider = kVcoMaxHz / outputHz;
    divider &= ~1u;
    if (outputHz > 150'000'000) {
        divider = 4;
    }
    if (divider < 4 || divider > 2048) {
        return plan;
    }
    const uint64_t vco = static_cast<uint64_t>(outputHz) * divider;
    if (vco < kVcoMinHz || vco > kVcoMaxHz) {
        return plan;
    }

    const uint32_t a = static_cast<uint32_t>(vco / xtalHz);
    const uint64_t remainder = vco - static_cast<uint64_t>(a) * xtalHz;
    uint32_t b = static_cast<uint32_t>((remainder * kFractionDenominator + xtalHz / 2) / xtalHz);
    uint32_t aFinal = a;
    if (b >= kFractionDenominator) { // rounding carried into the integer part
        b -= kFractionDenominator;
        aFinal += 1;
    }
    if (aFinal < 15 || aFinal > 90) {
        return plan;
    }

    plan.msDivider = static_cast<uint16_t>(divider);
    plan.pllA = aFinal;
    plan.pllB = b;
    plan.pllC = kFractionDenominator;
    const uint64_t actualVco =
        static_cast<uint64_t>(xtalHz) * aFinal +
        (static_cast<uint64_t>(xtalHz) * b + kFractionDenominator / 2) / kFractionDenominator;
    plan.vcoHz = static_cast<uint32_t>(actualVco);
    plan.actualHz = static_cast<uint32_t>((actualVco + divider / 2) / divider);
    plan.valid = true;
    return plan;
}

bool Si5351::probe() {
    uint8_t status = 0;
    if (!bus_.readReg(kAddress, kRegStatus, status)) {
        return false;
    }
    return (status & kStatusSysInit) == 0;
}

bool Si5351::begin(Drive drive) {
    drive_ = drive;
    enabled_ = false;
    plan_ = {};
    return bus_.writeReg(kAddress, kRegOutputEnable, 0xFF) &&
           bus_.writeReg(kAddress, kRegClk0Control, kClkPowerDown) &&
           bus_.writeReg(kAddress, kRegClk1Control, kClkPowerDown) &&
           bus_.writeReg(kAddress, kRegClk2Control, kClkPowerDown) &&
           bus_.writeReg(kAddress, kRegSpreadSpectrum, 0x00) &&
           bus_.writeReg(kAddress, kRegCrystalLoad, kCrystalLoad8pF);
}

bool Si5351::setFrequency(uint32_t hz) {
    const Si5351Plan next = si5351Plan(xtalHz_, hz);
    if (!next.valid) {
        return false;
    }
    const bool dividerChanged = next.msDivider != plan_.msDivider;
    if (!writePll(next)) {
        return false;
    }
    if (dividerChanged) {
        if (!writeMultisynth(next)) {
            return false;
        }
        const uint8_t control = kClkIntegerMode | kClkSourceMultisynth |
                                static_cast<uint8_t>(drive_); // source PLLA, not inverted
        if (!bus_.writeReg(kAddress, kRegClk0Control, control) ||
            !bus_.writeReg(kAddress, kRegPllReset, kPllAReset)) {
            return false;
        }
    }
    plan_ = next;
    return true;
}

bool Si5351::setOutputEnabled(bool enabled) {
    if (!bus_.writeReg(kAddress, kRegOutputEnable, enabled ? 0xFE : 0xFF)) {
        return false;
    }
    enabled_ = enabled;
    return true;
}

bool Si5351::writePll(const Si5351Plan& plan) {
    uint32_t p1 = 0, p2 = 0, p3 = 0;
    si5351Params(plan.pllA, plan.pllB, plan.pllC, p1, p2, p3);
    const uint8_t regs[8] = {
        static_cast<uint8_t>((p3 >> 8) & 0xFF),
        static_cast<uint8_t>(p3 & 0xFF),
        static_cast<uint8_t>((p1 >> 16) & 0x03),
        static_cast<uint8_t>((p1 >> 8) & 0xFF),
        static_cast<uint8_t>(p1 & 0xFF),
        static_cast<uint8_t>(((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0x0F)),
        static_cast<uint8_t>((p2 >> 8) & 0xFF),
        static_cast<uint8_t>(p2 & 0xFF),
    };
    return writeRegs(kRegPllA, regs, sizeof regs);
}

bool Si5351::writeMultisynth(const Si5351Plan& plan) {
    uint8_t regs[8] = {0, 1, 0, 0, 0, 0, 0, 0}; // P3 = 1: integer divider
    if (plan.msDivider == 4) {
        regs[2] = 0x0C; // MS0_DIVBY4 = 11, P1 = P2 = 0
    } else {
        uint32_t p1 = 0, p2 = 0, p3 = 0;
        si5351Params(plan.msDivider, 0, 1, p1, p2, p3);
        regs[2] = static_cast<uint8_t>((p1 >> 16) & 0x03); // R0_DIV = 1
        regs[3] = static_cast<uint8_t>((p1 >> 8) & 0xFF);
        regs[4] = static_cast<uint8_t>(p1 & 0xFF);
    }
    return writeRegs(kRegMs0, regs, sizeof regs);
}

bool Si5351::writeRegs(uint8_t first, const uint8_t* values, uint8_t count) {
    uint8_t buf[9];
    if (count > sizeof buf - 1) {
        return false;
    }
    buf[0] = first;
    for (uint8_t i = 0; i < count; ++i) {
        buf[i + 1] = values[i];
    }
    return bus_.write(kAddress, buf, count + 1u);
}

} // namespace gbsr::hal
