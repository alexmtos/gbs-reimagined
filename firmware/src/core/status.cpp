#include "core/status.h"

#include <stdio.h>

namespace gbsr::core {

using video::Headline;
using video::Standard;

const char* headlineCode(Headline h) {
    switch (h) {
    case Headline::ScalerMissing:
        return "scalerMissing";
    case Headline::NoSignal:
        return "noSignal";
    case Headline::SyncUnstable:
        return "syncUnstable";
    case Headline::Settling:
        return "settling";
    case Headline::Locking:
        return "locking";
    case Headline::Scaling:
        return "scaling";
    case Headline::Paused:
        return "paused";
    }
    return "settling";
}

const char* standardText(Standard s, bool interlaced) {
    switch (s) {
    case Standard::Ntsc:
        return interlaced ? "480i" : "240p";
    case Standard::Pal:
        return interlaced ? "576i" : "288p";
    case Standard::Edtv60:
        return "480p";
    case Standard::Edtv50:
        return "576p";
    case Standard::Hd720p:
        return "720p";
    case Standard::Hd1080i:
        return "1080i";
    case Standard::Hd1080p:
        return "1080p";
    case Standard::MedRes:
        return "25 kHz";
    case Standard::Unknown:
        return "unknown";
    case Standard::PcOnYpbpr:
    case Standard::RgbhvScaled:
    case Standard::RgbhvBypass:
        return "PC";
    case Standard::None:
        break;
    }
    return "";
}

const char* outputModeId(video::OutputMode m) {
    // The value ids of output.resolution (schema/settings.yaml).
    static const char* const kIds[] = {"auto",  "r1080p", "r1024p", "r960p",
                                       "r720p", "r480p",  "r15khz", "passthrough"};
    const uint8_t i = static_cast<uint8_t>(m);
    return i < 8 ? kIds[i] : "auto";
}

void buildStatus(StatusModel& m, const video::EngineStatus& e, bool frameLockSetting,
                 uint16_t revertSecondsLeft, bool safeMode, const BoardInfo& board) {
    StatusModel next;
    next.headline = e.headline;
    next.scaler = e.state != video::EngineState::NoPower;
    const bool source = e.sync != video::SyncType::None && e.standard != Standard::None;
    next.port = e.input;
    next.sync = e.sync;
    next.standard = e.standard;
    next.interlaced = e.interlaced;
    // The sync processor counts lines from zero (480p reads 524).
    next.lines = source && e.sourceLines != 0 ? static_cast<uint16_t>(e.sourceLines + 1) : 0;
    next.vMilliHz = source ? e.sourceMilliHz : 0;
    next.hHz = static_cast<uint32_t>(static_cast<uint64_t>(next.lines) * next.vMilliHz / 1000);
    next.mode = e.program == tv5725::ProgramId::Count ? video::OutputMode::Auto : e.outputMode;
    next.pixelClockHz = e.program == tv5725::ProgramId::Count ? 0 : e.pixelClockHz;
    next.rateMilliHz = e.frameLocked ? e.sourceMilliHz : e.outputMilliHz;
    next.frameLock = !frameLockSetting ? FrameLockState::Off
                     : e.frameLocked   ? FrameLockState::Locked
                                       : FrameLockState::Inactive;
    next.board = board;
    next.revertSecondsLeft = revertSecondsLeft;
    next.safeMode = safeMode;
    next.levelCalibration = e.levelCalibration;

    // Uptime, heap and signal strength move on every rebuild; they do not count as a change.
    const bool changed =
        next.headline != m.headline || next.scaler != m.scaler || next.port != m.port ||
        next.sync != m.sync || next.standard != m.standard || next.interlaced != m.interlaced ||
        next.lines != m.lines || next.vMilliHz != m.vMilliHz || next.mode != m.mode ||
        next.pixelClockHz != m.pixelClockHz || next.rateMilliHz != m.rateMilliHz ||
        next.frameLock != m.frameLock || next.board.ip != m.board.ip ||
        next.board.clockGen != m.board.clockGen || next.revertSecondsLeft != m.revertSecondsLeft ||
        next.safeMode != m.safeMode || next.levelCalibration != m.levelCalibration;
    next.seq = changed ? m.seq + 1 : m.seq;
    m = next;
}

size_t statusJson(const StatusModel& m, const char* version, const char* schema, char* buf,
                  size_t cap) {
    static const char* const kPort[] = {"component", "rgb", "none"};
    static const char* const kSync[] = {"none", "hv", "csync"};
    static const char* const kLock[] = {"locked", "inactive", "off"};
    const bool source = m.sync != video::SyncType::None;
    char revert[40] = "";
    if (m.revertSecondsLeft != 0) {
        snprintf(revert, sizeof revert, ",\"revert\":{\"secondsLeft\":%u}", m.revertSecondsLeft);
    }
    const int n = snprintf(
        buf, cap,
        "{\"seq\":%lu,\"headline\":\"%s\",\"safeMode\":%s,"
        "\"source\":{\"port\":\"%s\",\"sync\":\"%s\",\"standard\":\"%s\",\"lines\":%u,"
        "\"interlaced\":%s,\"hFreqKHz\":%lu.%03lu,\"vFreqHz\":%lu.%03lu},"
        "\"output\":{\"mode\":\"%s\",\"pixelClockMHz\":%lu.%03lu,\"rateHz\":%lu.%03lu,"
        "\"frameLock\":\"%s\"},"
        "\"board\":{\"version\":\"%s\",\"schema\":\"%s\",\"uptimeS\":%lu,\"heapFree\":%lu,"
        "\"heapMin\":%lu,\"rssi\":%d,\"ip\":\"%u.%u.%u.%u\",\"scaler\":%s,\"clockGen\":%s}%s}",
        static_cast<unsigned long>(m.seq), headlineCode(m.headline), m.safeMode ? "true" : "false",
        source ? kPort[static_cast<uint8_t>(m.port) % 3] : "none",
        kSync[static_cast<uint8_t>(m.sync) % 3], standardText(m.standard, m.interlaced), m.lines,
        m.interlaced ? "true" : "false", static_cast<unsigned long>(m.hHz / 1000),
        static_cast<unsigned long>(m.hHz % 1000), static_cast<unsigned long>(m.vMilliHz / 1000),
        static_cast<unsigned long>(m.vMilliHz % 1000), outputModeId(m.mode),
        static_cast<unsigned long>(m.pixelClockHz / 1'000'000),
        static_cast<unsigned long>(m.pixelClockHz / 1000 % 1000),
        static_cast<unsigned long>(m.rateMilliHz / 1000),
        static_cast<unsigned long>(m.rateMilliHz % 1000),
        kLock[static_cast<uint8_t>(m.frameLock) % 3], version, schema,
        static_cast<unsigned long>(m.board.uptimeS), static_cast<unsigned long>(m.board.heapFree),
        static_cast<unsigned long>(m.board.heapMin), m.board.rssi, m.board.ip & 0xFF,
        m.board.ip >> 8 & 0xFF, m.board.ip >> 16 & 0xFF, m.board.ip >> 24,
        m.scaler ? "true" : "false", m.board.clockGen ? "true" : "false", revert);
    return n > 0 && static_cast<size_t>(n) < cap ? static_cast<size_t>(n) : 0;
}

} // namespace gbsr::core
