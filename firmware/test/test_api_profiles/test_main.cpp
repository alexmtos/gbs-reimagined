#include <string>

#include <ArduinoJson.h>

#include "../support/doctest_main.h"
#include "../support/fake_storage.h"
#include "../support/fake_video.h"
#include "core/api.h"

using namespace gbsr;
using core::InputSignature;
using settings::Id;

namespace {

struct StringSink : core::Sink {
    std::string text;
    int code = 0;
    void status(int c) override { code = c; }
    void write(const char* data, size_t len) override { text.append(data, len); }
};

InputSignature snes() {
    InputSignature s;
    s.valid = true;
    s.port = static_cast<uint8_t>(video::InputPort::Rgb);
    s.sync = static_cast<uint8_t>(video::SyncType::Csync);
    s.standard = static_cast<uint8_t>(video::Standard::Ntsc);
    s.lines = 262;
    return s;
}

struct Rig {
    test::FakeFileStore fs;
    test::FakeVideo video;
    core::Controller ctl{fs, video};
    Rig() { ctl.begin(false, 0); }

    // POST /api/profile; checks the HTTP status and returns the body.
    std::string profile(const std::string& body, int expect = 200) {
        StringSink out;
        CHECK(core::postProfile(ctl, body.data(), body.size(), 1000, out) == expect);
        CHECK(out.code == expect);
        return out.text;
    }
    std::string list() {
        StringSink out;
        CHECK(core::getProfiles(ctl, out) == 200);
        return out.text;
    }
    std::string exported(uint8_t slot, int expect = 200) {
        StringSink out;
        CHECK(core::getProfile(ctl, slot, out) == expect);
        return out.text;
    }
};

bool has(const std::string& s, const char* part) {
    return s.find(part) != std::string::npos;
}

bool validJson(const std::string& s) {
    JsonDocument doc;
    return deserializeJson(doc, s) == DeserializationError::Ok;
}

} // namespace

TEST_CASE("GET /api/profiles lists the slots in use, the active one and the current source") {
    Rig r;
    CHECK(r.list() == R"({"active":0,"modified":false,"source":null,"profiles":[]})");
    r.ctl.onSource(snes(), 0);
    CHECK(has(r.profile(R"({"action":"save","slot":3,"name":"Super \"F\"","source":"this"})"),
              R"("ok":true)"));
    const std::string s = r.list();
    CHECK(validJson(s));
    CHECK(has(s, R"("active":3,"modified":false)"));
    CHECK(has(s, R"("source":{"port":"rgb","sync":"csync","standard":"ntsc","lines":262})"));
    CHECK(has(s, R"({"slot":3,"name":"Super \"F\"","source":{"port":"rgb")"));
}

TEST_CASE("POST /api/profile loads, saves, renames, re-signs and deletes") {
    Rig r;
    r.ctl.set(Id::PictureBrightness, 5, core::Origin::Oled, 0);
    r.profile(R"({"action":"save","slot":1})");
    r.ctl.set(Id::PictureBrightness, -5, core::Origin::Oled, 0);
    CHECK(has(r.list(), R"("active":1,"modified":true)"));
    CHECK(r.profile(R"({"action":"load","slot":1})") == R"({"ok":true,"result":"loaded"})");
    CHECK(r.video.picture.brightness == 5);

    r.ctl.set(Id::OutputResolution, 1, core::Origin::Oled, 0);
    r.profile(R"({"action":"save","slot":2,"name":"HD"})");
    r.ctl.set(Id::OutputResolution, 5, core::Origin::Oled, 0);
    CHECK(r.profile(R"({"action":"load","slot":2})") ==
          R"({"ok":true,"result":"revertArmed","revert":{"secondsLeft":20}})");
    CHECK(has(r.profile(R"({"action":"save","slot":4})", 409), "revertPending"));
    r.ctl.keep();

    r.profile(R"({"action":"rename","slot":2,"name":"Mega Drive"})");
    CHECK(has(r.list(), R"("name":"Mega Drive")"));
    CHECK(has(r.profile(R"({"action":"source","slot":2,"on":true})", 409), "noSource"));
    r.ctl.onSource(snes(), 0);
    r.profile(R"({"action":"source","slot":2,"on":true})");
    CHECK(r.ctl.profiles().signature(2) == snes());
    r.profile(R"({"action":"delete","slot":2})");
    CHECK_FALSE(r.ctl.profiles().exists(2));
}

TEST_CASE("POST /api/profile refuses with a reason") {
    Rig r;
    CHECK(has(r.profile(R"({"action":"save","slot":17})", 400), "invalidSlot"));
    CHECK(has(r.profile(R"({"action":"save","slot":1,"name":"€"})", 400), "invalidName"));
    CHECK(has(r.profile(R"({"action":"save","slot":1,"source":"this"})", 409), "noSource"));
    CHECK(has(r.profile(R"({"action":"load","slot":9})", 404), "noProfile"));
    CHECK(has(r.profile(R"({"action":"rename","slot":9,"name":"x"})", 404), "noProfile"));
    CHECK(has(r.profile(R"({"action":"delete","slot":9})", 404), "noProfile"));
    CHECK(has(r.profile(R"({"action":"fly","slot":1})", 404), "unknownAction"));
    CHECK(has(r.profile(R"({"slot":1})", 400), "expectedAction"));
    CHECK(has(r.profile("[", 400), "expectedAction"));
}

TEST_CASE("an exported profile imports into another slot unchanged") {
    Rig r;
    r.ctl.onSource(snes(), 0);
    r.ctl.set(Id::PictureScanlines, 1, core::Origin::Oled, 0);
    r.ctl.set(Id::OutputAspect, 1, core::Origin::Oled, 0);
    r.profile(R"({"action":"save","slot":5,"name":"São","source":"this"})");
    const std::string doc = r.exported(5);
    CHECK(validJson(doc));
    CHECK(has(doc, R"({"format":"gbsr-profile","version":1,"schema":")"));
    CHECK(has(doc, "\"name\":\"S\xC3\xA3o\""));
    CHECK(has(doc, R"("picture.scanlines":"on")"));
    CHECK_FALSE(has(doc, "system."));
    CHECK(has(r.exported(6, 404), "noProfile"));

    const std::string body = R"({"action":"import","slot":6,"profile":)" + doc + "}";
    CHECK(r.profile(body) == R"({"ok":true,"skipped":[]})");
    core::Profile a;
    core::Profile b;
    REQUIRE(r.ctl.profiles().read(5, a));
    REQUIRE(r.ctl.profiles().read(6, b));
    CHECK(std::string(a.name) == b.name);
    CHECK(a.signature == b.signature);
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        CHECK(a.has[i] == b.has[i]);
        CHECK(a.values[i] == b.values[i]);
    }
}

TEST_CASE(
    "an import skips what this build does not know, and replacing the active slot unmarks it") {
    Rig r;
    r.profile(R"({"action":"save","slot":1})");
    REQUIRE(r.ctl.activeProfile() == 1);
    const std::string s =
        r.profile(R"({"action":"import","slot":1,"profile":{"name":"Old","source":null,"values":)"
                  R"({"picture.brightness":-3,"picture.hue":4,"system.language":"ptBR",)"
                  R"("picture.contrast":"lots"}}})");
    CHECK(s == R"({"ok":true,"skipped":["picture.hue","system.language","picture.contrast"]})");
    CHECK(r.ctl.activeProfile() == 0);
    core::Profile p;
    REQUIRE(r.ctl.profiles().read(1, p));
    CHECK(p.has[static_cast<uint8_t>(Id::PictureBrightness)]);
    CHECK(p.values[static_cast<uint8_t>(Id::PictureBrightness)] == -3);
    CHECK_FALSE(p.has[static_cast<uint8_t>(Id::PictureContrast)]);

    CHECK(has(r.profile(R"({"action":"import","slot":2,"profile":{"name":"x","source":)"
                        R"({"port":"vga"},"values":{}}})",
                        400),
              "invalidSource"));
    CHECK(has(r.profile(R"({"action":"import","slot":2,"profile":{"name":"x"}})", 400),
              "expectedProfile"));
}

TEST_CASE("POST /api/svs takes a switcher line") {
    Rig r;
    r.ctl.set(Id::PictureBrightness, 7, core::Origin::Oled, 0);
    r.profile(R"({"action":"save","slot":2})");
    r.ctl.set(Id::PictureBrightness, 0, core::Origin::Oled, 0);
    StringSink out;
    const std::string line = "SVS NEW INPUT=2\r\n";
    CHECK(core::postSwitcher(r.ctl, line.data(), line.size(), 0, out) == 200);
    CHECK(out.text == R"({"ok":true,"result":"loaded"})");
    CHECK(r.video.picture.brightness == 7);

    StringSink same;
    const std::string current = "SVS CURRENT INPUT=2";
    CHECK(core::postSwitcher(r.ctl, current.data(), current.size(), 0, same) == 200);
    CHECK(has(same.text, "unchanged"));

    StringSink missing;
    const std::string other = "remote prof9";
    CHECK(core::postSwitcher(r.ctl, other.data(), other.size(), 0, missing) == 404);

    StringSink bad;
    const std::string junk = "hello";
    CHECK(core::postSwitcher(r.ctl, junk.data(), junk.size(), 0, bad) == 400);
}

TEST_CASE("a backup holds the settings, the calibration and every profile, and is valid JSON") {
    Rig r;
    r.ctl.set(Id::SystemLanguage, 1, core::Origin::Oled, 0);
    const uint8_t rgb[3] = {0x70, 0x71, 0x72};
    r.video.setBaseGains(1, rgb);
    r.profile(R"({"action":"save","slot":1,"name":"One"})");
    r.profile(R"({"action":"save","slot":16,"name":"Sixteen"})");
    StringSink out;
    CHECK(core::getBackup(r.ctl, "0.1.0", out) == 200);
    const std::string& s = out.text;
    CHECK(validJson(s));
    CHECK(has(s, R"({"format":"gbsr-backup","version":1,"firmware":"0.1.0","schema":")"));
    CHECK(has(s, R"("system.language":"ptBR")"));
    CHECK(has(s, R"("levels":{"component":[123,123,123],"rgb":[112,113,114]})"));
    CHECK(has(s, R"("profiles":{"1":{"name":"One","source":null,"values":{)"));
    CHECK(has(s, R"(,"16":{"name":"Sixteen")"));
    CHECK_FALSE(has(s, "password"));
    CHECK_FALSE(has(s, "ssid"));
}

TEST_CASE("POST /api/levels puts a backup's calibration back and saves it") {
    Rig r;
    StringSink out;
    const std::string body = R"({"component":[100,101,102],"rgb":[110,111,112]})";
    CHECK(core::postLevels(r.ctl, body.data(), body.size(), out) == 200);
    CHECK(r.video.base[0][2] == 102);
    CHECK(r.video.base[1][0] == 110);
    CHECK(r.fs.files.count("/levels.bin") == 1);

    StringSink bad;
    const std::string wrong = R"({"component":[100,101],"rgb":[110,111,300]})";
    CHECK(core::postLevels(r.ctl, wrong.data(), wrong.size(), bad) == 400);
    CHECK(r.video.base[0][0] == 100);
}

TEST_CASE("names that need escaping stay valid JSON in the list, an export and a backup") {
    Rig r;
    r.profile(R"({"action":"save","slot":1,"name":"Say \"hi\" \\ there"})");
    CHECK(validJson(r.list()));
    CHECK(has(r.list(), R"("name":"Say \"hi\" \\ there")"));
    CHECK(validJson(r.exported(1)));
    StringSink out;
    core::getBackup(r.ctl, "0.1.0", out);
    CHECK(validJson(out.text));
}
