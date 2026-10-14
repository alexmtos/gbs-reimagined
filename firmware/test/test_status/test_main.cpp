#include <string>

#include "../support/doctest_main.h"
#include "core/status.h"

using namespace gbsr;

namespace {

video::EngineStatus scaling240p() {
    video::EngineStatus e;
    e.state = video::EngineState::Running;
    e.headline = video::Headline::Scaling;
    e.standard = video::Standard::Ntsc;
    e.input = video::InputPort::Rgb;
    e.sync = video::SyncType::Csync;
    e.sourceLines = 261;
    e.program = tv5725::ProgramId::Ntsc1080p;
    e.outputMode = video::OutputMode::R1080p;
    e.frameLocked = true;
    e.pixelClockHz = 108'402'237;
    e.sourceMilliHz = 60'098;
    e.outputMilliHz = 59'875;
    return e;
}

std::string json(const core::StatusModel& m) {
    char buf[768];
    const size_t n = core::statusJson(m, "0.1.0", "f838c797", buf, sizeof buf);
    REQUIRE(n > 0);
    return std::string(buf, n);
}

bool has(const std::string& s, const char* part) {
    return s.find(part) != std::string::npos;
}

} // namespace

TEST_CASE("a locked 240p source reads as the web client expects it") {
    core::StatusModel m;
    core::BoardInfo b;
    b.ip = 0x7701000A; // 10.0.1.119
    b.clockGen = true;
    b.heapFree = 30'000;
    core::buildStatus(m, scaling240p(), true, 0, false, b);
    const std::string s = json(m);
    CHECK(has(s, "\"headline\":\"scaling\""));
    CHECK(has(s, "\"port\":\"rgb\",\"sync\":\"csync\",\"standard\":\"240p\",\"lines\":262"));
    CHECK(has(s, "\"hFreqKHz\":15.745,\"vFreqHz\":60.098"));
    CHECK(has(s, "\"mode\":\"r1080p\",\"pixelClockMHz\":108.402,\"rateHz\":60.098"));
    CHECK(has(s, "\"frameLock\":\"locked\""));
    CHECK(has(s, "\"ip\":\"10.0.1.119\""));
    CHECK(has(s, "\"schema\":\"f838c797\""));
    CHECK_FALSE(has(s, "revert"));
    // Balanced braces: one object, closed.
    int depth = 0;
    for (char c : s) {
        depth += c == '{' ? 1 : (c == '}' ? -1 : 0);
        CHECK(depth >= 0);
    }
    CHECK(depth == 0);
}

TEST_CASE("no signal reads as no source, and frame lock follows the setting") {
    core::StatusModel m;
    video::EngineStatus e;
    e.state = video::EngineState::Searching;
    e.headline = video::Headline::NoSignal;
    core::buildStatus(m, e, false, 0, false, core::BoardInfo{});
    const std::string s = json(m);
    CHECK(has(s, "\"headline\":\"noSignal\""));
    CHECK(has(s, "\"port\":\"none\",\"sync\":\"none\",\"standard\":\"\",\"lines\":0"));
    CHECK(has(s, "\"mode\":\"auto\",\"pixelClockMHz\":0.000"));
    CHECK(has(s, "\"frameLock\":\"off\""));
}

TEST_CASE("a pending revert is announced") {
    core::StatusModel m;
    core::buildStatus(m, scaling240p(), true, 17, false, core::BoardInfo{});
    CHECK(has(json(m), "\"revert\":{\"secondsLeft\":17}}"));
}

TEST_CASE("the sequence moves on content changes, not on uptime or heap") {
    core::StatusModel m;
    core::BoardInfo b;
    core::buildStatus(m, scaling240p(), true, 0, false, b);
    const uint32_t first = m.seq;
    b.uptimeS = 99;
    b.heapFree = 1234;
    core::buildStatus(m, scaling240p(), true, 0, false, b);
    CHECK(m.seq == first);
    video::EngineStatus e = scaling240p();
    e.interlaced = true;
    core::buildStatus(m, e, true, 0, false, b);
    CHECK(m.seq == first + 1);
    CHECK(has(json(m), "\"standard\":\"480i\""));
}

TEST_CASE("a buffer too small yields nothing rather than a truncated object") {
    core::StatusModel m;
    char buf[64];
    CHECK(core::statusJson(m, "0.1.0", "x", buf, sizeof buf) == 0);
}
