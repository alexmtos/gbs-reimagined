#include "../support/doctest_main.h"
#include "../support/fake_storage.h"
#include "../support/fake_video.h"
#include "core/controller.h"

using namespace gbsr;
using core::Controller;
using core::Origin;
using settings::Id;
using test::FakeVideo;

namespace {

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

TEST_CASE("pending settings and out-of-range values are refused") {
    Rig r;
    r.ctl.begin(false, 0);
    CHECK(r.ctl.set(Id::OutputForce60, 1, Origin::Web, 0) == Controller::Outcome::Unavailable);
    CHECK(r.ctl.set(Id::PictureContrast, 50, Origin::Web, 0) == Controller::Outcome::Invalid);
    CHECK(r.ctl.set(Id::PictureContrast, 0, Origin::Web, 0) == Controller::Outcome::Unchanged);
}

TEST_CASE("the geometry settings reach the video path together") {
    Rig r;
    r.ctl.begin(false, 0);
    CHECK(r.ctl.set(Id::OutputAspect, 1, Origin::Web, 0) == Controller::Outcome::Applied);
    CHECK(r.ctl.set(Id::OutputHPosition, -5, Origin::Oled, 0) == Controller::Outcome::Applied);
    CHECK(r.ctl.set(Id::OutputVSize, 12, Origin::Console, 0) == Controller::Outcome::Applied);
    CHECK(r.video.geometry.aspect == video::Aspect::SixteenNine);
    CHECK(r.video.geometry.hPosition == -5);
    CHECK(r.video.geometry.vSize == 12);
    CHECK(r.video.geometry.vPosition == 0);
    CHECK(r.ctl.set(Id::OutputHSize, 65, Origin::Web, 0) == Controller::Outcome::Invalid);
}

TEST_CASE("the line settings reach the video path together") {
    Rig r;
    r.ctl.begin(false, 0);
    CHECK(r.video.lines.strength == 4); // the default level
    CHECK(r.ctl.set(Id::PictureScanlines, 1, Origin::Oled, 0) == Controller::Outcome::Applied);
    CHECK(r.ctl.set(Id::PictureScanlineStrength, 7, Origin::Web, 0) ==
          Controller::Outcome::Applied);
    CHECK(r.ctl.set(Id::PictureDeinterlace, 1, Origin::Console, 0) == Controller::Outcome::Applied);
    CHECK(r.video.lines.scanlines);
    CHECK(r.video.lines.strength == 7);
    CHECK(r.video.lines.bob);
    CHECK(r.ctl.set(Id::PictureScanlineStrength, 0, Origin::Web, 0) ==
          Controller::Outcome::Invalid);
}

TEST_CASE("the sampling settings reach the video path, Auto as -1 and 0") {
    Rig r;
    r.ctl.begin(false, 0);
    CHECK(r.video.sampling.sog == -1);
    CHECK(r.video.sampling.phase == -1);
    CHECK(r.video.sampling.lpf == 0);
    CHECK(r.ctl.set(Id::InputSyncThreshold, 9, Origin::Web, 0) ==
          Controller::Outcome::RevertArmed); // can lose sync: confirmed like the resolution
    CHECK(r.ctl.set(Id::InputPhase, 20, Origin::Web, 0) == Controller::Outcome::Applied);
    CHECK(r.video.sampling.sog == 9);
    CHECK(r.video.sampling.phase == 20);
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
