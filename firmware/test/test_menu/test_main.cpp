#include <string>

#include "../support/doctest_main.h"
#include "../support/fake_storage.h"
#include "../support/fake_video.h"
#include "ui/menu.h"

using namespace gbsr;
using settings::Id;

namespace {

size_t glyphs(const char* s) {
    size_t n = 0;
    for (; *s != '\0'; ++s) {
        n += (static_cast<uint8_t>(*s) & 0xC0) != 0x80;
    }
    return n;
}

struct Rig {
    test::FakeFileStore fs;
    test::FakeVideo video;
    core::Controller ctl{fs, video};
    core::StatusModel status;
    ui::NetView net;
    ui::Menu menu{ctl, status, net};
    uint32_t now = 1000;

    Rig() {
        ctl.begin(false, 0);
        status.headline = video::Headline::Scaling;
        status.standard = video::Standard::Ntsc;
        status.sync = video::SyncType::Csync;
        status.port = video::InputPort::Rgb;
        status.mode = video::OutputMode::R1080p;
        status.vMilliHz = 60'098;
        status.pixelClockHz = 108'402'237;
        status.frameLock = core::FrameLockState::Locked;
        net.mode = ui::NetView::Mode::Joined;
        net.ip = 0x7701000A; // 10.0.1.119
        net.ssid = "Home";
        net.apSsid = "GBS-EAC4";
        net.apPassword = "qqqqqqqq";
    }

    ui::Frame frame() {
        ui::Frame f;
        menu.render(f, now);
        for (const auto& line : f.line) {
            CHECK(glyphs(line) <= ui::Frame::kCols);
        }
        return f;
    }
    std::string line(uint8_t n) { return frame().line[n]; }
    void press() { menu.press(now += 100); }
    void hold() { menu.longPress(now += 100); }
    void turn(int steps, bool coarse = false) { menu.turn(steps, coarse, now += 100); }
    // Root menu: 0 Output, 1 Input, 2 Picture, 3 Profiles, 4 Network, 5 Status, 6 System, 7 Back.
    void openGroup(int index) {
        press();
        turn(index);
        press();
    }
};

} // namespace

TEST_CASE("home shows the headline, the source, the output and the address") {
    Rig r;
    const ui::Frame f = r.frame();
    CHECK(std::string(f.line[0]) == "Scaling");
    CHECK(std::string(f.line[1]) == "240p > 1080p");
    CHECK(std::string(f.line[2]) == "RGB csync 60.10 Hz");
    CHECK(std::string(f.line[3]) == "108.40 MHz frame lock");
    CHECK(std::string(f.line[4]) == "10.0.1.119");
    CHECK(std::string(f.line[5]) == "Press for the menu");
}

TEST_CASE("a long headline wraps onto the second line") {
    Rig r;
    r.status.headline = video::Headline::Settling;
    CHECK(r.line(0) == "Signal found,");
    CHECK(r.line(1) == "settling");
}

TEST_CASE("the menu lists the groups and highlights the cursor") {
    Rig r;
    r.press();
    ui::Frame f = r.frame();
    CHECK(std::string(f.line[0]) == "Output");
    CHECK(std::string(f.line[2]) == "Picture");
    CHECK(f.inverted == 0x01);
    r.turn(2);
    CHECK(r.frame().inverted == 0x04);
    r.turn(99); // clamps at the last entry, scrolling the list
    f = r.frame();
    CHECK(std::string(f.line[4]) == "< Back");
    CHECK(f.inverted == 0x10);
}

TEST_CASE("picture settings change while turning; a long press backs the change out") {
    Rig r;
    r.openGroup(2); // Picture
    CHECK(r.line(0).rfind("Brightness", 0) == 0);
    CHECK(r.line(0).find("+0") != std::string::npos);
    r.press(); // edit brightness
    r.turn(3);
    CHECK(r.ctl.get(Id::PictureBrightness) == 3);
    CHECK(r.video.picture.brightness == 3);
    CHECK(r.line(2).find("< +3 >") != std::string::npos);
    r.turn(2, true); // coarse: ten per detent
    CHECK(r.ctl.get(Id::PictureBrightness) == 23);
    r.hold();
    CHECK(r.ctl.get(Id::PictureBrightness) == 0);
}

TEST_CASE("editing a sampling setting shows the live sync quality") {
    Rig r;
    r.ctl.set(Id::SystemShowAdvanced, 1, core::Origin::Oled, 0);
    r.status.syncQuality = 97;
    r.openGroup(1); // Input: port, sync threshold, phase, ...
    r.turn(2);
    r.press(); // edit the sampling phase
    CHECK(r.line(0).rfind("Sampling phase", 0) == 0);
    CHECK(r.line(4).find("Sync 97%") != std::string::npos);
    r.status.syncQuality = 0xFF;
    CHECK(r.line(4).find("Sync --") != std::string::npos);
    r.hold();
}

TEST_CASE("risky settings apply on the press, without a revert from the OLED") {
    Rig r;
    r.openGroup(0); // Output: resolution first
    r.press();
    r.turn(1);
    CHECK(r.ctl.get(Id::OutputResolution) == 0); // not yet
    CHECK(r.video.output == video::OutputMode::Auto);
    r.press();
    CHECK(r.ctl.get(Id::OutputResolution) == 1);
    CHECK(r.video.output == video::OutputMode::R1080p);
    CHECK_FALSE(r.ctl.revertPending());
}

TEST_CASE("a pending revert takes over the screen until answered") {
    Rig r;
    r.ctl.set(Id::OutputResolution, 2, core::Origin::Web, r.now);
    r.menu.tick(r.now);
    ui::Frame f = r.frame();
    CHECK(std::string(f.line[0]) == "Keep the change?");
    CHECK(std::string(f.line[1]) == "Reverting in 20 s");
    CHECK(f.inverted == 0x08); // Keep
    r.turn(1);
    CHECK(r.frame().inverted == 0x10); // Revert
    r.turn(1);
    r.press(); // Keep
    CHECK_FALSE(r.ctl.revertPending());
    CHECK(r.ctl.get(Id::OutputResolution) == 2);
    r.menu.tick(r.now);
    CHECK(r.line(0) == "Scaling");
}

TEST_CASE("the OLED speaks the board's language setting") {
    Rig r;
    r.ctl.set(Id::SystemLanguage, 1, core::Origin::Oled, 0);
    CHECK(r.line(0) == "Escalando");
    CHECK(r.line(5) == "Aperte para o menu");
    r.press();
    CHECK(r.line(0) == "Saída");
}

TEST_CASE("the menu returns home after thirty seconds untouched") {
    Rig r;
    r.openGroup(2);
    r.menu.tick(r.now + 29'000);
    CHECK(r.line(0).rfind("Brightness", 0) == 0);
    r.menu.tick(r.now + 30'000);
    CHECK(r.line(0) == "Scaling");
}

TEST_CASE("the network page shows how to reach the setup access point") {
    Rig r;
    r.net.mode = ui::NetView::Mode::Setup;
    r.net.failure = 1; // refused
    r.openGroup(4);
    const ui::Frame f = r.frame();
    CHECK(std::string(f.line[0]) == "Setup network:");
    CHECK(std::string(f.line[1]) == "GBS-EAC4");
    CHECK(std::string(f.line[3]) == "qqqqqqqq");
    CHECK(std::string(f.line[4]) == "Password refused");
}

TEST_CASE("factory reset asks for a second press") {
    Rig r;
    r.ctl.set(Id::PictureSaturation, 9, core::Origin::Oled, 0);
    r.openGroup(6); // System
    r.turn(20);     // to the end: ..., Restart, Factory reset, < Back
    r.turn(-1);
    CHECK(r.line(5) == "Long press: back");
    r.press();
    CHECK(r.line(2) == "Press again: confirm");
    CHECK(r.ctl.get(Id::PictureSaturation) == 9);
    r.press();
    CHECK(r.ctl.get(Id::PictureSaturation) == 0);
    CHECK(r.line(2) == "Done");
}

TEST_CASE("a long press at home opens the profiles; Save as new takes the first free slot") {
    Rig r;
    r.hold();
    ui::Frame f = r.frame();
    CHECK(std::string(f.line[0]) == "Save as new");
    CHECK(std::string(f.line[1]) == "< Back");
    r.press();
    CHECK(r.line(2) == "Profile saved");
    CHECK(r.ctl.profiles().exists(1));
    r.press(); // back to the list, on the new profile
    f = r.frame();
    CHECK(std::string(f.line[0]).rfind("1 Profile 1", 0) == 0);
    CHECK(std::string(f.line[0]).find("\xC2\xAB") != std::string::npos); // in use
    CHECK(f.inverted == 0x01);
    CHECK(std::string(f.line[5]) == "Only when chosen");
    r.hold();
    CHECK(r.line(0) == "Scaling"); // back home, where it came from
}

TEST_CASE("loading a profile from the OLED goes home and shows it there") {
    Rig r;
    r.ctl.set(Id::OutputResolution, 1, core::Origin::Oled, 0);
    REQUIRE(r.ctl.saveProfile(2, "Mega Drive", core::Controller::SaveSource::None));
    r.ctl.set(Id::OutputResolution, 5, core::Origin::Oled, 0);
    r.status.profile = 2;
    r.status.profileModified = true;
    std::snprintf(r.status.profileName, sizeof r.status.profileName, "Mega Drive");
    CHECK(r.line(5) == "2 Mega Drive*");

    r.hold();
    CHECK(r.line(5) == "Changed, not saved");
    r.press(); // the profile's actions
    ui::Frame f = r.frame();
    CHECK(std::string(f.line[0]) == "Load");
    CHECK(std::string(f.line[2]).rfind("Load by itself", 0) == 0);
    CHECK(std::string(f.line[5]) == "2 Mega Drive");
    r.press(); // Load: a risky change, yet no revert from the board itself
    CHECK(r.line(2) == "Profile loaded");
    CHECK(r.video.output == video::OutputMode::R1080p);
    CHECK_FALSE(r.ctl.revertPending());
    r.press();
    CHECK(r.line(0) == "Scaling");
}

TEST_CASE("the OLED marks a profile for the current source, saves over it and deletes it") {
    Rig r;
    REQUIRE(r.ctl.saveProfile(1, "SNES", core::Controller::SaveSource::None));
    r.press();
    r.turn(3); // Profiles in the root menu
    CHECK(r.line(3) == "Profiles");
    r.press();
    r.press(); // profile 1
    r.turn(2);
    r.press(); // Load by itself, with no source settled
    CHECK(r.line(2) == "No source now");
    r.hold();
    r.press();
    core::InputSignature s;
    s.valid = true;
    s.port = 1;
    s.sync = 2;
    s.standard = 1;
    s.lines = 262;
    r.ctl.onSource(s, r.now);
    r.turn(2);
    r.press();
    CHECK(r.line(2).find("On") != std::string::npos);
    CHECK(r.ctl.profiles().signature(1) == s);

    r.turn(-1); // Save here
    r.press();
    CHECK(r.line(2) == "Press again: confirm");
    r.ctl.set(Id::PictureBrightness, 4, core::Origin::Oled, 0);
    r.press();
    CHECK(r.line(2) == "Profile saved");
    core::Profile p;
    REQUIRE(r.ctl.profiles().read(1, p));
    CHECK(p.values[static_cast<uint8_t>(Id::PictureBrightness)] == 4);
    CHECK(p.signature == s); // kept

    r.press(); // the list
    r.press();
    r.turn(3); // Delete
    r.press();
    r.hold(); // changed my mind: back on Delete
    CHECK(r.ctl.profiles().exists(1));
    r.press();
    r.press();
    CHECK(r.line(2) == "Profile deleted");
    CHECK_FALSE(r.ctl.profiles().exists(1));
    r.hold();
    r.hold();
    CHECK(r.line(3) == "Profiles"); // back in the root menu, on Profiles
    CHECK(r.frame().inverted == 0x08);
}

TEST_CASE("the startup profile reads as words or a profile number") {
    Rig r;
    r.openGroup(6); // System
    r.turn(20);
    bool found = false;
    for (int i = 0; i < 12 && !found; ++i) {
        found = r.line(5) == "Loaded at power-on";
        if (!found) {
            r.turn(-1);
        }
    }
    REQUIRE(found);
    r.press();
    CHECK(r.line(2).find("< Last used >") != std::string::npos);
    r.turn(1);
    CHECK(r.line(2).find("< Auto by input >") != std::string::npos);
    r.turn(3);
    CHECK(r.line(2).find("< Profile 3 >") != std::string::npos);
}
