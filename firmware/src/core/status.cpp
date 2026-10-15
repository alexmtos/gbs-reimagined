#include "core/status.h"

#include <stdio.h>
#include <string.h>

#include "platform/progmem.h"

namespace gbsr::core {

using video::Headline;
using video::Standard;

namespace {

// A JSON string body: quotes, backslashes and control characters escaped; UTF-8 passes through.
void jsonEscape(const char* in, char* out, size_t cap) {
    size_t n = 0;
    for (; *in != '\0'; ++in) {
        const unsigned char c = static_cast<unsigned char>(*in);
        char piece[8];
        if (c == '"' || c == '\\') {
            snprintf(piece, sizeof piece, "\\%c", c);
        } else if (c < 0x20) {
            snprintf(piece, sizeof piece, "\\u%04x", c);
        } else {
            piece[0] = static_cast<char>(c);
            piece[1] = '\0';
        }
        const size_t len = strlen(piece);
        if (n + len >= cap) {
            break;
        }
        memcpy(out + n, piece, len);
        n += len;
    }
    out[n] = '\0';
}

} // namespace

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

const char* standardFamily(Standard s) {
    switch (s) {
    case Standard::Ntsc:
        return "240p/480i";
    case Standard::Pal:
        return "288p/576i";
    default:
        return standardText(s, false);
    }
}

const char* outputModeId(video::OutputMode m) {
    // The value ids of output.resolution (schema/settings.yaml).
    static const char* const kIds[] = {"auto",  "r1080p", "r1024p", "r960p",
                                       "r720p", "r480p",  "r15khz", "passthrough"};
    const uint8_t i = static_cast<uint8_t>(m);
    return i < 8 ? kIds[i] : "auto";
}

void buildStatus(StatusModel& m, const video::EngineStatus& e, bool frameLockSetting,
                 uint16_t revertSecondsLeft, bool safeMode, const BoardInfo& board,
                 const ProfileInfo& profile) {
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
    next.sogLevel = source ? e.sogLevel : 0;
    next.phase = source ? e.phaseAdc : 0;
    next.syncQuality = source ? e.syncQuality : 0xFF;
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
    next.profile = profile.slot;
    next.profileModified = profile.slot != 0 && profile.modified;
    if (profile.slot != 0) {
        strncpy(next.profileName, profile.name, sizeof next.profileName - 1);
    }

    // Uptime, heap and signal strength move on every rebuild; they do not count as a change.
    const bool changed =
        next.headline != m.headline || next.scaler != m.scaler || next.port != m.port ||
        next.sync != m.sync || next.standard != m.standard || next.interlaced != m.interlaced ||
        next.lines != m.lines || next.vMilliHz != m.vMilliHz || next.mode != m.mode ||
        next.sogLevel != m.sogLevel || next.phase != m.phase || next.syncQuality != m.syncQuality ||
        next.pixelClockHz != m.pixelClockHz || next.rateMilliHz != m.rateMilliHz ||
        next.frameLock != m.frameLock || next.board.ip != m.board.ip ||
        next.board.clockGen != m.board.clockGen || next.revertSecondsLeft != m.revertSecondsLeft ||
        next.safeMode != m.safeMode || next.levelCalibration != m.levelCalibration ||
        next.profile != m.profile || next.profileModified != m.profileModified ||
        strcmp(next.profileName, m.profileName) != 0;
    next.seq = changed ? m.seq + 1 : m.seq;
    m = next;
}

InputSignature settledSource(const video::EngineStatus& e) {
    InputSignature s;
    // 10 ticks (200 ms) of a stable running program: the same bar "calibrate levels" waits for.
    s.valid = e.state == video::EngineState::Running && e.stableCounter >= 10 &&
              e.noSyncCounter == 0 && e.sync != video::SyncType::None &&
              e.standard != Standard::None && e.sourceLines != 0;
    if (s.valid) {
        s.port = static_cast<uint8_t>(e.input);
        s.sync = static_cast<uint8_t>(e.sync);
        s.standard = static_cast<uint8_t>(e.standard);
        s.lines = static_cast<uint16_t>(e.sourceLines + 1); // as the status reports them
    }
    return s;
}

InputSignature detectedSource(const video::DetectedSource& d) {
    InputSignature s;
    s.valid = d.sync != video::SyncType::None && d.standard != Standard::None && d.lines != 0;
    if (s.valid) {
        s.port = static_cast<uint8_t>(d.port);
        s.sync = static_cast<uint8_t>(d.sync);
        s.standard = static_cast<uint8_t>(d.standard);
        s.lines = static_cast<uint16_t>(d.lines + 1); // as settledSource counts them
    }
    return s;
}

size_t statusJson(const StatusModel& m, const char* version, const char* schema, char* buf,
                  size_t cap) {
    static const char* const kPort[] = {"component", "rgb", "none"};
    static const char* const kSync[] = {"none", "hv", "csync"};
    static const char* const kLock[] = {"locked", "inactive", "off"};
    const bool source = m.sync != video::SyncType::None;
    char quality[8] = "null";
    if (m.syncQuality <= 100) {
        snprintf(quality, sizeof quality, "%u", m.syncQuality);
    }
    char revert[40] = "";
    if (m.revertSecondsLeft != 0) {
        snprintf_P(revert, sizeof revert, PSTR(",\"revert\":{\"secondsLeft\":%u}"),
                   m.revertSecondsLeft);
    }
    // "null", or {"slot":16,"name":"<23 bytes, each escaped to at most 6>","modified":false}
    char profile[200] = "null";
    if (m.profile != 0) {
        char name[kProfileNameCap * 6];
        jsonEscape(m.profileName, name, sizeof name);
        snprintf_P(profile, sizeof profile, PSTR("{\"slot\":%u,\"name\":\"%s\",\"modified\":%s}"),
                   m.profile, name, m.profileModified ? "true" : "false");
    }
    // The format lives in flash: it is the largest string of the firmware.
    const int n = snprintf_P(
        buf, cap,
        PSTR("{\"seq\":%lu,\"headline\":\"%s\",\"safeMode\":%s,"
             "\"source\":{\"port\":\"%s\",\"sync\":\"%s\",\"standard\":\"%s\",\"lines\":%u,"
             "\"interlaced\":%s,\"hFreqKHz\":%lu.%03lu,\"vFreqHz\":%lu.%03lu,"
             "\"sog\":%u,\"phase\":%u,\"syncQuality\":%s},"
             "\"output\":{\"mode\":\"%s\",\"pixelClockMHz\":%lu.%03lu,\"rateHz\":%lu.%03lu,"
             "\"frameLock\":\"%s\"},"
             "\"board\":{\"version\":\"%s\",\"schema\":\"%s\",\"uptimeS\":%lu,\"heapFree\":%lu,"
             "\"heapMin\":%lu,\"rssi\":%d,\"ip\":\"%u.%u.%u.%u\",\"scaler\":%s,\"clockGen\":%s},"
             "\"profile\":%s%s}"),
        static_cast<unsigned long>(m.seq), headlineCode(m.headline), m.safeMode ? "true" : "false",
        source ? kPort[static_cast<uint8_t>(m.port) % 3] : "none",
        kSync[static_cast<uint8_t>(m.sync) % 3], standardText(m.standard, m.interlaced), m.lines,
        m.interlaced ? "true" : "false", static_cast<unsigned long>(m.hHz / 1000),
        static_cast<unsigned long>(m.hHz % 1000), static_cast<unsigned long>(m.vMilliHz / 1000),
        static_cast<unsigned long>(m.vMilliHz % 1000), m.sogLevel, m.phase, quality,
        outputModeId(m.mode), static_cast<unsigned long>(m.pixelClockHz / 1'000'000),
        static_cast<unsigned long>(m.pixelClockHz / 1000 % 1000),
        static_cast<unsigned long>(m.rateMilliHz / 1000),
        static_cast<unsigned long>(m.rateMilliHz % 1000),
        kLock[static_cast<uint8_t>(m.frameLock) % 3], version, schema,
        static_cast<unsigned long>(m.board.uptimeS), static_cast<unsigned long>(m.board.heapFree),
        static_cast<unsigned long>(m.board.heapMin), m.board.rssi, m.board.ip & 0xFF,
        m.board.ip >> 8 & 0xFF, m.board.ip >> 16 & 0xFF, m.board.ip >> 24,
        m.scaler ? "true" : "false", m.board.clockGen ? "true" : "false", profile, revert);
    return n > 0 && static_cast<size_t>(n) < cap ? static_cast<size_t>(n) : 0;
}

} // namespace gbsr::core
