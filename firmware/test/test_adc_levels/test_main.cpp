#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/fields.gen.h"
#include "chip/tv5725.h"
#include "platform/time.h"
#include "video/adc_levels.h"
#include "video/ops.h"

using namespace gbsr;

namespace {

// The decimator's test output as the offset calibration sees it: near zero once the selected
// channel's offset reaches that channel's black level, far above zero below it.
class CalibratingTv5725 : public test::FakeTv5725 {
public:
    uint8_t blackLevel[3] = {0x3F, 0x3E, 0x41}; // R, G, B

    bool read(uint8_t reg, uint8_t* out, size_t len) override {
        if (segment == 0) {
            // DEC_TEST_SEL (S5_1F bits 4..6) picks 1 G, 2 R, 3 B; offsets are S5_06 R, 07 G, 08 B.
            static constexpr uint8_t kOffsetAddr[4] = {0, 0x07, 0x06, 0x08};
            static constexpr uint8_t kChannel[4] = {0, 1, 0, 2};
            const uint8_t sel = (regs[5][0x1F] >> 4) & 0x07;
            uint16_t v = 0x0200;
            if (sel >= 1 && sel <= 3 && regs[5][kOffsetAddr[sel]] >= blackLevel[kChannel[sel]]) {
                v = 3;
            }
            regs[0][0x2E] = static_cast<uint8_t>(v & 0xFF);
            regs[0][0x2F] = static_cast<uint8_t>(v >> 8);
        }
        return FakeTv5725::read(reg, out, len);
    }
};

struct Rig {
    test::FakeI2cBus bus;
    CalibratingTv5725 dev;
    tv5725::Chip chip{bus};
    video::LockState lock;
    video::Ops ops{chip, lock};
    video::OffsetCalibrator cal;
    uint32_t longestStepMs = 0;
    int steps = 0;

    Rig() { bus.attach(tv5725::Chip::kAddress, dev); }

    void run() {
        cal.start(ops);
        while (steps < 2000) {
            const uint32_t t0 = platform::nowMs();
            const bool more = cal.step(ops);
            const uint32_t took = platform::nowMs() - t0;
            longestStepMs = took > longestStepMs ? took : longestStepMs;
            ++steps;
            if (!more) {
                break;
            }
            platform::advanceMs(8); // the rest of a 20 ms main-loop tick
        }
    }
};

} // namespace

TEST_CASE("the offset calibration finds each channel's black level") {
    Rig r;
    r.run();
    REQUIRE_FALSE(r.cal.busy());
    CHECK_FALSE(r.cal.failed());
    CHECK(r.lock.adcOffset[0] == 0x3F);
    CHECK(r.lock.adcOffset[1] == 0x3E);
    CHECK(r.lock.adcOffset[2] == 0x41);
    // Applied to the chip, with the gain back at the lock state's value.
    CHECK(r.dev.at(5, 0x06) == 0x3F);
    CHECK(r.dev.at(5, 0x07) == 0x3E);
    CHECK(r.dev.at(5, 0x08) == 0x41);
    CHECK(r.dev.at(5, 0x09) == r.lock.adcGain);
    // Measured on the unconnected input.
    bool isolated = false;
    for (const auto& w : r.dev.writes) {
        isolated |= w.first == (5 << 8 | 0x02) && (w.second >> 6) == 2;
    }
    CHECK(isolated);
    // Each step stays a short slice of the main loop.
    CHECK(r.longestStepMs <= 13);
}

TEST_CASE("a channel that never reads black falls back to the default offsets") {
    Rig r;
    r.dev.blackLevel[2] = 0x60; // above the 0x52 ceiling
    r.run();
    REQUIRE_FALSE(r.cal.busy());
    CHECK(r.cal.failed());
    for (uint8_t i = 0; i < 3; ++i) {
        CHECK(r.lock.adcOffset[i] == 0x40);
    }
    CHECK(r.dev.at(5, 0x08) == 0x40);
}

TEST_CASE("an isolated miss does not raise the offset") {
    // Three misses in a row are tolerated; only the fourth raises the offset.
    struct Glitchy : CalibratingTv5725 {
        int reads = 0;
        bool read(uint8_t reg, uint8_t* out, size_t len) override {
            const bool ok = CalibratingTv5725::read(reg, out, len);
            if (segment == 0 && reg == 0x2E && ++reads % 50 < 3) {
                out[0] = 0x40; // glitch: three bad reads every fifty
            }
            return ok;
        }
    };
    test::FakeI2cBus bus;
    Glitchy dev;
    dev.blackLevel[0] = dev.blackLevel[1] = dev.blackLevel[2] = 0x3D; // black at the start value
    bus.attach(tv5725::Chip::kAddress, dev);
    tv5725::Chip chip{bus};
    video::LockState lock;
    video::Ops ops{chip, lock};
    video::OffsetCalibrator cal;
    cal.start(ops);
    for (int i = 0; i < 2000 && cal.step(ops); ++i) {
        platform::advanceMs(8);
    }
    CHECK(lock.adcOffset[0] == 0x3D);
    CHECK(lock.adcOffset[1] == 0x3D);
    CHECK(lock.adcOffset[2] == 0x3D);
}
