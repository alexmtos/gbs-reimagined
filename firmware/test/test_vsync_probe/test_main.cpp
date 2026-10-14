#include <vector>

#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/tv5725.h"
#include "video/sense.h"

using namespace gbsr;

namespace {

// The sync processor's line count (S0_1B..1C, 11 bits) as a scripted sequence of reads.
class LinesTv5725 : public test::FakeTv5725 {
public:
    std::vector<uint16_t> lines;
    size_t next = 0;

    bool read(uint8_t reg, uint8_t* out, size_t len) override {
        if (segment == 0 && reg == 0x1B) {
            const uint16_t v = next < lines.size() ? lines[next++] : 0;
            regs[0][0x1B] = static_cast<uint8_t>(v & 0xFF);
            regs[0][0x1C] = static_cast<uint8_t>((regs[0][0x1C] & 0xF8) | (v >> 8));
        }
        return FakeTv5725::read(reg, out, len);
    }
};

struct Rig {
    test::FakeI2cBus bus;
    LinesTv5725 dev;
    tv5725::Chip chip{bus};
    video::LockState lock;
    video::Ops ops{chip, lock};
    video::VsyncProber prober;
    uint32_t now = 1000;
    uint32_t longestGapMs = 0;
    int steps = 0;

    Rig() { bus.attach(tv5725::Chip::kAddress, dev); }

    video::VsyncProbe run() {
        dev.at(5, 0x20) = 0x08; // SP_EXT_SYNC_SEL set, as the detector leaves it
        dev.at(5, 0x56) = 0x01; // SOG mode on
        prober.start(ops, now);
        while (prober.step(ops, now) && steps < 100) {
            now += 20; // one engine tick
            ++steps;
        }
        return prober.result();
    }
};

} // namespace

TEST_CASE("a steady line count is a discrete vsync, measured across ticks") {
    Rig r;
    r.dev.lines = {524, 524, 525, 524, 523, 524, 524, 525, 524};
    const video::VsyncProbe p = r.run();
    CHECK(p.found);
    CHECK(p.lines == 524);
    CHECK(r.steps >= 13 + 8); // 260 ms of settling, then one read per 20 ms tick
    // The HV input and SOG mode are put back as found.
    CHECK(r.dev.at(5, 0x20) == 0x08);
    CHECK(r.dev.at(5, 0x56) == 0x01);
}

TEST_CASE("three zero reads end the probe early: no H lock") {
    Rig r;
    r.dev.lines = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    const video::VsyncProbe p = r.run();
    CHECK_FALSE(p.found);
    CHECK(r.dev.next == 3);
}

TEST_CASE("the chip's VS-active flag is believed") {
    Rig r;
    r.dev.at(0, 0x16) = 0x08;
    r.dev.lines = {0, 0, 0};
    CHECK(r.run().found);
}

TEST_CASE("a wandering line count is not a vsync") {
    Rig r;
    r.dev.lines = {262, 300, 410, 262, 520, 263, 330, 700, 262};
    CHECK_FALSE(r.run().found);
}
