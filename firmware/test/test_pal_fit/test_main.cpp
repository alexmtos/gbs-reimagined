#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/fields.gen.h"
#include "chip/tv5725.h"
#include "video/tuning.h"

using namespace gbsr;
using namespace gbsr::tv5725::fields;

namespace {

struct Rig {
    test::FakeI2cBus bus;
    test::FakeTv5725 dev;
    tv5725::Chip chip{bus};
    video::LockState lock;
    video::Ops ops{chip, lock};
    video::PathContext ctx;

    Rig(video::Standard s, uint8_t presetId) {
        bus.attach(tv5725::Chip::kAddress, dev);
        ctx.standard = s;
        ctx.presetId = presetId;
        chip.write(VDS_VSCALE, 512); // as the programs leave them
        chip.write(IF_VB_ST, 38);
        chip.write(IF_VB_SP, 40);
    }
};

} // namespace

TEST_CASE("PAL at 1080p shows all 576 lines from the first active one") {
    Rig r(video::Standard::Pal, 0x15);
    video::fitPalVertical(r.ops, r.ctx);
    CHECK(r.chip.get(VDS_VSCALE) == 546);
    CHECK(r.chip.get(IF_VB_ST) == 16);
    CHECK(r.chip.get(IF_VB_SP) == 18);
}

TEST_CASE("each measured PAL program gets its own scale; 720p keeps its scale") {
    Rig a(video::Standard::Pal, 0x12);
    video::fitPalVertical(a.ops, a.ctx);
    CHECK(a.chip.get(VDS_VSCALE) == 576);
    Rig b(video::Standard::Pal, 0x11);
    video::fitPalVertical(b.ops, b.ctx);
    CHECK(b.chip.get(VDS_VSCALE) == 614);
    Rig c(video::Standard::Pal, 0x13);
    video::fitPalVertical(c.ops, c.ctx);
    CHECK(c.chip.get(VDS_VSCALE) == 512); // untouched
    CHECK(c.chip.get(IF_VB_ST) == 16);
}

TEST_CASE("NTSC, unmeasured PAL programs and the bypass are left alone") {
    Rig ntsc(video::Standard::Ntsc, 0x05);
    video::fitPalVertical(ntsc.ops, ntsc.ctx);
    CHECK(ntsc.chip.get(VDS_VSCALE) == 512);
    CHECK(ntsc.chip.get(IF_VB_ST) == 38);
    Rig pal576(video::Standard::Pal, 0x14);
    video::fitPalVertical(pal576.ops, pal576.ctx);
    CHECK(pal576.chip.get(IF_VB_ST) == 38);
    Rig bypass(video::Standard::Pal, 0x15);
    bypass.ctx.hdBypass = true;
    video::fitPalVertical(bypass.ops, bypass.ctx);
    CHECK(bypass.chip.get(VDS_VSCALE) == 512);
}
