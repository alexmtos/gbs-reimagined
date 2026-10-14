#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/fields.gen.h"
#include "chip/programs.h"
#include "chip/tv5725.h"
#include "video/scanlines.h"

using namespace gbsr;
using namespace gbsr::tv5725::fields;

namespace {

struct Rig {
    test::FakeI2cBus bus;
    test::FakeTv5725 dev;
    tv5725::Chip chip{bus};

    Rig() {
        bus.attach(tv5725::Chip::kAddress, dev);
        // The idle deinterlacer a program load leaves (section-deinterlacer, VIIR on its default).
        chip.write(DEINT_00, 0xFF);
        chip.write(MADPT_Y_MI_OFFSET, 0);
        chip.write(MAPDT_VT_SEL_PRGV, 1);
        chip.write(MADPT_PD_RAM_BYPS, 1);
        chip.write(MADPT_VIIR_BYPS, 1);
        chip.write(MADPT_VIIR_COEF, 12);
        chip.write(VDS_W_LEV_BYPS, 1);
    }
};

} // namespace

TEST_CASE("the strength levels span the reference's range, level 5 being its default") {
    CHECK(video::scanlineOffset(1) == 0x70);
    CHECK(video::scanlineOffset(4) == 0x40);
    CHECK(video::scanlineOffset(5) == 0x30); // reference default
    CHECK(video::scanlineOffset(8) == 0x00); // darkest
    CHECK(video::scanlineOffset(0) == 0x70); // out of range: clamped
    CHECK(video::scanlineOffset(9) == 0x00);
}

TEST_CASE("on: the mixer is forced to the level's motion index on luma and chroma") {
    Rig r;
    video::enableScanlines(r.chip, 4);
    CHECK(r.chip.get(MADPT_Y_MI_OFFSET) == 0x40);
    CHECK(r.chip.get(MADPT_UV_MI_OFFSET) == 0x40);
    CHECK(r.chip.get(MADPT_Y_MI_DET_BYPS) == 1);
    CHECK(r.chip.get(MADPT_UV_MI_DET_BYPS) == 1);
    CHECK(r.chip.get(MAPDT_VT_SEL_PRGV) == 0);
    CHECK(r.chip.get(RFF_LINE_FLIP) == 1);
    CHECK(r.chip.get(MADPT_PD_RAM_BYPS) == 0);
    CHECK(r.chip.get(MADPT_VIIR_BYPS) == 0);
    CHECK(r.chip.get(MADPT_VIIR_COEF) == 8);
    CHECK(r.chip.get(VDS_WLEV_GAIN) == 8);
    CHECK(r.chip.get(VDS_W_LEV_BYPS) == 0);
    // The path select goes last, as in the reference.
    const auto& w = r.dev.writes;
    REQUIRE_FALSE(w.empty());
    CHECK(w.back().first ==
          static_cast<uint16_t>(MAPDT_VT_SEL_PRGV.seg << 8 | MAPDT_VT_SEL_PRGV.addr));
}

TEST_CASE("a strength change moves only the two motion indexes") {
    Rig r;
    video::enableScanlines(r.chip, 4);
    r.dev.writes.clear();
    video::setScanlineStrength(r.chip, 8);
    CHECK(r.chip.get(MADPT_Y_MI_OFFSET) == 0);
    CHECK(r.chip.get(MADPT_UV_MI_OFFSET) == 0);
    for (const auto& [where, value] : r.dev.writes) {
        (void)value;
        const bool ok = where == (MADPT_Y_MI_OFFSET.seg << 8 | MADPT_Y_MI_OFFSET.addr) ||
                        where == (MADPT_UV_MI_OFFSET.seg << 8 | MADPT_UV_MI_OFFSET.addr);
        CHECK(ok);
    }
}

TEST_CASE("off puts back exactly what the program left, field by field") {
    Rig r;
    // A distinct value in every field, so a restore into the wrong field shows.
    r.chip.write(MADPT_UVDLY_PD_SP, 3);
    r.chip.write(MADPT_UVDLY_PD_ST, 5);
    r.chip.write(MADPT_EN_UV_DEINT, 0);
    r.chip.write(MADPT_UV_MI_DET_BYPS, 0);
    r.chip.write(MADPT_UV_MI_OFFSET, 0x21);
    r.chip.write(MADPT_MO_ADP_UV_EN, 0);
    r.chip.write(DIAG_BOB_PLDY_RAM_BYPS, 1);
    r.chip.write(RFF_YUV_DEINTERLACE, 0);
    r.chip.write(MADPT_Y_MI_DET_BYPS, 0);
    r.chip.write(VDS_WLEV_GAIN, 0x33);
    r.chip.write(MADPT_Y_MI_OFFSET, 0x15);
    r.chip.write(RFF_LINE_FLIP, 0);
    const video::ScanlineBase base = video::readScanlineBase(r.chip);
    REQUIRE(base.valid);
    video::enableScanlines(r.chip, 6);
    CHECK_FALSE(video::readScanlineBase(r.chip) == base);
    video::disableScanlines(r.chip, base);
    CHECK(video::readScanlineBase(r.chip) == base);
}

TEST_CASE("a bus error while capturing voids the baseline") {
    Rig r;
    r.bus.failNext = true;
    CHECK_FALSE(video::readScanlineBase(r.chip).valid);
    CHECK(video::readScanlineBase(r.chip).valid);
}
