#include <string>

#include "../support/doctest_main.h"
#include "../support/fake_storage.h"
#include "../support/fake_video.h"
#include "core/api.h"

using namespace gbsr;
using core::Command;
using settings::Id;

namespace {

struct StringSink : core::Sink {
    std::string text;
    int writes = 0;
    void write(const char* data, size_t len) override {
        text.append(data, len);
        ++writes;
    }
};

struct Rig {
    test::FakeFileStore fs;
    test::FakeVideo video;
    core::Controller ctl{fs, video};
    Rig() { ctl.begin(false, 0); }

    std::string post(const std::string& body, int expect = 200) {
        StringSink out;
        CHECK(core::postSettings(ctl, body.data(), body.size(), 1000, out) == expect);
        return out.text;
    }
};

bool has(const std::string& s, const char* part) {
    return s.find(part) != std::string::npos;
}

} // namespace

TEST_CASE("GET /api/settings lists this milestone's settings as text") {
    Rig r;
    StringSink out;
    CHECK(core::getSettings(r.ctl, 0, out) == 200);
    CHECK(has(out.text, "{\"schema\":\""));
    CHECK(has(out.text, "\"output.resolution\":\"auto\""));
    CHECK(has(out.text, "\"output.frameLock\":\"on\""));
    CHECK(has(out.text, "\"picture.sharpness\":\"off\""));
    CHECK(has(out.text, "\"output.aspect\":\"a4x3\""));
    CHECK_FALSE(has(out.text, "output.force60")); // pending
    CHECK(out.text.back() == '}');
    CHECK(out.writes >= 2); // streamed through the small buffer, not built whole
}

TEST_CASE("POST /api/settings takes text, numbers and booleans, and reports each outcome") {
    Rig r;
    const std::string s =
        r.post(R"({"picture.sharpness":"medium","picture.brightness":-4,"input.autoGain":true,)"
               R"("picture.contrast":99,"output.force60":true,"no.such":1})");
    CHECK(has(s, "\"picture.sharpness\":\"applied\""));
    CHECK(has(s, "\"picture.brightness\":\"applied\""));
    CHECK(has(s, "\"input.autoGain\":\"applied\""));
    CHECK(has(s, "\"picture.contrast\":\"invalid\""));
    CHECK(has(s, "\"output.force60\":\"unavailable\""));
    CHECK(has(s, "\"no.such\":\"unknown\""));
    CHECK(r.video.picture.sharpness == 2);
    CHECK(r.video.picture.brightness == -4);
    CHECK(r.video.autoGain);
}

TEST_CASE("a risky change through the API arms the revert and says so") {
    Rig r;
    const std::string s = r.post(R"({"output.resolution":"r1080p"})");
    CHECK(has(s, "\"output.resolution\":\"revertArmed\""));
    CHECK(has(s, "\"revert\":{\"secondsLeft\":20}"));
    CHECK(r.ctl.revertPending());
}

TEST_CASE("a body that is not an object is refused") {
    Rig r;
    CHECK(has(r.post("[1,2]", 400), "error"));
    CHECK(has(r.post("{broken", 400), "error"));
}

TEST_CASE("commands: keep, revert, calibrate, reboot, factory reset, unknown") {
    Rig r;
    StringSink out;
    Command ran{};
    r.post(R"({"output.resolution":"r720p"})");
    const std::string keep = R"({"name":"keep"})";
    CHECK(core::postCommand(r.ctl, keep.data(), keep.size(), 2000, out, ran) == 200);
    CHECK(ran == Command::Keep);
    CHECK_FALSE(r.ctl.revertPending());
    CHECK(out.text == "{\"ok\":true}");

    const std::string reboot = R"({"name":"reboot"})";
    CHECK(core::postCommand(r.ctl, reboot.data(), reboot.size(), 2000, out, ran) == 200);
    CHECK(ran == Command::Reboot);

    const std::string reset = R"({"name":"factoryReset"})";
    CHECK(core::postCommand(r.ctl, reset.data(), reset.size(), 2000, out, ran) == 200);
    CHECK(r.ctl.get(Id::OutputResolution) == 0);

    StringSink bad;
    const std::string unknown = R"({"name":"selfDestruct"})";
    CHECK(core::postCommand(r.ctl, unknown.data(), unknown.size(), 2000, bad, ran) == 404);
    CHECK(ran == Command::None);
    CHECK(has(bad.text, "unknownCommand"));
}

TEST_CASE("text with quotes and control characters stays valid JSON") {
    StringSink out;
    {
        core::JsonOut j(out);
        j.str("a\"b\\c\n");
    }
    CHECK(out.text == "\"a\\\"b\\\\c\\u000a\"");
}
