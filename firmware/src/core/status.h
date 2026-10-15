#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/profiles.h"
#include "video/engine.h"

namespace gbsr::core {

enum class FrameLockState : uint8_t { Locked, Inactive, Off };

// Why frame lock is inactive while its setting is on: the Status page says it rather than a dash
// (docs/design/ux.md, principle 4).
enum class FrameLockReason : uint8_t {
    None,
    NoSignal,
    Passthrough, // the scaler is bypassed: no output frame to time
    Force60,     // a PAL source shown at 60 Hz cannot follow the source's rate
    NoClockGen,
    OutOfRange, // the source's rate needs a clock this output cannot reach
    Locking,    // the program is still settling
};

// What the board and the network add to the video status.
struct BoardInfo {
    uint32_t uptimeS = 0;
    uint32_t heapFree = 0;
    uint32_t heapMin = 0;
    int8_t rssi = 0; // 0 when not joined to a network
    uint32_t ip = 0; // IPv4, first octet in the low byte (the ESP8266 convention)
    bool clockGen = false;
};

// The active profile, as the controller reports it.
struct ProfileInfo {
    uint8_t slot = 0; // 0: none
    bool modified = false;
    const char* name = "";
};

// The one status model every front end reads (docs/design/ux.md, "Status"), rebuilt every 250 ms.
// `seq` moves only when something in it changed, so pollers can skip identical answers.
struct StatusModel {
    uint32_t seq = 0;
    video::Headline headline = video::Headline::ScalerMissing;
    bool scaler = false;
    // source
    video::InputPort port = video::InputPort::Rgb;
    video::SyncType sync = video::SyncType::None;
    video::Standard standard = video::Standard::None;
    bool interlaced = false;
    uint16_t lines = 0;         // per field
    uint32_t vMilliHz = 0;      // field rate
    uint32_t hHz = 0;           // lines × field rate
    uint8_t sogLevel = 0;       // SOG threshold in effect
    uint8_t phase = 0;          // ADC sampling phase in effect
    uint8_t syncQuality = 0xFF; // % of recent lines with a locked line PLL; 0xFF unknown
    // output
    video::OutputMode mode = video::OutputMode::Auto;
    uint32_t pixelClockHz = 0;
    uint32_t rateMilliHz = 0;
    FrameLockState frameLock = FrameLockState::Inactive;
    FrameLockReason lockReason = FrameLockReason::None;
    // board
    BoardInfo board;
    uint16_t revertSecondsLeft = 0; // 0: nothing pending
    bool safeMode = false;
    uint8_t levelCalibration = 0; // video::GainCalibrator::Outcome of the last "calibrate levels"
    uint16_t settingsSeq = 0;     // moves whenever a setting changes, wherever it was changed
    // profile
    uint8_t profile = 0; // 0: none
    bool profileModified = false;
    char profileName[kProfileNameCap] = {};
};

// Rebuilds `m` from its inputs; increments m.seq if anything other than the uptime changed.
// `settingsSeq` is the settings store's sequence: a page reloads its values when it moves.
void buildStatus(StatusModel& m, const video::EngineStatus& e, bool frameLockSetting,
                 uint16_t revertSecondsLeft, bool safeMode, const BoardInfo& board,
                 const ProfileInfo& profile = ProfileInfo{}, uint16_t settingsSeq = 0);

// The source's signature once the engine has been running it for a moment (ADR 0014); invalid
// while there is no source or it is still being loaded.
InputSignature settledSource(const video::EngineStatus& e);
// The same for a source the engine has just detected and is about to load.
InputSignature detectedSource(const video::DetectedSource& d);

// Short words shared by the JSON and the console.
const char* headlineCode(video::Headline h);
const char* standardText(video::Standard s, bool interlaced);
// A standard without its scan: "240p/480i", "288p/576i", otherwise as standardText. For a profile's
// signature, which covers both scans of a standard.
const char* standardFamily(video::Standard s);
const char* outputModeId(video::OutputMode m);

// GET /api/status (web/src/api/types.ts). Returns the length written, or 0 if `cap` is too small.
size_t statusJson(const StatusModel& m, const char* version, const char* schema, char* buf,
                  size_t cap);

} // namespace gbsr::core
