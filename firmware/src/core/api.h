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

} // namespace gbsr::core
