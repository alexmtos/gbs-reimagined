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
    // Root menu: 0 Output, 1 Input, 2 Picture, 3 System, 4 Network, 5 Status, 6 Back.
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
    r.openGroup(3); // System
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
