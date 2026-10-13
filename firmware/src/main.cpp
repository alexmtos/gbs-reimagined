// GBS Reimagined — composition root: constructs the hardware drivers and the video engine and runs
// the main loop. Front ends (web, OLED, full console) are added in later M1 steps.

#include <Arduino.h>

#include "chip/field_table.h"
#include "chip/fields.gen.h"
#include "chip/tv5725.h"
#include "core/settings.gen.h"
#include "hal/board.h"
#include "hal/esp_i2c_bus.h"
#include "hal/pulse_timer.h"
#include "hal/si5351.h"
#include "version.h"
#include "video/engine.h"

namespace {

using namespace gbsr;

hal::EspI2cBus bus;
tv5725::Chip chip(bus);
hal::Si5351 clockGen(bus, hal::board().clockGenXtalHz);
hal::EspPulseTimer pulseTimer(hal::board().pinVsyncProbe);
video::Engine engine(chip, &clockGen, &pulseTimer);

bool clockGenPresent = false;
uint32_t lastTickMs = 0;
uint32_t longestTickMs = 0;

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

void printStatus() {
    const video::EngineStatus& st = engine.status();
    char name[24] = "-";
    if (st.program != tv5725::ProgramId::Count) {
        const tv5725::ProgramInfo info = tv5725::programInfo(st.program);
        strncpy_P(name, info.name, sizeof name - 1);
        name[sizeof name - 1] = '\0';
    }
    Serial.printf_P(PSTR("state %s | %s on %s, %s sync%s\r\n"), stateName(st.state),
                    standardName(st.standard), st.input == video::InputPort::Rgb ? "RGB" : "YPbPr",
                    st.sync == video::SyncType::Csync ? "composite" : "HV",
                    st.deinterlacing ? ", deinterlacing" : "");
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
                    static_cast<unsigned long>(longestTickMs));
}

void cmdReg(char* args) {
    char* name = strtok(args, " ");
    char* value = strtok(nullptr, " ");
    tv5725::FieldInfo f{};
    if (name == nullptr || !tv5725::findField(name, f)) {
        Serial.println(F("usage: reg <FIELD> [value]  (unknown field)"));
        return;
    }
    if (value != nullptr) {
        if (!(f.flags & tv5725::kFieldWritable)) {
            Serial.println(F("read-only field"));
            return;
        }
        chip.writeField(f.seg, f.addr, f.lsb, f.width, strtoul(value, nullptr, 0));
    }
    uint32_t v = 0;
    chip.readField(f.seg, f.addr, f.lsb, f.width, f.flags & tv5725::kFieldSigned, v);
    Serial.printf_P(PSTR("%s = %lu (0x%lX)\r\n"), name, static_cast<unsigned long>(v),
                    static_cast<unsigned long>(v));
}

void cmdDump(char* args) {
    const long seg = strtol(args, nullptr, 0);
    if (seg < 0 || seg > 5) {
        Serial.println(F("usage: dump <segment 0-5>"));
        return;
    }
    uint8_t row[16];
    for (uint16_t addr = 0; addr < 0x100; addr += 16) {
        chip.readBytes(static_cast<uint8_t>(seg), static_cast<uint8_t>(addr), row, 16);
        Serial.printf_P(PSTR("S%ld_%02X:"), seg, addr);
        for (uint8_t b : row)
            Serial.printf_P(PSTR(" %02X"), b);
        Serial.println();
    }
}

void cmdOutput(const char* arg) {
    static const struct {
        const char* name;
        video::OutputMode mode;
    } kModes[] = {
        {"auto", video::OutputMode::Auto},    {"1080p", video::OutputMode::R1080p},
        {"1024p", video::OutputMode::R1024p}, {"960p", video::OutputMode::R960p},
        {"720p", video::OutputMode::R720p},   {"480p", video::OutputMode::R480p},
        {"15khz", video::OutputMode::R15kHz},
    };
    for (const auto& m : kModes) {
        if (strcasecmp(arg, m.name) == 0) {
            engine.setOutputPreference(m.mode);
            Serial.printf_P(PSTR("output %s requested\r\n"), m.name);
            return;
        }
    }
    Serial.println(F("usage: output auto|1080p|1024p|960p|720p|480p|15khz"));
}

void cmdInput(const char* arg) {
    if (strcasecmp(arg, "auto") == 0) {
        engine.setInputPreference(video::InputPreference::Auto);
    } else if (strcasecmp(arg, "rgb") == 0) {
        engine.setInputPreference(video::InputPreference::Rgb);
    } else if (strcasecmp(arg, "component") == 0) {
        engine.setInputPreference(video::InputPreference::Component);
    } else {
        Serial.println(F("usage: input auto|rgb|component"));
        return;
    }
    Serial.println(F("ok"));
}

void cmdLevels(char* args) {
    char* which = strtok(args, " ");
    char* count = strtok(nullptr, " ");
    video::AdcChannel ch = video::AdcChannel::G;
    if (which != nullptr && strcasecmp(which, "r") == 0) {
        ch = video::AdcChannel::R;
    } else if (which != nullptr && strcasecmp(which, "b") == 0) {
        ch = video::AdcChannel::B;
    } else if (which == nullptr || strcasecmp(which, "g") != 0) {
        Serial.println(F("usage: levels r|g|b [samples]"));
        return;
    }
    unsigned long n = count != nullptr ? strtoul(count, nullptr, 0) : 512;
    n = n > 4000 ? 4000 : n;
    video::LevelStats stats; // 258 bytes of stack, only while the command runs
    video::sampleLevels(engine.ops(), ch, static_cast<uint16_t>(n), stats);
    Serial.printf_P(PSTR("top %02X, at ceiling %u permille | level:count"), stats.top(20),
                    stats.share(video::kLevelCeiling, video::kLevelCeiling));
    for (uint8_t v = 0; v <= video::kLevelCeiling; ++v) {
        if (stats.hist[v] != 0) {
            Serial.printf_P(PSTR(" %02X:%u"), v, stats.hist[v]);
        }
    }
    Serial.println();
}

void cmdAutoGain(const char* arg) {
    if (strcasecmp(arg, "on") == 0 || strcasecmp(arg, "off") == 0) {
        engine.setAutoGain(strcasecmp(arg, "on") == 0);
        Serial.println(F("ok"));
    } else {
        Serial.println(F("usage: autogain on|off"));
    }
}

void handleLine(char* line) {
    char* rest = strchr(line, ' ');
    if (rest != nullptr) {
        *rest++ = '\0';
    } else {
        rest = line + strlen(line);
    }
    if (strcasecmp(line, "status") == 0) {
        printStatus();
    } else if (strcasecmp(line, "output") == 0) {
        cmdOutput(rest);
    } else if (strcasecmp(line, "input") == 0) {
        cmdInput(rest);
    } else if (strcasecmp(line, "reload") == 0) {
        engine.requestReload();
    } else if (strcasecmp(line, "reg") == 0) {
        cmdReg(rest);
    } else if (strcasecmp(line, "dump") == 0) {
        cmdDump(rest);
    } else if (strcasecmp(line, "levels") == 0) {
        cmdLevels(rest);
    } else if (strcasecmp(line, "calibrate") == 0) {
        if (engine.calibrateLevels()) {
            Serial.println(F("calibrating levels: keep a white screen showing for a few seconds"));
        } else {
            Serial.println(F("no stable picture to calibrate against"));
        }
    } else if (strcasecmp(line, "autogain") == 0) {
        cmdAutoGain(rest);
    } else if (strcasecmp(line, "reboot") == 0) {
        ESP.restart();
    } else if (line[0] != '\0') {
        Serial.println(
            F("commands: status, output <mode>, input <port>, reload, reg <FIELD> "
              "[value], dump <seg>, levels r|g|b [n], calibrate, autogain on|off, reboot"));
    }
}

char lineBuf[96];
uint8_t lineLen = 0;

void pollConsole() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            if (lineLen > 0) {
                lineBuf[lineLen] = '\0';
                handleLine(lineBuf);
                lineLen = 0;
            }
        } else if (lineLen < sizeof lineBuf - 1) {
            lineBuf[lineLen++] = c;
        }
    }
}

} // namespace

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.printf_P(PSTR("GBS Reimagined %s on %s, settings schema %s\r\n"), GBSR_VERSION,
                    hal::board().name, settings::kSchemaHash);
    const auto& b = hal::board();
    if (!bus.begin(b.pinSda, b.pinScl, b.i2cClockHz)) {
        Serial.println(F("I2C bus stuck after recovery"));
    }
    pulseTimer.begin();
    // Clock input off while the generator initialises (protects pin 40 from start-up spikes).
    if (chip.probe()) {
        chip.write(tv5725::fields::PAD_CKIN_ENZ, 1);
    }
    clockGenPresent = b.hasClockGen && clockGen.probe() && clockGen.begin();
    Serial.printf_P(PSTR("clock generator: %s\r\n"), clockGenPresent ? "Si5351" : "absent");
    engine.begin(clockGenPresent);
    printStatus();
}

void loop() {
    pollConsole();
    const uint32_t now = millis();
    if (now - lastTickMs >= 20) {
        lastTickMs = now;
        engine.tick();
        const uint32_t took = millis() - now;
        if (took > longestTickMs) {
            longestTickMs = took;
        }
    }
}
