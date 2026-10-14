#pragma once

#include <stddef.h>
#include <stdint.h>

#include "video/engine.h"

namespace gbsr::core {

enum class FrameLockState : uint8_t { Locked, Inactive, Off };

// What the board and the network add to the video status.
struct BoardInfo {
    uint32_t uptimeS = 0;
    uint32_t heapFree = 0;
    uint32_t heapMin = 0;
    int8_t rssi = 0; // 0 when not joined to a network
    uint32_t ip = 0; // IPv4, first octet in the low byte (the ESP8266 convention)
    bool clockGen = false;
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
    uint16_t lines = 0;    // per field
    uint32_t vMilliHz = 0; // field rate
    uint32_t hHz = 0;      // lines × field rate
    // output
    video::OutputMode mode = video::OutputMode::Auto;
    uint32_t pixelClockHz = 0;
    uint32_t rateMilliHz = 0;
    FrameLockState frameLock = FrameLockState::Inactive;
    // board
    BoardInfo board;
    uint16_t revertSecondsLeft = 0; // 0: nothing pending
    bool safeMode = false;
};

// Rebuilds `m` from its inputs; increments m.seq if anything other than the uptime changed.
void buildStatus(StatusModel& m, const video::EngineStatus& e, bool frameLockSetting,
                 uint16_t revertSecondsLeft, bool safeMode, const BoardInfo& board);

// Short words shared by the JSON and the console.
const char* headlineCode(video::Headline h);
const char* standardText(video::Standard s, bool interlaced);
const char* outputModeId(video::OutputMode m);

// GET /api/status (web/src/api/types.ts). Returns the length written, or 0 if `cap` is too small.
size_t statusJson(const StatusModel& m, const char* version, const char* schema, char* buf,
                  size_t cap);

} // namespace gbsr::core
