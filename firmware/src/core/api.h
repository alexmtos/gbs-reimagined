#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/controller.h"

namespace gbsr::core {

// Receives JSON text in pieces: an HTTP chunk writer on the board, a string in tests.
class Sink {
public:
    virtual ~Sink() = default;
    // The HTTP status, announced once before the first byte of the body.
    virtual void status(int code) { (void)code; }
    virtual void write(const char* data, size_t len) = 0;
};

// Small buffered JSON writer over a Sink, so handlers need neither a heap document nor a large
// static buffer (docs/design/memory-budget.md: transient buffers on the stack, under 1 KB).
class JsonOut {
public:
    explicit JsonOut(Sink& sink) : sink_(sink) {}
    ~JsonOut() { flush(); }
    JsonOut(const JsonOut&) = delete;
    JsonOut& operator=(const JsonOut&) = delete;

    JsonOut& raw(const char* text);
    JsonOut& str(const char* text); // quoted and escaped
    // The same for text in flash (PSTR): the fixed JSON of every route stays out of RAM.
    JsonOut& rawP(const char* text);
    JsonOut& strP(const char* text);
    JsonOut& num(long value);
    void flush();

private:
    void put(char c);
    Sink& sink_;
    char buf_[256];
    size_t len_ = 0;
};

// The transport-neutral half of the HTTP API (docs/design/architecture.md, "HTTP API"): what each
// route reads and writes, as JSON. The web server adds only HTTP. Each function announces the HTTP
// status to the sink before the body, and returns it.

// GET /api/settings: {"schema", "values": {id: text}, "revert"?}. Only settings of this milestone.
int getSettings(const Controller& ctl, uint32_t nowMs, Sink& out);

// POST /api/settings with {id: value, ...}; values as text ("r1080p", "on", "auto"), numbers or
// booleans. Answers {"results": {id: outcome}, "revert"?}; 400 when the body is not an object.
int postSettings(Controller& ctl, const char* body, size_t len, uint32_t nowMs, Sink& out);

enum class Command : uint8_t { None, Keep, Revert, Calibrate, Reboot, FactoryReset };

// POST /api/command with {"name": "keep" | "revert" | "calibrate" | "reboot" | "factoryReset"}.
// Answers {"ok": true} or {"ok": false, "reason": ...}. A reboot is reported in `ran` for the
// caller to carry out after the answer has gone.
int postCommand(Controller& ctl, const char* body, size_t len, uint32_t nowMs, Sink& out,
                Command& ran);

// Profiles (ADR 0014). A source signature is {"port", "sync", "standard", "lines"} or null.

// GET /api/profiles: {"active", "modified", "source", "profiles": [{"slot", "name", "source"}]},
// "active" 0 when none; only the slots in use are listed.
int getProfiles(const Controller& ctl, Sink& out);

// GET /api/profile?slot=<n>: one profile as an export document
// {"format": "gbsr-profile", "version", "schema", "name", "source", "values": {id: text}};
// 404 for an empty slot.
int getProfile(const Controller& ctl, uint8_t slot, Sink& out);

// POST /api/profile with {"action", "slot", ...}:
//   load; save ["name"] ["source": "keep" | "this" | "none"]; rename "name"; source "on";
//   delete; import "profile" (an export document; its format and schema are not checked).
// Answers {"ok": true, "result"?, "skipped"?: [id], "revert"?} or {"ok": false, "reason"}.
int postProfile(Controller& ctl, const char* body, size_t len, uint32_t nowMs, Sink& out);

// POST /api/svs with a switcher line as the whole body ("SVS NEW INPUT=3", "remote prof3").
int postSwitcher(Controller& ctl, const char* body, size_t len, uint32_t nowMs, Sink& out);

// GET /api/backup: {"format": "gbsr-backup", "version", "firmware", "schema", "settings",
// "levels": {"component": [r, g, b], "rgb": [r, g, b]}, "profiles": {"<slot>": {"name", "source",
// "values"}}}, streamed. Never any network credential.
int getBackup(const Controller& ctl, const char* firmware, Sink& out);

// POST /api/levels with {"component": [r, g, b], "rgb": [r, g, b]}: a backup's calibration.
int postLevels(Controller& ctl, const char* body, size_t len, Sink& out);

} // namespace gbsr::core
