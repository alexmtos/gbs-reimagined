#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/fields.gen.h"
#include "chip/programs.h"
#include "chip/tv5725.h"
#include "video/geometry.h"

using namespace gbsr;
using namespace gbsr::tv5725::fields;
using video::Aspect;
using video::GeometryAdjust;
using video::GeometryRegs;
using video::Standard;

namespace {

struct Rig {
    test::FakeI2cBus bus;
    test::FakeTv5725 dev;
    tv5725::Chip chip{bus};
    video::LockState lock;
    video::Ops ops{chip, lock};
    video::Sense sense{ops, nullptr};
    video::PathContext ctx;
    video::GeometryBase base;

    // The program's image straight into the registers, as a load leaves it.
    Rig(tv5725::ProgramId id, Standard standard, uint16_t vperiodIf = 525) {
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
        // VPERIOD_IF, read-only: S0_07 bits 1..11.
        const uint16_t raw = static_cast<uint16_t>(vperiodIf << 1);
        dev.at(0, 0x07) = static_cast<uint8_t>(raw & 0xFF);
        dev.at(0, 0x08) = static_cast<uint8_t>((dev.at(0, 0x08) & 0xF0) | (raw >> 8));
        ctx.standard = standard;
        ctx.presetId = info.presetId;
        base = video::readGeometryBase(chip, ctx);
    }

    GeometryRegs at(const GeometryAdjust& adj) const {
        return video::computeGeometry(base, adj, base.htotal);
    }
};

GeometryAdjust aspect(Aspect a) {
    GeometryAdjust adj;
    adj.aspect = a;
    return adj;
}

int span(const GeometryRegs& g) {
    return (g.disHbSt - g.disHbSp) * g.hscale / 1024;
}

} // namespace

TEST_CASE("4:3 keeps the programs that already show 4:3 exactly as loaded") {
    Rig r960(tv5725::ProgramId::Ntsc960p, Standard::Ntsc);
    REQUIRE(r960.base.valid);
    GeometryRegs g = r960.at({});
    CHECK(g.hscale == 512);
    CHECK(g.vscale == 512);
    CHECK(g.disHbSp == 612);
    CHECK(g.disHbSt == 2594);
    CHECK(g.hbSp == 480);
    CHECK(g.hbSt == 2512);
    CHECK(g.disVbSp == 38);
    CHECK(g.disVbSt == 1000);
    CHECK(g.ifHbSp2 == 116);
    CHECK(g.ifVbSp == 8);
    CHECK(g.ifVbSt == 6);

    Rig r1080(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    g = r1080.at({});
    CHECK(g.hscale == 1024); // 1:1, the scaler bypassed
    CHECK(g.vscale == 455);
    CHECK(g.disHbSp == 348);
    CHECK(g.disHbSt == 1356);
    CHECK(g.hbSp == 264);
    CHECK(g.hbSt == 1298);
    CHECK(g.ifHbSp2 == 104);
    CHECK(g.ifVbSp == 10);
}

TEST_CASE("the frame lock's shorter line moves the same window by half the difference") {
    Rig r(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    const GeometryRegs g = video::computeGeometry(r.base, {}, 1593);
    // Eight units shorter: every edge four earlier, the fetch distances kept.
    CHECK(g.disHbSp == 344);
    CHECK(g.disHbSt == 1352);
    CHECK(g.hbSp == 260);
    CHECK(g.hbSt == 1294);
    CHECK(g.hscale == 1024);
}

TEST_CASE("720p shows 4:3 pillarboxed instead of stretched across the frame") {
    Rig r(tv5725::ProgramId::Ntsc720p, Standard::Ntsc);
    const GeometryRegs g = r.at({});
    CHECK(g.hscale == 705); // the program's 512 filled the 16:9 frame
    CHECK(g.vscale == 683);
    CHECK(g.disHbSt - g.disHbSp == 1438);
    CHECK((g.disHbSt + g.disHbSp) / 2 == doctest::Approx(1331).epsilon(0.002));
    CHECK(span(g) == doctest::Approx(991).epsilon(0.002)); // the same capture, narrower
    CHECK(g.hbSp == g.disHbSp - 132);                      // the program's fetch distances
    // The fetch ends early enough to stay within what playback fetches per line.
    CHECK(g.hbSt == g.hbSp + 1464);
    CHECK((g.hbSt - g.hbSp) * g.hscale / 1024 <= 1008);
}

TEST_CASE("16:9 widens by four thirds on a 16:9 frame") {
    Rig r(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    const GeometryRegs g = r.at(aspect(Aspect::SixteenNine));
    CHECK(g.hscale == 769);
    CHECK(g.disHbSt - g.disHbSp == 1342);
    CHECK(g.vscale == 455);
    CHECK(g.ifHbSp2 == 104); // the whole capture is still shown
    CHECK(g.hbSp == 180 - 84);
    CHECK(g.hbSt == g.hbSp + 1342); // 1008 samples at 769
    // Full is the frame's own shape: the same thing here.
    CHECK(r.at(aspect(Aspect::Full)).hscale == 769);
}

TEST_CASE("16:9 letterboxes on a 4:3 frame, keeping the picture centred") {
    Rig r(tv5725::ProgramId::Ntsc960p, Standard::Ntsc);
    const GeometryRegs g = r.at(aspect(Aspect::SixteenNine));
    CHECK(g.hscale == 512);
    CHECK(g.vscale == 683); // three quarters of the height
    CHECK(g.disVbSt - g.disVbSp == 721);
    CHECK((g.disVbSt + g.disVbSp) / 2 == 519);
    // The fetch blanking moves down with the window; the capture is the program's (the values
    // written by hand on the bench, docs/bench/M2.md).
    CHECK(g.vbSt == 145);
    CHECK(g.vbSp == 147);
    CHECK(g.ifVbSp == 8);
    CHECK(g.ifVbSt == 6);
    CHECK(r.at(aspect(Aspect::Full)) == r.at({}));
}

TEST_CASE("4:3 letterboxes a 5:4 frame to 960 lines; Full fills it") {
    Rig r(tv5725::ProgramId::Ntsc1024p, Standard::Ntsc);
    const GeometryRegs g43 = r.at({});
    CHECK(g43.vscale == 512);
    CHECK(g43.hscale == 512);
    const GeometryRegs full = r.at(aspect(Aspect::Full));
    CHECK(full.vscale == 480);
    CHECK(full.disVbSt - full.disVbSp == 1026); // the program's whole window
}

GeometryAdjust integer(Aspect a = Aspect::FourThree) {
    GeometryAdjust adj = aspect(a);
    adj.integer = true;
    return adj;
}

TEST_CASE("integer scale shows 240p at exactly four lines per line on 1080p") {
    Rig r(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    const GeometryRegs plain = r.at({});
    const GeometryRegs g = r.at(integer());
    CHECK(plain.vscale == 455); // 4.5x
    CHECK(g.vscale == 512);     // 4x: each doubled line twice
    // The window holds the program's capture (482 doubled lines) at the new scale, centred.
    const int captured = (plain.disVbSt - plain.disVbSp) * plain.vscale / 1024;
    CHECK(g.disVbSt - g.disVbSp == 2 * captured);
    CHECK((g.disVbSt + g.disVbSp) / 2 == (plain.disVbSt + plain.disVbSp) / 2);
    // 4:3 of the smaller height would need fewer output units than 1:1 gives: the scaler only
    // enlarges, so on 1080p's own line the width stays (a longer line would make it fit).
    CHECK(plain.hscale == 1024);
    CHECK(g.hscale == 1024);
    // Vertical size cannot undo the multiple; horizontal size still applies.
    GeometryAdjust taller = integer();
    taller.vSize = 20;
    CHECK(r.at(taller) == g);
    GeometryAdjust wider = integer();
    wider.hSize = 40;
    CHECK(r.at(wider).hscale < g.hscale);
    // 16:9 keeps the multiple and narrows with the height, within the scaler's reach.
    const GeometryRegs wide = r.at(integer(Aspect::SixteenNine));
    CHECK(wide.vscale == 512);
    CHECK(wide.hscale == doctest::Approx(769.0 * 1080 / 960).epsilon(0.005));
}

TEST_CASE("integer scale changes nothing where the program is already a whole multiple") {
    Rig r720(tv5725::ProgramId::Ntsc720p, Standard::Ntsc); // 3x
    CHECK(r720.at(integer()) == r720.at({}));
    Rig r960(tv5725::ProgramId::Ntsc960p, Standard::Ntsc); // 4x
    CHECK(r960.at(integer()) == r960.at({}));
    Rig r1024(tv5725::ProgramId::Ntsc1024p, Standard::Ntsc); // 4:3 is already 960 lines
    CHECK(r1024.at(integer()) == r1024.at({}));
}

TEST_CASE("integer scale shows 288p at three lines per line on PAL 1080p, two on 720p") {
    Rig r(tv5725::ProgramId::Pal1080p, Standard::Pal, 625);
    CHECK(r.at(integer()).vscale == 683); // 1024 x 576 / 864: the nearest the divisor gets to 3x
    Rig r720(tv5725::ProgramId::Pal720p, Standard::Pal, 625);
    const GeometryRegs plain = r720.at({});
    const GeometryRegs g = r720.at(integer());
    CHECK(g.vscale == 1024);        // 2x exactly: the doubled lines unscaled
    CHECK(g.hscale > plain.hscale); // 4:3 of the smaller height: narrower
    // At least the source's full 576 lines (the program captures fewer).
    const int captured = (plain.disVbSt - plain.disVbSp) * plain.vscale / 1024;
    CHECK(g.disVbSt - g.disVbSp == doctest::Approx(captured > 576 ? captured : 576).epsilon(0.002));
}

TEST_CASE("position moves the whole window; the content stays the same") {
    Rig r(tv5725::ProgramId::Ntsc960p, Standard::Ntsc);
    GeometryAdjust adj;
    adj.hPosition = 10;
    adj.vPosition = -3;
    const GeometryRegs g = r.at(adj);
    const GeometryRegs g0 = r.at({});
    CHECK(g.disHbSp == g0.disHbSp + 20); // two output units per step
    CHECK(g.disHbSt == g0.disHbSt + 20);
    CHECK(g.hbSp == g0.hbSp + 20);
    CHECK(g.ifHbSp2 == g0.ifHbSp2);
    // One source line per step: two IF lines behind the line doubler, four output lines at 2x.
    CHECK(g.disVbSp == 38 - 12);
    CHECK(g.disVbSt == 1000 - 12);
    CHECK(g.vbSt == 24 - 12);
    CHECK(g.vbSp == 26 - 12);
    CHECK(g.ifVbSp == g0.ifVbSp);
    CHECK(g.hscale == g0.hscale);

    Rig edtv(tv5725::ProgramId::Ntsc960p, Standard::Edtv60);
    CHECK(edtv.at(adj).disVbSp == 38 - 6); // no doubler: one IF line per step
}

TEST_CASE("past the frame's edges the picture is cut, never wrapped") {
    Rig r(tv5725::ProgramId::Ntsc960p, Standard::Ntsc);
    GeometryAdjust down;
    down.vPosition = 3;
    GeometryRegs g = r.at(down);
    CHECK(g.disVbSp == 38 + 12); // the program's window already ends on the last line
    CHECK(g.disVbSt == 1000);
    CHECK(g.ifVbSp == 8);
    GeometryAdjust up;
    up.vPosition = -20;
    g = r.at(up);
    CHECK(g.disVbSp == 4 + 1 + 14); // just after VSync plus the fetch lead
    CHECK(g.disVbSt == 1000 - 80);
    CHECK(g.ifVbSp == 8 + 30); // the rest of the move: the capture starts later
    CHECK(g.ifVbSt == 8 + 28);
}

TEST_CASE(
    "size scales the picture; a picture larger than the window is cropped around its centre") {
    Rig r(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    GeometryAdjust adj;
    adj.hSize = 20;
    adj.vSize = 20;
    const GeometryRegs g = r.at(adj);
    CHECK(g.hscale == 932); // 1.1x, as measured on the bench
    CHECK(g.disHbSt - g.disHbSp == 1108);
    CHECK(g.ifHbSp2 == 104); // the window holds the whole capture
    CHECK(g.vscale == 414);
    CHECK(g.disVbSt - g.disVbSp == 1085); // the program's whole window
    CHECK(g.ifVbSp == 10 + 20);           // 439 of the 482 captured lines shown
}

TEST_CASE("the user's size stops at 2x, below which 1280x960 displaced lines") {
    Rig r(tv5725::ProgramId::Ntsc960p, Standard::Ntsc);
    GeometryAdjust adj;
    adj.hSize = 64;
    adj.vSize = 20;
    const GeometryRegs g = r.at(adj);
    CHECK(g.hscale == 512);
    CHECK(g.disHbSt - g.disHbSp == 1982);
    CHECK(g.ifHbSp2 == 116);
    CHECK(g.vscale == 465); // vertically 1.1x is fine
    CHECK(g.disVbSt - g.disVbSp == 962);
    CHECK(g.ifVbSp == 8 + 22); // 437 of the 481 captured lines shown
    // A shape that needs more than 2x keeps it: 480p's 4:3.
    Rig r480(tv5725::ProgramId::Ntsc480p, Standard::Ntsc);
    CHECK(r480.at({}).hscale == 478);
    adj.vSize = 0;
    CHECK(r480.at(adj).hscale == 478);
}

TEST_CASE("narrower than 1:1 is out of the scaler's reach") {
    Rig r(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    GeometryAdjust adj;
    adj.hSize = -20;
    const GeometryRegs g = r.at(adj);
    CHECK(g.hscale == 1024);
    CHECK(g.disHbSt - g.disHbSp == 1008);
}

TEST_CASE("the window stops short of the line end and of the sync") {
    Rig r(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    GeometryAdjust adj = aspect(Aspect::SixteenNine);
    adj.hPosition = 64;
    GeometryRegs g = r.at(adj);
    CHECK(g.disHbSt <= 1601 - 2);
    adj.hPosition = -64;
    g = r.at(adj);
    CHECK(g.disHbSp >= 72 + 4); // HSync ends at 72 in this program
    CHECK(g.hbSp >= 0);
}

TEST_CASE("a 50 Hz source on a 60 Hz program shows its full 576 lines, at 4:3") {
    Rig r(tv5725::ProgramId::Ntsc1080p, Standard::Pal, 625); // output.force60
    const GeometryRegs g = r.at({});
    CHECK(g.vscale == 546);               // 576 lines in 1080
    CHECK(g.disVbSt - g.disVbSp == 1080); // 576 lines at that scale, inside the 1085 window
    CHECK(g.hscale == 1019);              // the 60 Hz program samples less: true 4:3 here
}

TEST_CASE("the 15 kHz programs take position only") {
    Rig r(tv5725::ProgramId::NtscDownscale, Standard::Ntsc);
    GeometryAdjust adj = aspect(Aspect::SixteenNine);
    adj.hSize = 30;
    adj.hPosition = -4;
    const GeometryRegs g = r.at(adj);
    const GeometryRegs g0 = r.at({});
    CHECK(g.hscale == g0.hscale);
    CHECK(g.vscale == g0.vscale);
    CHECK(g.disHbSp == g0.disHbSp - 8);
    CHECK(g.ifHbSp2 == g0.ifHbSp2);
}

TEST_CASE("PAL 4:3 uses the 625-line width: 960p is a little narrower than the program") {
    Rig r(tv5725::ProgramId::Pal960p, Standard::Pal, 625);
    const GeometryRegs g = r.at({});
    CHECK(g.hscale == 615); // the program carries 607
    CHECK(g.vscale == 614); // as fitted for 576 lines (docs/bench/M1.md)
}

TEST_CASE("the frame lock writes the line and the recomputed windows together") {
    Rig r(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    r.ctx.inputIsYpbpr = false;
    REQUIRE(video::applyBestHTotal(r.ops, r.sense, r.ctx, 1593, r.base, {}));
    CHECK(r.chip.get(VDS_HSYNC_RST) == 1593);
    CHECK(r.chip.get(VDS_DIS_HB_SP) == 344);
    CHECK(r.chip.get(VDS_DIS_HB_ST) == 1352);
    CHECK(r.chip.get(VDS_EXT_HB_SP) == 344);
    CHECK(r.chip.get(VDS_HB_SP) == 260);
    CHECK_FALSE(video::applyBestHTotal(r.ops, r.sense, r.ctx, 1400, r.base, {})); // over 6 %
}

TEST_CASE("without a baseline the frame lock still shortens the line, and only the line") {
    Rig r(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    REQUIRE(video::applyBestHTotal(r.ops, r.sense, r.ctx, 1593, video::GeometryBase{}, {}));
    CHECK(r.chip.get(VDS_HSYNC_RST) == 1593);
    CHECK(r.chip.get(VDS_DIS_HB_SP) == 348);
    CHECK(r.chip.get(VDS_HSCALE) == 1023);
}

TEST_CASE("a bus error while reading the baseline voids it") {
    Rig r(tv5725::ProgramId::Ntsc960p, Standard::Ntsc);
    r.bus.failNext = true;
    CHECK_FALSE(video::readGeometryBase(r.chip, r.ctx).valid);
    CHECK(video::readGeometryBase(r.chip, r.ctx).valid);
}

TEST_CASE("an implausible line length falls back to the baseline's") {
    Rig r(tv5725::ProgramId::Ntsc720p, Standard::Ntsc);
    const GeometryAdjust adj = aspect(Aspect::SixteenNine);
    CHECK(video::computeGeometry(r.base, adj, 0) == r.at(adj));
}

TEST_CASE("a frame count far from the standard's own is replaced by the nominal one") {
    Rig r(tv5725::ProgramId::Ntsc960p, Standard::Ntsc, 0);
    CHECK(r.base.ifFrame == 524);
    Rig sega(tv5725::ProgramId::Ntsc960p, Standard::Ntsc, 523); // within reach: kept
    CHECK(sega.base.ifFrame == 522);
}

TEST_CASE("writing applies every owned field, with the bypass bits for 1:1") {
    Rig r(tv5725::ProgramId::Ntsc720p, Standard::Ntsc);
    video::writeGeometry(r.chip, r.at(aspect(Aspect::SixteenNine)));
    CHECK(r.chip.get(VDS_HSCALE_BYPS) == 0);
    video::writeGeometry(r.chip, video::computeGeometry(r.base, {}, r.base.htotal));
    CHECK(r.chip.get(VDS_HSCALE) == 705);
    CHECK(r.chip.get(VDS_EXT_VB_ST) == r.chip.get(VDS_DIS_VB_ST));
    Rig r1080(tv5725::ProgramId::Ntsc1080p, Standard::Ntsc);
    video::writeGeometry(r1080.chip, r1080.at({}));
    CHECK(r1080.chip.get(VDS_HSCALE) == 1023);
    CHECK(r1080.chip.get(VDS_HSCALE_BYPS) == 1);
}
