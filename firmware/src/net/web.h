#pragma once

#include <stdint.h>

#include <ESP8266WebServer.h>

#include "core/api.h"
#include "core/controller.h"
#include "core/status.h"
#include "net/wifi.h"

namespace gbsr::net {

// The HTTP server (ADR 0008): the embedded UI and the JSON API of docs/design/architecture.md.
// Synchronous: handlers run inside tick() on the main loop, answer from the status model and go
// through the controller like every other front end. JSON is streamed in small chunks.
class Web {
public:
    Web(core::Controller& ctl, const core::StatusModel& status, Wifi& wifi)
        : ctl_(ctl), status_(status), wifi_(wifi) {}

    void begin();
    void tick();
    // A reboot asked through the API, carried out by the main loop once the answer has gone.
    bool rebootRequested() const { return rebootRequested_; }

private:
    void handleUi();
    void handleStatus();
    void handleSettings();
    void handleCommand();
    void handleWifi();
    void handleScan();
    void handleProfiles();
    void handleProfile();
    void handleSwitcher();
    void handleBackup();
    void handleLevels();
    void handleUpdateUpload();
    void handleUpdateDone();
    void handleNotFound();
    bool sameOrigin();
    void sendJson(int code, const char* body);

    ESP8266WebServer server_{80};
    core::Controller& ctl_;
    const core::StatusModel& status_;
    Wifi& wifi_;
    bool rebootRequested_ = false;
    // The firmware upload in progress (ADR 0012): refused before any flash write unless the
    // file's header names this board.
    enum class UpdateState : uint8_t { Idle, Receiving, Done, Failed };
    UpdateState update_ = UpdateState::Idle;
    const char* updateReason_ = nullptr; // flash text
    char updateVersion_[32] = {};
    uint32_t updateHeapLow_ = 0;
};

} // namespace gbsr::net
