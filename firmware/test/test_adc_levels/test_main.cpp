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
    CHECK(r.dev.at(5, 0x09) == r.lock.adcGain[0]);
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

namespace {

// A source showing a field of uniform colour, as the decimator test output reports it: per read,
// either a blanking sample (level 0) or the field, in a fixed pattern of twenty reads. The field's
// level per channel follows the gain response measured on the bench, level ∝ 1 / (199 + register)
// (docs/bench/M1.md), and clips at 0x7F; bit 7 of the high byte toggles on every read. Blue reads
// as a colour difference (0x40) unless the matrix is bypassed.
class FieldTv5725 : public test::FakeTv5725 {
public:
    // Level each channel would reach at the preset gain 0x7B if the ADC did not clip (R, G, B).
    uint16_t levelAtPreset[3] = {0x90, 0x88, 0x84};
    uint8_t fieldReadsOfTwenty = 13; // the rest is blanking
    int reads = 0;

    uint8_t level(uint8_t channel) const {
        const uint32_t range = 199u + regs[5][0x09 + channel];
        const uint32_t v = static_cast<uint32_t>(levelAtPreset[channel]) * (199u + 0x7B) / range;
        return static_cast<uint8_t>(v > 0x7F ? 0x7F : v);
    }

    bool read(uint8_t reg, uint8_t* out, size_t len) override {
        if (segment == 0 && reg <= 0x2F && reg + len > 0x2F) {
            const uint8_t sel = (regs[5][0x1F] >> 4) & 0x07;
            const bool matrixBypassed = (regs[5][0x1F] >> 2) & 1;
            static constexpr int8_t kChannel[8] = {-1, 1, 0, 2, -1, -1, -1, -1};
            uint8_t v = 0;
            const int8_t c = kChannel[sel];
            if (c >= 0 && reads % 20 < fieldReadsOfTwenty) {
                v = (c == 2 && !matrixBypassed) ? 0x40 : level(static_cast<uint8_t>(c));
            }
            regs[0][0x2F] = static_cast<uint8_t>(v | ((reads & 1) << 7));
            ++reads;
        }
        return FakeTv5725::read(reg, out, len);
    }
};

struct GainRig {
    test::FakeI2cBus bus;
    FieldTv5725 dev;
    tv5725::Chip chip{bus};
    video::LockState lock;
    video::Ops ops{chip, lock};
    video::GainCalibrator cal;

    GainRig() {
        bus.attach(tv5725::Chip::kAddress, dev);
        ops.applyAdcLevels();
        dev.at(5, 0x1F) = 0x90; // as a running RGB program leaves it: matrix on, test output off
        dev.at(0, 0x4D) = 0x0A; // test bus on the sync processor, as the SOG search leaves it
    }

    void calibrate(bool ypbpr) {
        cal.start(ops, ypbpr);
        for (int i = 0; i < 5000 && cal.step(ops); ++i) {
            platform::advanceMs(8);
        }
    }
};

constexpr uint8_t kTarget = video::GainCalibrator::kWhiteTarget;

} // namespace

TEST_CASE("calibrating on a white field puts each channel's white just under the target") {
    GainRig r;
    r.calibrate(false);
    REQUIRE(r.cal.outcome() == video::GainCalibrator::Outcome::Done);
    for (uint8_t c = 0; c < 3; ++c) {
        const uint8_t gain = r.lock.adcGain[c];
        CHECK(r.dev.at(5, static_cast<uint8_t>(0x09 + c)) == gain);
        CHECK(r.dev.level(c) <= kTarget);
        r.dev.at(5, static_cast<uint8_t>(0x09 + c)) = static_cast<uint8_t>(gain - 1);
        CHECK(r.dev.level(c) > kTarget); // one step more gain would pass the target
        r.dev.at(5, static_cast<uint8_t>(0x09 + c)) = gain;
    }
    CHECK(r.lock.adcGain[0] > r.lock.adcGain[2]); // the hottest channel gets the least gain
    // Everything the taps borrowed is back.
    CHECK(r.dev.at(5, 0x1F) == 0x90);
    CHECK((r.dev.at(0, 0x4D) & 0x1F) == 0x0A);
}

TEST_CASE("a picture without a large bright area is refused and the gains stay") {
    GainRig r;
    r.dev.fieldReadsOfTwenty = 2; // a small white logo on black
    r.calibrate(false);
    CHECK(r.cal.outcome() == video::GainCalibrator::Outcome::NotWhite);
    for (uint8_t c = 0; c < 3; ++c) {
        CHECK(r.lock.adcGain[c] == 0x7B);
        CHECK(r.dev.at(5, static_cast<uint8_t>(0x09 + c)) == 0x7B);
    }
}

TEST_CASE("a dark field is refused rather than amplified") {
    GainRig r;
    for (uint16_t& l : r.dev.levelAtPreset) {
        l = 0x30; // a grey field: even the most gain cannot bring it to the target
    }
    r.calibrate(false);
    CHECK(r.cal.outcome() == video::GainCalibrator::Outcome::NotWhite);
    CHECK(r.lock.adcGain[1] == 0x7B);
}

TEST_CASE("a signal that clips even at the least gain is refused") {
    GainRig r;
    r.dev.levelAtPreset[1] = 0x200;
    r.calibrate(false);
    CHECK(r.cal.outcome() == video::GainCalibrator::Outcome::TooHot);
    CHECK(r.lock.adcGain[1] == 0x7B);
}

TEST_CASE("component sources calibrate Y and give Pb and Pr the same gain") {
    GainRig r;
    r.calibrate(true);
    REQUIRE(r.cal.outcome() == video::GainCalibrator::Outcome::Done);
    CHECK(r.dev.level(1) <= kTarget);
    CHECK(r.lock.adcGain[0] == r.lock.adcGain[1]);
    CHECK(r.lock.adcGain[2] == r.lock.adcGain[1]);
}

TEST_CASE("an abort puts back the gains in effect before the calibration") {
    GainRig r;
    r.lock.adcGain[1] = 0x90;
    r.ops.applyAdcLevels();
    r.cal.start(r.ops, false);
    for (int i = 0; i < 30; ++i) {
        r.cal.step(r.ops);
        platform::advanceMs(8);
    }
    REQUIRE(r.cal.busy());
    r.cal.abort(r.ops);
    CHECK(r.cal.outcome() == video::GainCalibrator::Outcome::Aborted);
    CHECK(r.lock.adcGain[1] == 0x90);
    CHECK(r.dev.at(5, 0x0A) == 0x90);
}

TEST_CASE("auto gain lowers all three gains while green clips, and leaves a clean picture alone") {
    GainRig r;
    video::AutoGain ag;
    ag.restart();
    // Green reaches 0x88 at the preset gain: clipped.
    CHECK(ag.step(r.ops));
    CHECK(r.lock.adcGain[0] == 0x7D);
    CHECK(r.lock.adcGain[1] == 0x7D);
    CHECK(r.lock.adcGain[2] == 0x7D);
    for (int i = 0; i < 200; ++i) {
        ag.step(r.ops);
        platform::advanceMs(20);
    }
    CHECK(r.dev.level(0) < 0x7F); // red and green no longer clip
    CHECK(r.dev.level(1) < 0x7F);
    const uint8_t settled = r.lock.adcGain[1];
    for (int i = 0; i < 50; ++i) {
        CHECK_FALSE(ag.step(r.ops));
        platform::advanceMs(20);
    }
    CHECK(r.lock.adcGain[1] == settled);
}
