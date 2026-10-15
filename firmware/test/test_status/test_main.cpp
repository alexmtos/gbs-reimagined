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

TEST_CASE("the sampling values and the sync quality travel with the source") {
    core::StatusModel m;
    video::EngineStatus e = scaling240p();
    e.sogLevel = 13;
    e.phaseAdc = 16;
    e.syncQuality = 97;
    core::buildStatus(m, e, true, 0, false, core::BoardInfo{});
    CHECK(has(json(m), "\"sog\":13,\"phase\":16,\"syncQuality\":97}"));
    e.syncQuality = 0xFF; // not measured yet
    core::buildStatus(m, e, true, 0, false, core::BoardInfo{});
    CHECK(has(json(m), "\"syncQuality\":null}"));
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

TEST_CASE("the active profile is reported with its name escaped, and moves the sequence") {
    core::StatusModel m;
    core::buildStatus(m, scaling240p(), true, 0, false, core::BoardInfo{});
    CHECK(has(json(m), "\"profile\":null"));
    const uint32_t first = m.seq;
    core::ProfileInfo p;
    p.slot = 3;
    p.name = "\"Mega\" \\ S\xC3\xA3o";
    core::buildStatus(m, scaling240p(), true, 0, false, core::BoardInfo{}, p);
    CHECK(has(
        json(m),
        "\"profile\":{\"slot\":3,\"name\":\"\\\"Mega\\\" \\\\ S\xC3\xA3o\",\"modified\":false}"));
    CHECK(m.seq == first + 1);
    p.modified = true;
    core::buildStatus(m, scaling240p(), true, 0, false, core::BoardInfo{}, p);
    CHECK(has(json(m), "\"modified\":true}"));
    CHECK(m.seq == first + 2);
}

TEST_CASE("the largest status still fits the web server's 768-byte buffer") {
    core::StatusModel m;
    core::BoardInfo b;
    b.uptimeS = 4'000'000'000u;
    b.heapFree = 4'000'000'000u;
    b.heapMin = 4'000'000'000u;
    b.rssi = -100;
    b.ip = 0xFFFFFFFF;
    video::EngineStatus e = scaling240p();
    e.sourceMilliHz = 4'000'000'000u;
    e.outputMilliHz = 4'000'000'000u;
    e.pixelClockHz = 4'000'000'000u;
    e.sourceLines = 65534;
    e.sogLevel = e.phaseAdc = 255;
    e.syncQuality = 100;
    core::ProfileInfo p;
    p.slot = 16;
    p.modified = true;
    p.name = "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
             "\x01\x01"; // 23 bytes that each take six to escape
    core::buildStatus(m, e, false, 65535, false, b, p);
    char buf[768];
    CHECK(core::statusJson(m, "0.1.0-beta.12+abcdef0", "f838c797", buf, sizeof buf) > 0);
}

TEST_CASE("a source counts as settled after 10 stable ticks of a running program") {
    video::EngineStatus e = scaling240p();
    e.stableCounter = 9;
    CHECK_FALSE(core::settledSource(e).valid);
    e.stableCounter = 10;
    const core::InputSignature s = core::settledSource(e);
    CHECK(s.valid);
    CHECK(s.port == static_cast<uint8_t>(video::InputPort::Rgb));
    CHECK(s.sync == static_cast<uint8_t>(video::SyncType::Csync));
    CHECK(s.standard == static_cast<uint8_t>(video::Standard::Ntsc));
    CHECK(s.lines == 262); // as the status shows them
    e.noSyncCounter = 1;
    CHECK_FALSE(core::settledSource(e).valid);
    e.noSyncCounter = 0;
    e.state = video::EngineState::Settling;
    CHECK_FALSE(core::settledSource(e).valid);
}

TEST_CASE("a detected source has the signature its settled self will have") {
    video::DetectedSource d;
    d.port = video::InputPort::Component;
    d.sync = video::SyncType::Csync;
    d.standard = video::Standard::Edtv60;
    d.lines = 524;
    const core::InputSignature s = core::detectedSource(d);
    CHECK(s.valid);
    CHECK(s.port == static_cast<uint8_t>(video::InputPort::Component));
    CHECK(s.standard == static_cast<uint8_t>(video::Standard::Edtv60));
    CHECK(s.lines == 525);
    d.lines = 0;
    CHECK_FALSE(core::detectedSource(d).valid);
}

TEST_CASE("a buffer too small yields nothing rather than a truncated object") {
    core::StatusModel m;
    char buf[64];
    CHECK(core::statusJson(m, "0.1.0", "x", buf, sizeof buf) == 0);
}
