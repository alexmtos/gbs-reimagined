#include "../support/doctest_main.h"
#include "../support/fake_storage.h"
#include "core/controller.h"

using namespace gbsr;
using core::Controller;
using core::Origin;
using settings::Id;

namespace {

// Records what the controller asks of the video path.
class FakeVideo : public video::VideoControl {
public:
    video::OutputMode output = video::OutputMode::Auto;
    video::InputPreference input = video::InputPreference::Auto;
    bool frameLock = false;
    bool autoGain = false;
    video::PictureAdjust picture;
    uint8_t base[2][3] = {{0x7B, 0x7B, 0x7B}, {0x7B, 0x7B, 0x7B}};
    uint16_t levelsSeq = 0;
    int outputCalls = 0;

    void setOutputPreference(video::OutputMode m) override {
        output = m;
        ++outputCalls;
    }
    void setInputPreference(video::InputPreference p) override { input = p; }
    void setFrameLock(bool on) override { frameLock = on; }
    void setAutoGain(bool on) override { autoGain = on; }
    void setPicture(const video::PictureAdjust& a) override { picture = a; }
    void setBaseGains(uint8_t port, const uint8_t (&g)[3]) override {
        for (int c = 0; c < 3; ++c) {
            base[port][c] = g[c];
        }
    }
    void baseGains(uint8_t port, uint8_t (&g)[3]) const override {
        for (int c = 0; c < 3; ++c) {
            g[c] = base[port][c];
        }
    }
    uint16_t levelsSequence() const override { return levelsSeq; }
    bool calibrateLevels() override { return true; }
};

struct Rig {
    test::FakeFileStore fs;
    FakeVideo video;
    Controller ctl{fs, video};
};

} // namespace

TEST_CASE("begin applies the saved settings to the video path") {
    Rig first;
    first.ctl.begin(false, 0);
    first.ctl.set(Id::OutputResolution, 1, Origin::Oled, 0); // r1080p
    first.ctl.set(Id::PictureSharpness, 2, Origin::Oled, 0);
    first.ctl.set(Id::InputAutoGain, 1, Origin::Oled, 0);
    first.ctl.tick(5000);

    Rig second;
    second.fs = first.fs;
    second.ctl.begin(false, 0);
    CHECK(second.video.output == video::OutputMode::R1080p);
    CHECK(second.video.picture.sharpness == 2);
    CHECK(second.video.autoGain);
    CHECK(second.video.frameLock); // default on
}

TEST_CASE("a risky change from the web reverts unless kept, and is not saved meanwhile") {
    Rig r;
    r.ctl.begin(false, 0);
    CHECK(r.ctl.set(Id::OutputResolution, 1, Origin::Web, 1000) ==
          Controller::Outcome::RevertArmed);
    CHECK(r.video.output == video::OutputMode::R1080p);
    CHECK(r.ctl.revertPending());
    CHECK(r.ctl.revertRemainingMs(1000) == 20'000);
    r.ctl.tick(15'000);
    CHECK(r.fs.writes == 0); // waiting for the answer
    r.ctl.tick(21'000);
    CHECK_FALSE(r.ctl.revertPending());
    CHECK(r.video.output == video::OutputMode::Auto);
    CHECK(r.ctl.get(Id::OutputResolution) == 0);
}

TEST_CASE("keep confirms a risky change, which is then saved") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.set(Id::OutputFrameLock, 0, Origin::Console, 0);
    r.ctl.keep();
    r.ctl.tick(30'000);
    CHECK_FALSE(r.video.frameLock);
    CHECK(r.fs.writes == 1);
}

TEST_CASE("a second risky change keeps the first snapshot and restarts the countdown") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.set(Id::OutputResolution, 1, Origin::Web, 0);
    r.ctl.set(Id::OutputResolution, 2, Origin::Web, 15'000);
    r.ctl.tick(25'000); // 20 s after the first, 10 s after the second
    CHECK(r.ctl.revertPending());
    r.ctl.revert();
    CHECK(r.video.output == video::OutputMode::Auto); // back to before the first change
}

TEST_CASE("changes on the OLED and non-risky changes confirm themselves") {
    Rig r;
    r.ctl.begin(false, 0);
    CHECK(r.ctl.set(Id::OutputResolution, 3, Origin::Oled, 0) == Controller::Outcome::Applied);
    CHECK(r.ctl.set(Id::PictureBrightness, 4, Origin::Web, 0) == Controller::Outcome::Applied);
    CHECK_FALSE(r.ctl.revertPending());
    CHECK(r.video.picture.brightness == 4);
}

TEST_CASE("the revert window follows system.revertSeconds") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.set(Id::SystemRevertSeconds, 0, Origin::Oled, 0); // s10
    r.ctl.set(Id::OutputResolution, 1, Origin::Web, 0);
    CHECK(r.ctl.revertRemainingMs(0) == 10'000);
}

TEST_CASE("settings of a later milestone and out-of-range values are refused") {
    Rig r;
    r.ctl.begin(false, 0);
    CHECK(r.ctl.set(Id::OutputAspect, 1, Origin::Web, 0) == Controller::Outcome::Unavailable);
    CHECK(r.ctl.set(Id::PictureContrast, 50, Origin::Web, 0) == Controller::Outcome::Invalid);
    CHECK(r.ctl.set(Id::PictureContrast, 0, Origin::Web, 0) == Controller::Outcome::Unchanged);
}

TEST_CASE("safe mode ignores the saved settings, outputs 480p and saves nothing") {
    Rig saved;
    saved.ctl.begin(false, 0);
    saved.ctl.set(Id::OutputResolution, 1, Origin::Oled, 0);
    saved.ctl.tick(5000);
    const auto file = saved.fs.files["/settings.bin"];

    Rig r;
    r.fs = saved.fs;
    r.fs.writes = 0;
    r.ctl.begin(true, 0);
    CHECK(r.ctl.safeMode());
    CHECK(r.video.output == video::OutputMode::R480p);
    r.ctl.set(Id::PictureBrightness, 3, Origin::Oled, 0);
    r.ctl.tick(60'000);
    CHECK(r.fs.writes == 0);
    CHECK(r.fs.files["/settings.bin"] == file);
}

TEST_CASE("a new level calibration is saved and restored at the next boot") {
    Rig r;
    r.ctl.begin(false, 0);
    r.video.base[1][0] = 0xC4;
    r.video.base[1][1] = 0xD0;
    r.video.base[1][2] = 0xB8;
    ++r.video.levelsSeq;
    r.ctl.tick(100);
    REQUIRE(r.fs.files.count("/levels.bin") == 1);

    Rig next;
    next.fs = r.fs;
    next.ctl.begin(false, 0);
    CHECK(next.video.base[1][0] == 0xC4);
    CHECK(next.video.base[1][1] == 0xD0);
    CHECK(next.video.base[1][2] == 0xB8);
    CHECK(next.video.base[0][0] == 0x7B);
}

TEST_CASE("a factory reset restores defaults and forgets the level calibration") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.set(Id::PictureSaturation, 9, Origin::Oled, 0);
    r.video.base[1][1] = 0xD0;
    ++r.video.levelsSeq;
    r.ctl.tick(5000);
    r.ctl.factoryReset(6000);
    CHECK(r.ctl.get(Id::PictureSaturation) == 0);
    CHECK(r.video.picture.saturation == 0);
    CHECK(r.video.base[1][1] == 0x7B);
    CHECK(r.fs.files.count("/levels.bin") == 0);
}
