#include <stdlib.h>

#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "hal/si5351.h"

using namespace gbsr;
using namespace gbsr::hal;

namespace {

constexpr uint32_t kXtal = 25'000'000;

// Error in parts per billion between what the plan produces and what was asked.
long long errorPpb(const Si5351Plan& p) {
    const double exact =
        static_cast<double>(kXtal) * (p.pllA + static_cast<double>(p.pllB) / p.pllC) / p.msDivider;
    return llabs(static_cast<long long>((exact - p.outputHz) / p.outputHz * 1e9));
}

} // namespace

TEST_CASE("plans keep the VCO in range and use an even MultiSynth divider") {
    const uint32_t freqs[] = {13'500'000,  27'000'000,  40'500'000,  81'000'000,  108'000'000,
                              138'514'147, 148'500'000, 150'000'000, 155'830'000, 162'000'000};
    for (uint32_t f : freqs) {
        CAPTURE(f);
        const Si5351Plan p = si5351Plan(kXtal, f);
        REQUIRE(p.valid);
        CHECK(p.vcoHz >= 600'000'000u);
        CHECK(p.vcoHz <= 900'000'000u);
        CHECK(p.msDivider % 2 == 0);
        CHECK(p.pllA >= 15);
        CHECK(p.pllA <= 90);
        CHECK(p.pllB < p.pllC);
        CHECK(errorPpb(p) < 50); // well under the 0.07 ppm a frame lock needs to hold
    }
}

TEST_CASE("above 150 MHz the divider is the divide-by-4 mode") {
    CHECK(si5351Plan(kXtal, 150'000'001).msDivider == 4);
    CHECK(si5351Plan(kXtal, 162'000'000).msDivider == 4);
}

TEST_CASE("out-of-range requests are refused") {
    CHECK_FALSE(si5351Plan(kXtal, 0).valid);
    CHECK_FALSE(si5351Plan(kXtal, 250'000'000).valid);
}

TEST_CASE("AN619 parameter encoding") {
    uint32_t p1, p2, p3;
    si5351Params(36, 0, 1, p1, p2, p3); // 900 MHz from 25 MHz, integer
    CHECK(p1 == 128 * 36 - 512);
    CHECK(p2 == 0);
    CHECK(p3 == 1);
    si5351Params(33, 1, 2, p1, p2, p3); // 33.5
    CHECK(p1 == 128 * 33 + 64 - 512);
    CHECK(p2 == 0);
    CHECK(p3 == 2);
}

TEST_CASE("a fine change within the same divider rewrites only the PLL, without a reset") {
    test::FakeI2cBus bus;
    test::FakeFlatDevice dev;
    bus.attach(Si5351::kAddress, dev);
    Si5351 si(bus, kXtal);
    REQUIRE(si.begin());
    REQUIRE(si.setFrequency(138'514'147));
    dev.writes.clear();
    REQUIRE(si.setFrequency(138'514'200)); // a frame-lock sized nudge
    for (const auto& w : dev.writes) {
        CHECK(w.first >= 26);
        CHECK(w.first <= 33); // PLLA registers only
    }
}

TEST_CASE("a divider change reprograms the MultiSynth and resets the PLL") {
    test::FakeI2cBus bus;
    test::FakeFlatDevice dev;
    bus.attach(Si5351::kAddress, dev);
    Si5351 si(bus, kXtal);
    REQUIRE(si.begin());
    REQUIRE(si.setFrequency(81'000'000));
    bool sawReset = false;
    for (const auto& w : dev.writes) {
        sawReset |= (w.first == 177 && w.second == 0x20);
    }
    CHECK(sawReset);
    CHECK((dev.regs[16] & 0x0C) == 0x0C); // CLK0 sourced from its MultiSynth
}

TEST_CASE("the output enable register gates only CLK0") {
    test::FakeI2cBus bus;
    test::FakeFlatDevice dev;
    bus.attach(Si5351::kAddress, dev);
    Si5351 si(bus, kXtal);
    REQUIRE(si.begin());
    CHECK(dev.regs[3] == 0xFF);
    REQUIRE(si.setOutputEnabled(true));
    CHECK(dev.regs[3] == 0xFE);
}
