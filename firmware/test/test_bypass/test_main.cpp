#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/fields.gen.h"
#include "chip/tv5725.h"
#include "video/bypass.h"

using namespace gbsr;
using namespace gbsr::tv5725::fields;

namespace {

struct Rig {
    test::FakeI2cBus bus;
    test::FakeTv5725 dev;
    tv5725::Chip chip{bus};
    video::LockState lock;
    video::Ops ops{chip, lock};
    video::Sense sense{ops, nullptr};
    video::PathContext ctx;
    Rig() { bus.attach(tv5725::Chip::kAddress, dev); }
};

} // namespace

TEST_CASE("720p over component passes through the HD block to the DAC") {
    Rig r;
    r.ctx.standard = video::Standard::Hd720p;
    r.ctx.inputIsYpbpr = true;
    video::enterHdBypass(r.ops, r.sense, r.ctx);
    CHECK(r.ctx.hdBypass);
    CHECK(r.chip.get(DAC_RGBS_BYPS2DAC) == 1);
    CHECK(r.chip.get(PLL648_CONTROL_01) == 0x35); // display clock unused
    CHECK(r.chip.get(PB_BYPASS) == 1);
    CHECK(r.chip.get(PLLAD_MD) == 2474);
    CHECK(r.chip.get(HD_HSYNC_RST) == 550);
    CHECK(r.chip.get(HD_MATRIX_BYPS) == 0); // YPbPr converted to RGB in the HD block
    CHECK(r.chip.get(OUT_SYNC_SEL) == 1);
    CHECK(r.lock.phaseSp == 8);
    CHECK(r.lock.phaseAdc == 24);
}

TEST_CASE("a 240p source passed through takes its sync from the sync processor") {
    Rig r;
    r.ctx.standard = video::Standard::Ntsc;
    video::enterHdBypass(r.ops, r.sense, r.ctx);
    CHECK(r.chip.get(OUT_SYNC_SEL) == 2);
    CHECK(r.chip.get(HD_MATRIX_BYPS) == 1); // RGB passes as it is
    CHECK(r.chip.get(PLLAD_MD) == 2345);
    CHECK(r.chip.get(HD_HSYNC_RST) == 2345 / 2 + 8);
    CHECK(r.chip.get(HD_HB_ST) == 2345 * 945 / 1000);
    CHECK(r.chip.get(HD_VS_SP) == 522);
}

TEST_CASE("an RGBHV source with separate sync goes from the ADC straight to the DAC") {
    Rig r;
    r.ctx.syncCsync = false;
    video::enterRgbhvBypass(r.ops, r.ctx);
    CHECK(r.ctx.standard == video::Standard::RgbhvBypass);
    CHECK(r.chip.get(DAC_RGBS_ADC2DAC) == 1);
    CHECK(r.chip.get(PLLAD_MD) == 1856);
    CHECK(r.chip.get(SP_SOG_MODE) == 0);
    CHECK(r.chip.get(SP_EXT_SYNC_SEL) == 0);
    CHECK(r.chip.get(SP_H_PULSE_IGNOR) == 0xFF);
    CHECK(r.chip.get(DAC_RGBS_PWDNZ) == 1); // output on at the end
}

TEST_CASE("the H-PLL rate picks the charge-pump band, and only a change rewrites it") {
    Rig r;
    CHECK(video::tuneRgbhvBand(r.ops, 50, 0) == 0); // implausible
    CHECK(video::tuneRgbhvBand(r.ops, 900, 0) == 1);
    CHECK(r.chip.get(PLLAD_KS) == 2);
    CHECK(r.chip.get(PLLAD_FS) == 0);
    CHECK(r.chip.get(PLLAD_ICP) == 6);
    CHECK(video::tuneRgbhvBand(r.ops, 3500, 1) == 4);
    CHECK(r.chip.get(PLLAD_KS) == 0);
    r.chip.write(PLLAD_KS, 3); // a band that did not change leaves the registers alone
    CHECK(video::tuneRgbhvBand(r.ops, 3600, 4) == 4);
    CHECK(r.chip.get(PLLAD_KS) == 3);
    CHECK(video::tuneRgbhvBand(r.ops, 5000, 4) == 5);
    CHECK(r.chip.get(PLLAD_FS) == 1);
}
