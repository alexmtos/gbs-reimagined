#include <string>

#include "../support/doctest_main.h"
#include "../support/fake_storage.h"
#include "core/record.h"
#include "core/settings_store.h"

using namespace gbsr;
using settings::Id;
using Result = settings::Store::SetResult;

namespace {

std::string text(Id id, int16_t v) {
    char buf[32];
    settings::formatValue(id, v, buf, sizeof buf);
    return buf;
}

void putEntry(std::vector<uint8_t>& out, uint32_t hash, int16_t value) {
    for (int b = 0; b < 4; ++b) {
        out.push_back(static_cast<uint8_t>(hash >> (8 * b)));
    }
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(static_cast<uint16_t>(value) >> 8));
}

} // namespace

TEST_CASE("a fresh store holds the schema defaults and finds no file") {
    test::FakeFileStore fs;
    settings::Store store(fs);
    CHECK_FALSE(store.load());
    CHECK(store.get(Id::OutputResolution) == 0); // auto
    CHECK(store.on(Id::OutputFrameLock));
    CHECK(store.get(Id::InputSyncThreshold) == -1); // auto
    CHECK(store.get(Id::PictureSharpness) == 0);    // off
    CHECK_FALSE(store.savePending());
}

TEST_CASE("a change is saved three seconds after the last one and survives a reload") {
    test::FakeFileStore fs;
    settings::Store store(fs);
    CHECK(store.set(Id::PictureBrightness, 5, 1000) == Result::Changed);
    CHECK(store.set(Id::PictureBrightness, 5, 1000) == Result::Unchanged);
    CHECK(store.set(Id::PictureBrightness, 99, 1000) == Result::OutOfRange);
    CHECK(store.set(Id::OutputResolution, 1, 2000) == Result::Changed);
    store.tick(4500); // 2.5 s after the last change
    CHECK(fs.writes == 0);
    store.tick(5000);
    CHECK(fs.writes == 1);
    CHECK_FALSE(store.savePending());

    settings::Store again(fs);
    REQUIRE(again.load());
    CHECK(again.get(Id::PictureBrightness) == 5);
    CHECK(again.get(Id::OutputResolution) == 1);
    CHECK(again.on(Id::OutputFrameLock));
}

TEST_CASE("unknown ids are ignored, out-of-range values keep the default, missing ids too") {
    test::FakeFileStore fs;
    std::vector<uint8_t> payload;
    putEntry(payload, 0xDEADBEEF, 7);                                // a setting this build lacks
    putEntry(payload, settings::def(Id::PictureContrast).hash, 300); // out of range
    putEntry(payload, settings::def(Id::PictureSaturation).hash, -4);
    REQUIRE(core::saveRecord(fs, "/settings.bin", 0x53524247, 1, payload.data(),
                             static_cast<uint16_t>(payload.size())));
    settings::Store store(fs);
    REQUIRE(store.load());
    CHECK(store.get(Id::PictureContrast) == 0);
    CHECK(store.get(Id::PictureSaturation) == -4);
    CHECK(store.get(Id::OutputResolution) == 0);
}

TEST_CASE("a damaged file reads as absent") {
    test::FakeFileStore fs;
    settings::Store store(fs);
    store.set(Id::PictureBrightness, 3, 0);
    REQUIRE(store.saveNow());
    fs.files["/settings.bin"][14] ^= 0x01; // one bit in the payload
    settings::Store again(fs);
    CHECK_FALSE(again.load());
    CHECK(again.get(Id::PictureBrightness) == 0);
}

TEST_CASE("a failed save is retried after another delay, not on every pass") {
    test::FakeFileStore fs;
    fs.failWrites = true;
    settings::Store store(fs);
    store.set(Id::PictureBrightness, 1, 0);
    store.tick(3000);
    store.tick(3020);
    store.tick(3040);
    CHECK(fs.writes == 1);
    fs.failWrites = false;
    store.tick(6000);
    CHECK(fs.writes == 2);
    CHECK_FALSE(store.savePending());
}

TEST_CASE("values convert to and from text the way the console and the API spell them") {
    CHECK(text(Id::OutputResolution, 1) == "r1080p");
    CHECK(text(Id::OutputFrameLock, 1) == "on");
    CHECK(text(Id::InputPhase, -1) == "auto");
    CHECK(text(Id::InputPhase, 12) == "12");
    CHECK(text(Id::PictureBrightness, -3) == "-3");

    int16_t v = 0;
    CHECK(settings::parseValue(Id::OutputResolution, "R1080P", v));
    CHECK(v == 1);
    CHECK(settings::parseValue(Id::PictureSharpness, "medium", v));
    CHECK(v == 2);
    CHECK(settings::parseValue(Id::PictureSharpness, "3", v)); // a bare index
    CHECK(v == 3);
    CHECK(settings::parseValue(Id::OutputFrameLock, "off", v));
    CHECK(v == 0);
    CHECK(settings::parseValue(Id::InputPhase, "auto", v));
    CHECK(v == -1);
    CHECK_FALSE(settings::parseValue(Id::PictureBrightness, "40", v)); // above +32
    CHECK_FALSE(settings::parseValue(Id::PictureBrightness, "5x", v));
    CHECK_FALSE(settings::parseValue(Id::PictureSharpness, "extreme", v));
    CHECK_FALSE(settings::parseValue(Id::PictureBrightness, "auto", v)); // has no special

    Id id{};
    CHECK(settings::findId("Picture.Sharpness", id));
    CHECK(id == Id::PictureSharpness);
    CHECK_FALSE(settings::findId("picture.hue", id));
}

TEST_CASE("settings of this milestone are available unless pending") {
    CHECK(settings::available(Id::OutputResolution));
    CHECK(settings::available(Id::InputAutoGain));
    CHECK(settings::available(Id::OutputAspect));            // M2
    CHECK_FALSE(settings::available(Id::PictureScanlines));  // M2, pending
    CHECK(settings::def(Id::PictureScanlines).milestone() == 2);
}
