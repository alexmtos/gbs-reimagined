#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/fields.gen.h"
#include "chip/programs.h"
#include "chip/tv5725.h"

using namespace gbsr;
using namespace gbsr::tv5725;

namespace {

struct Rig {
    test::FakeI2cBus bus;
    test::FakeTv5725 dev;
    Chip chip{bus};
    Rig() { bus.attach(Chip::kAddress, dev); }
};

} // namespace

TEST_CASE("a 12-bit field spanning two bytes reads and writes little-endian") {
    Rig r;
    // VDS_HSYNC_RST: S3_01 bits 0..7 and S3_02 bits 0..3; S3_02 bits 4..7 belong to VSYNC_RST.
    r.dev.at(3, 0x02) = 0xA0;
    REQUIRE(r.chip.write(fields::VDS_HSYNC_RST, 0x63F));
    CHECK(r.dev.at(3, 0x01) == 0x3F);
    CHECK(r.dev.at(3, 0x02) == 0xA6); // upper nibble preserved
    CHECK(r.chip.get(fields::VDS_HSYNC_RST) == 0x63F);
    CHECK(r.chip.ok());
}

TEST_CASE("a single-bit write preserves the neighbouring bits") {
    Rig r;
    r.dev.at(0, 0x44) = 0xFE;
    REQUIRE(r.chip.write(fields::DAC_RGBS_PWDNZ, 1));
    CHECK(r.dev.at(0, 0x44) == 0xFF);
    REQUIRE(r.chip.write(fields::DAC_RGBS_PWDNZ, 0));
    CHECK(r.dev.at(0, 0x44) == 0xFE);
}

TEST_CASE("the vendor's one signed field is sign-extended") {
    Rig r;
    r.dev.at(3, 0x3F) = 0x80;
    uint32_t v = 0;
    REQUIRE(r.chip.read(fields::VDS_UV_BLK_VAL, v));
    CHECK(static_cast<int32_t>(v) == -128);
}

TEST_CASE("the segment is selected once for consecutive accesses in the same segment") {
    Rig r;
    r.chip.get(fields::VDS_HSYNC_RST);
    r.chip.get(fields::VDS_VSYNC_RST);
    CHECK(r.dev.segmentSelects == 1);
    r.chip.get(fields::ADC_INPUT_SEL);
    CHECK(r.dev.segmentSelects == 2);
}

TEST_CASE("reads longer than the chip's 16-byte burst are split") {
    Rig r;
    for (int i = 0; i < 40; ++i) {
        r.dev.at(4, static_cast<uint8_t>(i)) = static_cast<uint8_t>(i * 3);
    }
    uint8_t buf[40] = {};
    REQUIRE(r.chip.readBytes(4, 0, buf, sizeof buf));
    for (int i = 0; i < 40; ++i) {
        CHECK(buf[i] == static_cast<uint8_t>(i * 3));
    }
}

TEST_CASE("a bus error is sticky and forgets the cached segment") {
    Rig r;
    r.chip.get(fields::VDS_HSYNC_RST);
    r.bus.failNext = true;
    uint32_t v = 0;
    CHECK_FALSE(r.chip.read(fields::VDS_HSYNC_RST, v));
    CHECK_FALSE(r.chip.ok());
    const int selectsBefore = r.dev.segmentSelects;
    r.chip.clearError();
    r.chip.get(fields::VDS_HSYNC_RST);
    CHECK(r.dev.segmentSelects == selectsBefore + 1); // re-selected after the error
    CHECK(r.chip.ok());
}

TEST_CASE("probe uses a scratch register and restores it") {
    Rig r;
    r.dev.at(5, 0x69) = 0x11;
    CHECK(r.chip.probe());
    CHECK(r.dev.at(5, 0x69) == 0x11);
}

TEST_CASE("every baseline program writes each of its values exactly where annotated") {
    for (uint8_t i = 0; i < static_cast<uint8_t>(ProgramId::Count); ++i) {
        Rig r;
        const auto id = static_cast<ProgramId>(i);
        const ProgramInfo info = programInfo(id);
        CAPTURE(i);
        REQUIRE(r.chip.writeProgram(id));
        CHECK(r.dev.writes.size() == info.valueCount);
        // Spot-check the decoding against the stored records.
        const uint8_t seg = pgm_read_byte(info.records);
        const uint8_t start = pgm_read_byte(info.records + 1);
        const uint8_t first = pgm_read_byte(info.records + 3);
        CHECK(r.dev.at(seg, start) == first);
    }
}

TEST_CASE("the 1080p NTSC program carries the values its source annotates") {
    Rig r;
    REQUIRE(r.chip.writeProgram(ProgramId::Ntsc1080p));
    CHECK(r.dev.at(0, 0x40) == 0x7C);
    CHECK(r.dev.at(0, 0x41) == 0x85);
    CHECK(r.dev.at(3, 0x10) == 0x4C);
}
