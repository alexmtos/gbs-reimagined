#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/fields.gen.h"
#include "chip/programs.h"
#include "chip/tv5725.h"
#include "video/geometry.h"
#include "video/raster.h"

using namespace gbsr;
using video::Aspect;
using video::GeometryAdjust;
using video::GeometryRegs;
using video::Standard;
using video::Timing;

namespace {

// A program's baseline, read from its image as a load leaves it (as in test_geometry).
video::GeometryBase programBase(tv5725::ProgramId id, Standard standard, uint16_t vperiodIf) {
    test::FakeI2cBus bus;
    test::FakeTv5725 dev;
    tv5725::Chip chip{bus};
    bus.attach(tv5725::Chip::kAddress, dev);
    const tv5725::ProgramInfo info = tv5725::programInfo(id);
    const uint8_t* p = info.records;
    const uint8_t* end = info.records + info.recordsSize;
    while (p < end) {
        const uint8_t seg = *p++;
        const uint8_t start = *p++;
        const uint8_t count = *p++;
        for (uint8_t i = 0; i < count; ++i) {
            dev.at(seg, static_cast<uint8_t>(start + i)) = *p++;
        }
    }
    const uint16_t raw = static_cast<uint16_t>(vperiodIf << 1);
    dev.at(0, 0x07) = static_cast<uint8_t>(raw & 0xFF);
    dev.at(0, 0x08) = static_cast<uint8_t>((dev.at(0, 0x08) & 0xF0) | (raw >> 8));
    video::PathContext ctx;
    ctx.standard = standard;
    ctx.presetId = info.presetId;
    return video::readGeometryBase(chip, ctx);
}

Timing timing(const char* name) {
    Timing t{};
    REQUIRE(video::timingByName(name, t));
    return t;
}

// The 4:3 picture's width as a share of what the converter shows, in 1/1000. The window shows
// the capture, which may hold a little less than the 4:3 picture (the fetch limit).
int widthShare(const video::GeometryBase& b, const GeometryRegs& g) {
    const int visible = (b.htotal + 1) * b.frameActive / b.frameTotal;
    const int window = g.disHbSt - g.disHbSp;
    const int capture = window * g.hscale / 1024;
    return window * b.width43 / capture * 1000 / visible;
}

GeometryAdjust integer() {
    GeometryAdjust adj;
    adj.integer = true;
    return adj;
}

} // namespace

TEST_CASE("the timing table has the standards' totals") {
    CHECK(timing("1080p").hTotal() == 2200);
    CHECK(timing("1080p").vTotal() == 1125);
    CHECK(timing("1024x768").hTotal() == 1344);
    CHECK(timing("1024x768").vTotal() == 806);
    CHECK(timing("800x600").hTotal() == 1056);
    CHECK(timing("1600x1200").vTotal() == 1235);
    CHECK(timing("1920x1200").hTotal() == 2080);
    Timing t{};
    CHECK_FALSE(video::timingByName("640x400", t));
}

TEST_CASE("1080p rebuilt from its standard on the program's own line matches the program") {
    const video::GeometryBase prog = programBase(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc, 525);
    REQUIRE(prog.valid);
    const video::Raster r = video::makeRaster(prog, timing("1080p"), 1602);
    REQUIRE(r.base.valid);
    CHECK(r.regs.hsyncRst == 1601);
    CHECK(r.regs.vsyncRst == 1124);
    CHECK(r.regs.hsSt < r.regs.hsSp); // positive
    CHECK(r.regs.vsSt < r.regs.vsSp);
    const GeometryRegs fromProgram = video::computeGeometry(prog, {}, prog.htotal);
    const GeometryRegs fromRaster = video::computeGeometry(r.base, {}, r.base.htotal);
    CHECK(fromRaster.vscale == fromProgram.vscale);
    CHECK(fromRaster.hscale == fromProgram.hscale);
    CHECK(fromRaster.disHbSt - fromRaster.disHbSp == fromProgram.disHbSt - fromProgram.disHbSp);
    // The program's window reaches 5 lines into the blanking; the standard's active area does not.
    CHECK(fromRaster.disVbSt - fromRaster.disVbSp == 1080);
    CHECK(fromProgram.disVbSt - fromProgram.disVbSp == 1085);
    // Centred in the standard's active area, a few units from where the program put it.
    const int centreR = (fromRaster.disHbSt + fromRaster.disHbSp) / 2;
    const int centreP = (fromProgram.disHbSt + fromProgram.disHbSp) / 2;
    CHECK(centreR == doctest::Approx(centreP).epsilon(0.02));
}

TEST_CASE("1024x768 has negative syncs, and twice the units per pixel give the scaler room") {
    const video::GeometryBase prog = programBase(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc, 525);
    const video::Raster r = video::makeRaster(prog, timing("1024x768"), 2 * 1344);
    REQUIRE(r.base.valid);
    CHECK(r.regs.hsyncRst == 2687);
    CHECK(r.regs.vsyncRst == 805);
    CHECK(r.regs.hsSt > r.regs.hsSp); // negative: ST > SP
    CHECK(r.regs.hsSt - r.regs.hsSp == 272);
    CHECK(r.regs.vsSt > r.regs.vsSp);
    CHECK(r.regs.vsynSize == 807);
    const GeometryRegs g = video::computeGeometry(r.base, {}, r.base.htotal);
    CHECK(g.vscale == 640);                                              // 480 doubled lines in 768
    CHECK(widthShare(r.base, g) == doctest::Approx(1000).epsilon(0.01)); // 4:3 fills 4:3
    CHECK(g.hscale < 1024);
    CHECK(g.hbSp > r.base.syncEnd); // DMT's back porch leaves the fetch clear of the sync
}

TEST_CASE("800x600 needs more units per pixel than one, as the scaler cannot shrink") {
    const video::GeometryBase prog = programBase(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc, 525);
    CHECK_FALSE(video::makeRaster(prog, timing("800x600"), 1056).base.valid);
    const video::Raster r = video::makeRaster(prog, timing("800x600"), 3 * 1056);
    REQUIRE(r.base.valid);
    const GeometryRegs g = video::computeGeometry(r.base, {}, r.base.htotal);
    CHECK(widthShare(r.base, g) == doctest::Approx(1000).epsilon(0.01));
    CHECK(g.vscale == 819); // 480 in 600
}

TEST_CASE("1920x1200 shows 4:3 pillarboxed at 16:10, and integer scale gives 5x for 240p") {
    const video::GeometryBase prog = programBase(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc, 525);
    const video::Raster r = video::makeRaster(prog, timing("1920x1200"), 2080);
    REQUIRE(r.base.valid);
    CHECK(r.regs.hsSt < r.regs.hsSp); // CVT-RB: +H
    CHECK(r.regs.vsSt > r.regs.vsSp); //         -V
    const GeometryRegs g = video::computeGeometry(r.base, {}, r.base.htotal);
    CHECK(widthShare(r.base, g) == doctest::Approx(1000 * 4 * 10 / (3 * 16)).epsilon(0.01));
    const GeometryRegs gi = video::computeGeometry(r.base, integer(), r.base.htotal);
    CHECK(gi.vscale == 410); // 1024 x 480 / 1200 = 409.6: 2.5x the doubled lines, 5x 240p
}

TEST_CASE("a longer line keeps the picture, then lets 1080p narrow to 4:3 of 960 lines") {
    const video::GeometryBase prog = programBase(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc, 525);
    const GeometryRegs plain = video::computeGeometry(prog, {}, prog.htotal);
    const GeometryRegs plainInt = video::computeGeometry(prog, integer(), prog.htotal);
    // On the program's line the integer picture cannot narrow: it stays as wide as 1080 lines'.
    CHECK(widthShare(prog, plainInt) == widthShare(prog, plain));

    const uint16_t longer = 1801; // 1802 units, 9/8 of 1602
    const video::GeometryBase s = video::stretchBase(prog, longer);
    REQUIRE(s.valid);
    CHECK(s.htotal == longer);
    const GeometryRegs same = video::computeGeometry(s, {}, longer);
    CHECK(widthShare(s, same) == doctest::Approx(widthShare(prog, plain)).epsilon(0.01));
    const GeometryRegs narrow = video::computeGeometry(s, integer(), longer);
    CHECK(narrow.vscale == 512);
    CHECK(narrow.hscale <= 1024);
    // 4:3 of 960 lines on a 16:9 frame of 1080: 1280 of 1920.
    CHECK(widthShare(s, narrow) == doctest::Approx(667).epsilon(0.01));
    CHECK(video::stretchPosition(800, 1601, 1801) == 900);
}

TEST_CASE("each extra mode runs on the fastest internal clock that leaves 16:9 room") {
    const video::GeometryBase prog = programBase(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc, 525);
    struct Case {
        const char* name;
        uint8_t code;
    };
    const Case cases[] = {
        {"1024x768", 0x65}, {"800x600", 0x55}, {"1600x1200", 0x85}, {"1920x1200", 0x95}};
    for (const Case& c : cases) {
        CAPTURE(c.name);
        const Timing t = timing(c.name);
        const video::LoadRaster lr = video::rasterForMode(prog, t, 60'000);
        REQUIRE(lr.valid);
        CHECK(lr.code == c.code);
        // The internal clock alone runs the raster at 60 Hz, within a line.
        const uint32_t hz = video::displayClockHz(lr.code);
        const uint32_t units = lr.raster.regs.hsyncRst + 1u;
        CHECK(static_cast<double>(hz) / (units * t.vTotal()) ==
              doctest::Approx(60.0).epsilon(0.002));
        CHECK(video::computeGeometry(lr.raster.base, {}, lr.raster.base.htotal).hscale >= 683);
        GeometryAdjust wide;
        wide.aspect = Aspect::SixteenNine;
        CHECK(video::computeGeometry(lr.raster.base, wide, lr.raster.base.htotal).hscale >= 512);
    }
    // 50 Hz sources get the line for 50 Hz: a fifth more units at the same clock.
    const video::GeometryBase pal = programBase(tv5725::ProgramId::Pal1080p, Standard::Pal, 625);
    const video::LoadRaster lr50 = video::rasterForMode(pal, timing("1024x768"), 50'000);
    REQUIRE(lr50.valid);
    const uint32_t hz = video::displayClockHz(lr50.code);
    CHECK(static_cast<double>(hz) / ((lr50.raster.regs.hsyncRst + 1u) * 806) ==
          doctest::Approx(50.0).epsilon(0.002));
}

TEST_CASE("1080p gets a longer line only when the picture would be over 2 % too wide") {
    const video::GeometryBase ntsc = programBase(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc, 525);
    CHECK_FALSE(video::stretchFor(ntsc, {}, 0x85).valid); // 4:3 asks for 1025.5: left alone
    GeometryAdjust wide;
    wide.aspect = Aspect::SixteenNine;
    CHECK_FALSE(video::stretchFor(ntsc, wide, 0x85).valid);

    const video::LoadStretch i = video::stretchFor(ntsc, integer(), 0x85);
    REQUIRE(i.valid);
    CHECK(i.code == 0x95);       // 129.6 MHz
    CHECK(i.htotal == 1922 - 1); // 1602 x 129.6 / 108
    const GeometryRegs gi = video::computeGeometry(i.base, integer(), i.htotal);
    CHECK(gi.vscale == 512);
    CHECK(gi.hscale <= 1014);
    CHECK(widthShare(i.base, gi) == doctest::Approx(667).epsilon(0.01)); // 4:3 of 960 lines

    const video::GeometryBase pal = programBase(tv5725::ProgramId::Pal1080p, Standard::Pal, 625);
    const video::LoadStretch p = video::stretchFor(pal, {}, 0x65);
    REQUIRE(p.valid);
    CHECK(p.code == 0x85); // 81 -> 108 MHz
    const GeometryRegs gp = video::computeGeometry(p.base, {}, p.htotal);
    CHECK(widthShare(p.base, gp) == doctest::Approx(750).epsilon(0.01)); // exact 4:3 at 1080
    const video::LoadStretch pi = video::stretchFor(pal, integer(), 0x65);
    REQUIRE(pi.valid);
    CHECK(pi.code == 0x95);
    // The user's size never asks for a longer line by itself.
    GeometryAdjust narrower;
    narrower.hSize = -40;
    CHECK_FALSE(video::stretchFor(ntsc, narrower, 0x85).valid);
}

TEST_CASE("every extra mode also fits a PAL source's 1080p program at 50 Hz") {
    const video::GeometryBase pal = programBase(tv5725::ProgramId::Pal1080p, Standard::Pal, 625);
    REQUIRE(pal.valid);
    for (const char* name : {"1024x768", "800x600", "1600x1200", "1920x1200"}) {
        CAPTURE(name);
        const Timing t = timing(name);
        const video::LoadRaster lr = video::rasterForMode(pal, t, 50'000);
        REQUIRE(lr.valid);
        const uint32_t hz = video::displayClockHz(lr.code);
        CHECK(static_cast<double>(hz) / ((lr.raster.regs.hsyncRst + 1u) * t.vTotal()) ==
              doctest::Approx(50.0).epsilon(0.002));
        CHECK(lr.raster.base.vbSp < lr.raster.base.disVbSp); // the fetch starts in the frame
        CHECK(lr.raster.base.vbSt < lr.raster.base.disVbSp);
        CHECK(video::computeGeometry(lr.raster.base, {}, lr.raster.base.htotal).hscale >= 683);
    }
}

TEST_CASE("only the 1080p programs ever ask for a longer line") {
    struct Case {
        tv5725::ProgramId id;
        Standard standard;
        uint16_t vperiod;
    };
    using P = tv5725::ProgramId;
    const Case cases[] = {
        {P::Ntsc960p, Standard::Ntsc, 525}, {P::Ntsc1024p, Standard::Ntsc, 525},
        {P::Ntsc720p, Standard::Ntsc, 525}, {P::Ntsc480p, Standard::Ntsc, 525},
        {P::Pal960p, Standard::Pal, 625},   {P::Pal1024p, Standard::Pal, 625},
        {P::Pal720p, Standard::Pal, 625},   {P::Pal576p, Standard::Pal, 625},
    };
    for (const Case& c : cases) {
        CAPTURE(static_cast<int>(c.id));
        const video::GeometryBase b = programBase(c.id, c.standard, c.vperiod);
        const uint8_t code = tv5725::programInfo(c.id).displayClock;
        CHECK_FALSE(video::stretchFor(b, {}, code).valid);
        CHECK_FALSE(video::stretchFor(b, integer(), code).valid);
    }
    const video::GeometryBase ntsc = programBase(P::Ntsc1080p, Standard::Ntsc, 525);
    CHECK(video::stretchFor(ntsc, integer(), tv5725::programInfo(P::Ntsc1080p).displayClock).valid);
    const video::GeometryBase pal = programBase(P::Pal1080p, Standard::Pal, 625);
    CHECK(video::stretchFor(pal, {}, tv5725::programInfo(P::Pal1080p).displayClock).valid);
}
