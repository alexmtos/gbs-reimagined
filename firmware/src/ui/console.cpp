#include "ui/console.h"

#include <Arduino.h>

#include "chip/field_table.h"
#include "core/settings_store.h"

namespace gbsr::ui {

using core::Controller;
using core::Origin;

namespace {

const char* standardName(video::Standard s) {
    switch (s) {
    case video::Standard::Ntsc:
        return "NTSC 15k (240p/480i)";
    case video::Standard::Pal:
        return "PAL 15k (288p/576i)";
    case video::Standard::Edtv60:
        return "480p";
    case video::Standard::Edtv50:
        return "576p";
    case video::Standard::Hd720p:
        return "720p";
    case video::Standard::Hd1080i:
        return "1080i";
    case video::Standard::Hd1080p:
        return "1080p";
    case video::Standard::MedRes:
        return "25 kHz";
    case video::Standard::Unknown:
        return "unknown";
    case video::Standard::PcOnYpbpr:
        return "PC on YPbPr";
    case video::Standard::RgbhvScaled:
        return "RGBHV scaled";
    case video::Standard::RgbhvBypass:
        return "RGBHV";
    default:
        return "none";
    }
}

const char* stateName(video::EngineState s) {
    switch (s) {
    case video::EngineState::NoPower:
        return "scaler not answering";
    case video::EngineState::Searching:
        return "searching";
    case video::EngineState::Loading:
        return "loading";
    case video::EngineState::Settling:
        return "settling";
    case video::EngineState::Running:
        return "running";
    case video::EngineState::Bypass:
        return "bypass";
    case video::EngineState::Starting:
        return "calibrating ADC";
    }
    return "?";
}

// Splits off the first word of `s` (in place); returns the rest, or an empty string.
char* nextWord(char*& s) {
    while (*s == ' ') {
        ++s;
    }
    char* word = s;
    while (*s != '\0' && *s != ' ') {
        ++s;
    }
    if (*s != '\0') {
        *s++ = '\0';
    }
    return word;
}

} // namespace

void Console::poll() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            if (len_ > 0) {
                line_[len_] = '\0';
                handleLine(line_);
                len_ = 0;
            }
        } else if (len_ < sizeof line_ - 1) {
            line_[len_++] = c;
        }
    }
    const bool pending = ctl_.revertPending();
    if (revertWasPending_ && !pending) {
        Serial.println(F("revert window closed"));
    }
    revertWasPending_ = pending;
}

void Console::handleLine(char* line) {
    char* rest = line;
    char* cmd = nextWord(rest);
    if (strcasecmp(cmd, "factory-reset") != 0) {
        factoryResetAsked_ = false;
    }
    if (strcasecmp(cmd, "status") == 0) {
        printStatus();
    } else if (strcasecmp(cmd, "get") == 0) {
        cmdGet(rest);
    } else if (strcasecmp(cmd, "set") == 0) {
        cmdSet(rest);
    } else if (strcasecmp(cmd, "list") == 0) {
        cmdList(nextWord(rest));
    } else if (strcasecmp(cmd, "output") == 0 || strcasecmp(cmd, "input") == 0) {
        // Shortcuts for output.resolution and input.port; "output 1080p" spells r1080p.
        char buf[48];
        const char* value = nextWord(rest);
        const bool output = strcasecmp(cmd, "output") == 0;
        snprintf(buf, sizeof buf, "%s %s%s", output ? "output.resolution" : "input.port",
                 output && isdigit(static_cast<unsigned char>(value[0])) ? "r" : "", value);
        cmdSet(buf);
    } else if (strcasecmp(cmd, "keep") == 0) {
        ctl_.keep();
        Serial.println(F("kept"));
    } else if (strcasecmp(cmd, "revert") == 0) {
        Serial.println(ctl_.revertPending() ? F("reverting") : F("nothing to revert"));
        ctl_.revert();
    } else if (strcasecmp(cmd, "wifi") == 0) {
        cmdWifi(rest);
    } else if (strcasecmp(cmd, "calibrate") == 0) {
        Serial.println(ctl_.calibrateLevels()
                           ? F("calibrating levels: keep a white screen showing for a few seconds")
                           : F("no stable picture to calibrate against"));
    } else if (strcasecmp(cmd, "reboot") == 0) {
        Serial.println(F("rebooting"));
        Serial.flush();
        ESP.restart();
    } else if (strcasecmp(cmd, "factory-reset") == 0) {
        if (!factoryResetAsked_) {
            factoryResetAsked_ = true;
            Serial.println(F("all settings and the level calibration go back to defaults; type "
                             "factory-reset again to confirm"));
        } else {
            factoryResetAsked_ = false;
            ctl_.factoryReset(millis());
            Serial.println(F("factory settings restored"));
        }
    } else if (strcasecmp(cmd, "expert") == 0) {
        expert_ = strcasecmp(nextWord(rest), "on") == 0;
        Serial.println(expert_ ? F("expert tools on: reg, dump, levels") : F("expert tools off"));
    } else if (expert_ && strcasecmp(cmd, "reg") == 0) {
        cmdReg(rest);
    } else if (expert_ && strcasecmp(cmd, "dump") == 0) {
        cmdDump(rest);
    } else if (expert_ && strcasecmp(cmd, "levels") == 0) {
        cmdLevels(rest);
    } else if (strcasecmp(cmd, "help") == 0 || cmd[0] != '\0') {
        help();
    }
}

void Console::help() {
    Serial.println(F("status                  headline and figures"));
    Serial.println(F("get <setting>           e.g. get output.resolution"));
    Serial.println(F("set <setting> <value>   e.g. set picture.sharpness medium"));
    Serial.println(F("list [prefix]           settings with current values"));
    Serial.println(F("output <mode>           auto 1080p 1024p 960p 720p 480p 15khz"));
    Serial.println(F("input <port>            auto rgb component"));
    Serial.println(F("keep | revert           answer a pending revert"));
    Serial.println(F("calibrate               set input levels from a white screen"));
    Serial.println(F("wifi [<ssid> [password]] show the network, or join one"));
    Serial.println(F("reboot | factory-reset"));
    Serial.println(F("expert on|off           reg <FIELD> [value], dump <seg>, levels r|g|b [n]"));
}

void Console::cmdWifi(char* args) {
    const char* ssid = nextWord(args);
    if (strcasecmp(ssid, "scan") == 0 && args[0] == '\0') {
        // Diagnostic: how the saved network is heard from here (blocks ~2 s).
        const int n = WiFi.scanNetworks(false, true);
        int seen = 0;
        for (int i = 0; i < n; ++i) {
            if (WiFi.SSID(i) == wifi_.ssid()) {
                ++seen;
                Serial.printf_P(PSTR("saved network: %d dBm, channel %d, encryption %u\r\n"),
                                WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i));
            }
        }
        Serial.printf_P(PSTR("%d networks heard, saved one %s\r\n"), n,
                        seen ? "among them" : "not heard");
        WiFi.scanDelete();
        return;
    }
    if (ssid[0] != '\0') {
        const char* password = nextWord(args);
        Serial.println(wifi_.join(ssid, password, millis()) ? F("joining") : F("invalid network"));
        return;
    }
    static const char* const kMode[] = {"joining", "joined", "setup"};
    const uint32_t ip = wifi_.stationIp();
    Serial.printf_P(PSTR("wifi %s, network \"%s\", address %u.%u.%u.%u (gbs.local), rssi %d\r\n"),
                    kMode[static_cast<uint8_t>(wifi_.mode())], wifi_.ssid(), ip & 0xFF,
                    ip >> 8 & 0xFF, ip >> 16 & 0xFF, ip >> 24, wifi_.rssi());
    static const char* const kFailure[] = {"", "the network refused the password",
                                           "the network was not found",
                                           "the network did not answer"};
    if (wifi_.lastFailure() != net::Wifi::Failure::None) {
        Serial.printf_P(PSTR("last join failed: %s\r\n"),
                        kFailure[static_cast<uint8_t>(wifi_.lastFailure())]);
    }
    if (wifi_.apActive()) {
        Serial.printf_P(PSTR("setup access point \"%s\", password \"%s\", http://192.168.4.1\r\n"),
                        wifi_.apSsid(), net::Wifi::kApPassword);
    }
}

void Console::cmdGet(char* args) {
    const char* name = nextWord(args);
    settings::Id id{};
    if (!settings::findId(name, id)) {
        Serial.println(F("unknown setting (list shows them)"));
        return;
    }
    char value[24];
    settings::formatValue(id, ctl_.get(id), value, sizeof value);
    Serial.printf_P(PSTR("%s = %s\r\n"), name, value);
}

void Console::cmdSet(char* args) {
    const char* name = nextWord(args);
    const char* text = nextWord(args);
    settings::Id id{};
    if (!settings::findId(name, id)) {
        Serial.println(F("unknown setting (list shows them)"));
        return;
    }
    int16_t value = 0;
    if (!settings::parseValue(id, text, value)) {
        Serial.println(F("invalid value (list shows the current ones)"));
        return;
    }
    reportSet(name, ctl_.set(id, value, Origin::Console, millis()));
}

void Console::reportSet(const char* name, Controller::Outcome outcome) {
    switch (outcome) {
    case Controller::Outcome::Applied:
        Serial.println(F("ok"));
        break;
    case Controller::Outcome::Unchanged:
        Serial.println(F("unchanged"));
        break;
    case Controller::Outcome::Invalid:
        Serial.println(F("invalid value"));
        break;
    case Controller::Outcome::Unavailable:
        Serial.printf_P(PSTR("%s arrives in a later release\r\n"), name);
        break;
    case Controller::Outcome::RevertArmed:
        Serial.printf_P(PSTR("ok; reverts in %lu s unless you type keep\r\n"),
                        static_cast<unsigned long>(ctl_.revertRemainingMs(millis()) / 1000));
        break;
    }
}

void Console::cmdList(const char* prefix) {
    const size_t plen = strlen(prefix);
    char name[32];
    char value[24];
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        const settings::Id id = static_cast<settings::Id>(i);
        if (!settings::available(id)) {
            continue;
        }
        settings::idName(id, name, sizeof name);
        if (plen != 0 && strncasecmp(name, prefix, plen) != 0) {
            continue;
        }
        settings::formatValue(id, ctl_.get(id), value, sizeof value);
        Serial.printf_P(PSTR("%-24s %s\r\n"), name, value);
    }
}

void Console::printStatus() {
    const video::EngineStatus& st = engine_.status();
    char name[24] = "-";
    if (st.program != tv5725::ProgramId::Count) {
        const tv5725::ProgramInfo info = tv5725::programInfo(st.program);
        strncpy_P(name, info.name, sizeof name - 1);
        name[sizeof name - 1] = '\0';
    }
    Serial.printf_P(PSTR("state %s | %s on %s, %s sync%s%s\r\n"), stateName(st.state),
                    standardName(st.standard), st.input == video::InputPort::Rgb ? "RGB" : "YPbPr",
                    st.sync == video::SyncType::Csync ? "composite" : "HV",
                    st.deinterlacing ? ", deinterlacing" : "",
                    ctl_.safeMode() ? " | SAFE MODE" : "");
    Serial.printf_P(PSTR("source lines %u, HPERIOD_IF %u, VPERIOD_IF %u%s\r\n"), st.sourceLines,
                    st.hPeriodIf, st.vPeriodIf, st.interlaced ? " (interlaced)" : "");
    Serial.printf_P(PSTR("program %s, loads %u, stable %u, no-sync %u\r\n"), name, st.loads,
                    st.stableCounter, st.noSyncCounter);
    Serial.printf_P(PSTR("clock %lu Hz %s%s, source %lu mHz, output %lu mHz\r\n"),
                    static_cast<unsigned long>(st.pixelClockHz),
                    st.clockAdopted ? "(external)" : "(internal)", st.frameLocked ? " locked" : "",
                    static_cast<unsigned long>(st.sourceMilliHz),
                    static_cast<unsigned long>(st.outputMilliHz));
    static const char* const kLevelOutcome[] = {"not run",   "running", "done",
                                                "not white", "too hot", "aborted"};
    Serial.printf_P(PSTR("ADC offset R %02X G %02X B %02X%s | gain R %02X G %02X B %02X, auto gain "
                         "%s, levels %s\r\n"),
                    st.adcOffset[0], st.adcOffset[1], st.adcOffset[2],
                    st.offsetCalibrationFailed ? " (calibration failed)" : "", st.adcGain[0],
                    st.adcGain[1], st.adcGain[2], st.autoGain ? "on" : "off",
                    st.levelCalibration < 6 ? kLevelOutcome[st.levelCalibration] : "?");
    Serial.printf_P(PSTR("SOG %u, phase SP %u ADC %u | heap %u, longest tick %lu ms\r\n"),
                    st.sogLevel, st.phaseSp, st.phaseAdc, ESP.getFreeHeap(),
                    static_cast<unsigned long>(loop_.longestTickMs));
    if (ctl_.revertPending()) {
        Serial.printf_P(PSTR("revert pending: %lu s left (keep | revert)\r\n"),
                        static_cast<unsigned long>(ctl_.revertRemainingMs(millis()) / 1000));
    }
}

void Console::cmdReg(char* args) {
    const char* name = nextWord(args);
    const char* value = nextWord(args);
    tv5725::FieldInfo f{};
    if (!tv5725::findField(name, f)) {
        Serial.println(F("usage: reg <FIELD> [value]  (unknown field)"));
        return;
    }
    if (value[0] != '\0') {
        if (!(f.flags & tv5725::kFieldWritable)) {
            Serial.println(F("read-only field"));
            return;
        }
        chip_.writeField(f.seg, f.addr, f.lsb, f.width, strtoul(value, nullptr, 0));
    }
    uint32_t v = 0;
    chip_.readField(f.seg, f.addr, f.lsb, f.width, f.flags & tv5725::kFieldSigned, v);
    Serial.printf_P(PSTR("%s = %lu (0x%lX)\r\n"), name, static_cast<unsigned long>(v),
                    static_cast<unsigned long>(v));
}

void Console::cmdDump(const char* args) {
    const long seg = strtol(args, nullptr, 0);
    if (seg < 0 || seg > 5) {
        Serial.println(F("usage: dump <segment 0-5>"));
        return;
    }
    uint8_t row[16];
    for (uint16_t addr = 0; addr < 0x100; addr += 16) {
        chip_.readBytes(static_cast<uint8_t>(seg), static_cast<uint8_t>(addr), row, 16);
        Serial.printf_P(PSTR("S%ld_%02X:"), seg, addr);
        for (uint8_t b : row) {
            Serial.printf_P(PSTR(" %02X"), b);
        }
        Serial.println();
    }
}

void Console::cmdLevels(char* args) {
    const char* which = nextWord(args);
    const char* count = nextWord(args);
    video::AdcChannel ch = video::AdcChannel::G;
    if (strcasecmp(which, "r") == 0) {
        ch = video::AdcChannel::R;
    } else if (strcasecmp(which, "b") == 0) {
        ch = video::AdcChannel::B;
    } else if (strcasecmp(which, "g") != 0) {
        Serial.println(F("usage: levels r|g|b [samples]"));
        return;
    }
    unsigned long n = count[0] != '\0' ? strtoul(count, nullptr, 0) : 512;
    n = n > 4000 ? 4000 : n;
    video::LevelStats stats; // 258 bytes of stack, only while the command runs
    video::sampleLevels(engine_.ops(), ch, static_cast<uint16_t>(n), stats);
    Serial.printf_P(PSTR("top %02X, at ceiling %u permille | level:count"), stats.top(20),
                    stats.share(video::kLevelCeiling, video::kLevelCeiling));
    for (uint8_t v = 0; v <= video::kLevelCeiling; ++v) {
        if (stats.hist[v] != 0) {
            Serial.printf_P(PSTR(" %02X:%u"), v, stats.hist[v]);
        }
    }
    Serial.println();
}

} // namespace gbsr::ui
