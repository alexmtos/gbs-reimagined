#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/tv5725.h"
#include "video/picture.h"

using namespace gbsr;

namespace {

struct Rig {
    test::FakeI2cBus bus;
    test::FakeTv5725 dev;
    tv5725::Chip chip{bus};
    video::LockState lock;
    video::Ops ops{chip, lock};
    Rig() { bus.attach(tv5725::Chip::kAddress, dev); }
};

} // namespace

TEST_CASE("neutral RGB: unity luma, matrix chroma gains, no offsets, peaking off") {
    Rig r;
    r.dev.at(3, 0x4E) = 0x1A; // a program's peaking bits around the bypass
    video::applyPicture(r.ops, false, video::PictureAdjust{});
    CHECK(r.dev.at(3, 0x35) == 0x80); // VDS_Y_GAIN
    CHECK(r.dev.at(3, 0x36) == 0x1C); // VDS_UCOS_GAIN
    CHECK(r.dev.at(3, 0x37) == 0x29); // VDS_VCOS_GAIN
    CHECK(r.dev.at(3, 0x3A) == 0x00); // VDS_Y_OFST
    CHECK(r.dev.at(3, 0x45) == 0x00); // VDS_PK_LB_GAIN
    CHECK(r.dev.at(3, 0x4E) == 0x1B); // only VDS_PK_Y_H_BYPS changed
}

TEST_CASE("component input keeps its own offsets under the adjustments") {
    Rig r;
    video::PictureAdjust adj;
    adj.brightness = -3;
    video::applyPicture(r.ops, true, adj);
    CHECK(r.dev.at(3, 0x3A) == 0xFB); // -2 - 3, two's complement
    CHECK(r.dev.at(3, 0x3B) == 0x03); // VDS_U_OFST
    CHECK(r.dev.at(3, 0x3C) == 0x03); // VDS_V_OFST
}

TEST_CASE("contrast, saturation and sharpness move only their own fields") {
    Rig r;
    video::PictureAdjust adj;
    adj.contrast = 16;
    adj.saturation = -32;
    adj.sharpness = 2;
    video::applyPicture(r.ops, false, adj);
    CHECK(r.dev.at(3, 0x35) == 0xA0);
    CHECK(r.dev.at(3, 0x36) == 0x00); // grey
    CHECK(r.dev.at(3, 0x37) == 0x00);
    CHECK(r.dev.at(3, 0x45) == 0x14); // medium: the community firmware's peaking
    CHECK(r.dev.at(3, 0x47) == 0x14);
    CHECK((r.dev.at(3, 0x4E) & 0x01) == 0);
}
