#include "core/api.h"

#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

#include "core/settings_store.h"
#include "platform/progmem.h"

namespace gbsr::core {

using settings::Id;

namespace {

// Flash text, for JsonOut::strP.
const char* outcomeText(Controller::Outcome o) {
    switch (o) {
    case Controller::Outcome::Applied:
        return PSTR("applied");
    case Controller::Outcome::Unchanged:
        return PSTR("unchanged");
    case Controller::Outcome::Invalid:
        return PSTR("invalid");
    case Controller::Outcome::Unavailable:
        return PSTR("unavailable");
    case Controller::Outcome::RevertArmed:
        return PSTR("revertArmed");
    }
    return PSTR("invalid");
}

void revertMember(JsonOut& j, const Controller& ctl, uint32_t nowMs) {
    if (ctl.revertPending()) {
        j.rawP(PSTR(",\"revert\":{\"secondsLeft\":"))
            .num(static_cast<long>((ctl.revertRemainingMs(nowMs) + 999) / 1000))
            .rawP(PSTR("}"));
    }
}

// Text ids of video::Standard, by value; the gaps are values the engine never reports.
const char kStandardIds[][12] PROGMEM = {
    "none",   "ntsc",    "pal", "edtv60", "edtv50", "hd720p",    "hd1080i",     "hd1080p",
    "medres", "unknown", "",    "",       "",       "pcOnYpbpr", "rgbhvScaled", "rgbhvBypass"};
constexpr uint8_t kStandardCount = sizeof kStandardIds / sizeof kStandardIds[0];

void sourceValue(JsonOut& j, const InputSignature& s) {
    if (!s.valid) {
        j.rawP(PSTR("null"));
        return;
    }
    j.rawP(PSTR("{\"port\":"))
        .strP(s.port == static_cast<uint8_t>(video::InputPort::Rgb) ? PSTR("rgb")
                                                                    : PSTR("component"));
    j.rawP(PSTR(",\"sync\":"))
        .strP(s.sync == static_cast<uint8_t>(video::SyncType::Csync) ? PSTR("csync") : PSTR("hv"));
    j.rawP(PSTR(",\"standard\":"))
        .strP(kStandardIds[s.standard < kStandardCount ? s.standard : 9]); // 9: unknown
    j.rawP(PSTR(",\"lines\":")).num(s.lines).rawP(PSTR("}"));
}

// null (or absent) is no signature; anything else must be a complete one.
bool parseSource(JsonVariantConst v, InputSignature& out) {
    out = InputSignature{};
    if (v.isNull()) {
        return true;
    }
    const char* port = v["port"] | "";
    const char* sync = v["sync"] | "";
    const char* standard = v["standard"] | "";
    const long lines = v["lines"] | 0L;
    if (strcmp(port, "rgb") == 0 || strcmp(port, "component") == 0) {
        out.port = static_cast<uint8_t>(strcmp(port, "rgb") == 0 ? video::InputPort::Rgb
                                                                 : video::InputPort::Component);
    } else {
        return false;
    }
    if (strcmp(sync, "csync") == 0 || strcmp(sync, "hv") == 0) {
        out.sync = static_cast<uint8_t>(strcmp(sync, "csync") == 0 ? video::SyncType::Csync
                                                                   : video::SyncType::Hv);
    } else {
        return false;
    }
    uint8_t s = 1;
    while (s < kStandardCount &&
           (pgm_read_byte(kStandardIds[s]) == '\0' || strcmp_P(standard, kStandardIds[s]) != 0)) {
        ++s;
    }
    if (s == kStandardCount || lines < 1 || lines > 4095) {
        return false;
    }
    out.standard = s;
    out.lines = static_cast<uint16_t>(lines);
    out.valid = true;
    return true;
}

// {id: text} for the values a profile carries.
void valuesObject(JsonOut& j, const Profile& p) {
    char name[32];
    char value[24];
    j.rawP(PSTR("{"));
    bool first = true;
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        if (!p.has[i]) {
            continue;
        }
        settings::idName(static_cast<Id>(i), name, sizeof name);
        settings::formatValue(static_cast<Id>(i), p.values[i], value, sizeof value);
        j.raw(first ? "" : ",").str(name).rawP(PSTR(":")).str(value);
        first = false;
    }
    j.rawP(PSTR("}"));
}

// "name", "source" and "values" of a profile, without the braces around them.
void profileMembers(JsonOut& j, const Profile& p) {
    j.rawP(PSTR("\"name\":")).str(p.name).rawP(PSTR(",\"source\":"));
    sourceValue(j, p.signature);
    j.rawP(PSTR(",\"values\":"));
    valuesObject(j, p);
}

// A value as setting text, the way POST /api/settings takes it.
void valueText(JsonVariantConst v, char* out, size_t cap) {
    if (v.is<bool>()) {
        snprintf(out, cap, "%s", v.as<bool>() ? "on" : "off");
    } else if (v.is<long>()) {
        snprintf(out, cap, "%ld", v.as<long>());
    } else if (v.is<const char*>()) {
        snprintf(out, cap, "%s", v.as<const char*>());
    } else {
        out[0] = '\0';
    }
}

// The value of a profile setting from an import, if it is one this build knows and accepts.
bool importValue(const char* key, JsonVariantConst v, Id& id, int16_t& value) {
    char text[24];
    valueText(v, text, sizeof text);
    return settings::findId(key, id) && inProfile(id) && settings::parseValue(id, text, value);
}

// {"ok":false,"reason":...} with a reason in flash.
void answer(JsonOut& j, const char* reason) {
    j.rawP(PSTR("{\"ok\":false,\"reason\":")).strP(reason).rawP(PSTR("}"));
}

// Flash text, for JsonOut::strP.
const char* loadText(Controller::ProfileResult r) {
    switch (r) {
    case Controller::ProfileResult::Loaded:
        return PSTR("loaded");
    case Controller::ProfileResult::RevertArmed:
        return PSTR("revertArmed");
    case Controller::ProfileResult::Unchanged:
        return PSTR("unchanged");
    case Controller::ProfileResult::Missing:
        return PSTR("noProfile");
    case Controller::ProfileResult::Refused:
        return PSTR("safeMode");
    }
    return PSTR("noProfile");
}

int loadAnswer(Controller& ctl, Controller::ProfileResult r, uint32_t nowMs, Sink& out) {
    const bool ok =
        r != Controller::ProfileResult::Missing && r != Controller::ProfileResult::Refused;
    const int code = ok ? 200 : (r == Controller::ProfileResult::Missing ? 404 : 409);
    out.status(code);
    JsonOut j(out);
    if (ok) {
        j.rawP(PSTR("{\"ok\":true,\"result\":")).strP(loadText(r));
        revertMember(j, ctl, nowMs);
        j.rawP(PSTR("}"));
    } else {
        answer(j, loadText(r));
    }
    return code;
}

} // namespace

// ------------------------------------------------------------------------------------------------
// JsonOut

void JsonOut::put(char c) {
    if (len_ == sizeof buf_) {
        flush();
    }
    buf_[len_++] = c;
}

void JsonOut::flush() {
    if (len_ != 0) {
        sink_.write(buf_, len_);
        len_ = 0;
    }
}

JsonOut& JsonOut::raw(const char* text) {
    while (*text != '\0') {
        put(*text++);
    }
    return *this;
}

JsonOut& JsonOut::str(const char* text) {
    put('"');
    for (; *text != '\0'; ++text) {
        const unsigned char c = static_cast<unsigned char>(*text);
        if (c == '"' || c == '\\') {
            put('\\');
            put(static_cast<char>(c));
        } else if (c < 0x20) {
            char esc[8];
            snprintf(esc, sizeof esc, "\\u%04x", c);
            raw(esc);
        } else {
            put(static_cast<char>(c));
        }
    }
    put('"');
    return *this;
}

JsonOut& JsonOut::rawP(const char* text) {
    for (char c = static_cast<char>(pgm_read_byte(text)); c != '\0';
         c = static_cast<char>(pgm_read_byte(++text))) {
        put(c);
    }
    return *this;
}

JsonOut& JsonOut::strP(const char* text) {
    // Flash strings here are ids and reasons: nothing to escape.
    put('"');
    rawP(text);
    put('"');
    return *this;
}

JsonOut& JsonOut::num(long value) {
    char digits[16];
    snprintf(digits, sizeof digits, "%ld", value);
    return raw(digits);
}

// ------------------------------------------------------------------------------------------------
// Routes

int getSettings(const Controller& ctl, uint32_t nowMs, Sink& out) {
    out.status(200);
    JsonOut j(out);
    j.rawP(PSTR("{\"schema\":")).str(settings::kSchemaHash).rawP(PSTR(",\"values\":{"));
    bool first = true;
    char name[32];
    char value[24];
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        const Id id = static_cast<Id>(i);
        if (!settings::available(id)) {
            continue;
        }
        settings::idName(id, name, sizeof name);
        settings::formatValue(id, ctl.get(id), value, sizeof value);
        j.raw(first ? "" : ",").str(name).rawP(PSTR(":")).str(value);
        first = false;
    }
    j.rawP(PSTR("}"));
    revertMember(j, ctl, nowMs);
    j.rawP(PSTR("}"));
    return 200;
}

int postSettings(Controller& ctl, const char* body, size_t len, uint32_t nowMs, Sink& out) {
    JsonDocument doc;
    if (deserializeJson(doc, body, len) != DeserializationError::Ok || !doc.is<JsonObject>()) {
        out.status(400);
        JsonOut j(out);
        j.rawP(PSTR("{\"error\":\"expected an object of setting values\"}"));
        return 400;
    }
    out.status(200);
    JsonOut j(out);
    j.rawP(PSTR("{\"results\":{"));
    bool first = true;
    for (JsonPair kv : doc.as<JsonObject>()) {
        const char* key = kv.key().c_str();
        const char* outcome = PSTR("unknown");
        Id id{};
        if (settings::findId(key, id)) {
            char text[24];
            valueText(kv.value(), text, sizeof text);
            int16_t value = 0;
            outcome = settings::parseValue(id, text, value)
                          ? outcomeText(ctl.set(id, value, Origin::Web, nowMs))
                          : PSTR("invalid");
        }
        j.raw(first ? "" : ",").str(key).rawP(PSTR(":")).strP(outcome);
        first = false;
    }
    j.rawP(PSTR("}"));
    revertMember(j, ctl, nowMs);
    j.rawP(PSTR("}"));
    return 200;
}

int postCommand(Controller& ctl, const char* body, size_t len, uint32_t nowMs, Sink& out,
                Command& ran) {
    ran = Command::None;
    JsonDocument doc;
    int code = 200;
    const char* reason = nullptr;
    if (deserializeJson(doc, body, len) != DeserializationError::Ok ||
        !doc["name"].is<const char*>()) {
        code = 400;
        reason = PSTR("expectedName");
    } else {
        const char* name = doc["name"].as<const char*>();
        if (strcmp(name, "keep") == 0) {
            ctl.keep();
            ran = Command::Keep;
        } else if (strcmp(name, "revert") == 0) {
            ctl.revert();
            ran = Command::Revert;
        } else if (strcmp(name, "calibrate") == 0) {
            if (ctl.calibrateLevels()) {
                ran = Command::Calibrate;
            } else {
                code = 409;
                reason = PSTR("noStablePicture");
            }
        } else if (strcmp(name, "reboot") == 0) {
            ran = Command::Reboot;
        } else if (strcmp(name, "factoryReset") == 0) {
            ctl.factoryReset(nowMs);
            ran = Command::FactoryReset;
        } else {
            code = 404;
            reason = PSTR("unknownCommand");
        }
    }
    out.status(code);
    JsonOut j(out);
    if (reason == nullptr) {
        j.rawP(PSTR("{\"ok\":true}"));
    } else {
        answer(j, reason);
    }
    return code;
}

int getProfiles(const Controller& ctl, Sink& out) {
    out.status(200);
    JsonOut j(out);
    j.rawP(PSTR("{\"active\":")).num(ctl.activeProfile());
    j.rawP(PSTR(",\"modified\":")).raw(ctl.profileModified() ? "true" : "false");
    j.rawP(PSTR(",\"source\":"));
    sourceValue(j, ctl.currentSource());
    j.rawP(PSTR(",\"profiles\":["));
    bool first = true;
    for (uint8_t slot = 1; slot <= kProfileSlots; ++slot) {
        Profile p;
        if (!ctl.profiles().read(slot, p)) {
            continue;
        }
        j.rawP(first ? PSTR("{\"slot\":") : PSTR(",{\"slot\":")).num(slot);
        j.rawP(PSTR(",\"name\":")).str(p.name).rawP(PSTR(",\"source\":"));
        sourceValue(j, p.signature);
        j.rawP(PSTR("}"));
        first = false;
    }
    j.rawP(PSTR("]}"));
    return 200;
}

int getProfile(const Controller& ctl, uint8_t slot, Sink& out) {
    Profile p;
    if (!ctl.profiles().read(slot, p)) {
        out.status(404);
        JsonOut j(out);
        answer(j, PSTR("noProfile"));
        return 404;
    }
    out.status(200);
    JsonOut j(out);
    j.rawP(PSTR("{\"format\":\"gbsr-profile\",\"version\":1,\"schema\":"))
        .str(settings::kSchemaHash);
    j.rawP(PSTR(","));
    profileMembers(j, p);
    j.rawP(PSTR("}"));
    return 200;
}

int postProfile(Controller& ctl, const char* body, size_t len, uint32_t nowMs, Sink& out) {
    JsonDocument doc;
    if (deserializeJson(doc, body, len) != DeserializationError::Ok ||
        !doc["action"].is<const char*>()) {
        out.status(400);
        JsonOut j(out);
        answer(j, PSTR("expectedAction"));
        return 400;
    }
    const char* action = doc["action"].as<const char*>();
    const long slotArg = doc["slot"] | 0L;
    const uint8_t slot =
        slotArg >= 1 && slotArg <= kProfileSlots ? static_cast<uint8_t>(slotArg) : 0;
    const char* name = doc["name"] | "";
    const char* reason = nullptr; // flash text
    int code = 200;
    const auto refuse = [&](const char* why, int status) {
        reason = why;
        code = status;
    };
    if (strcmp(action, "load") == 0) {
        return loadAnswer(ctl, ctl.loadProfile(slot, Controller::LoadKind::Manual, nowMs), nowMs,
                          out);
    }
    if (slot == 0) {
        refuse(PSTR("invalidSlot"), 400);
    } else if (ctl.safeMode()) {
        refuse(PSTR("safeMode"), 409);
    } else if (strcmp(action, "save") == 0) {
        const char* source = doc["source"] | "keep";
        const Controller::SaveSource mode =
            strcmp(source, "this") == 0   ? Controller::SaveSource::This
            : strcmp(source, "none") == 0 ? Controller::SaveSource::None
                                          : Controller::SaveSource::Keep;
        if (ctl.revertPending()) {
            refuse(PSTR("revertPending"), 409);
        } else if (!Controller::validProfileName(name)) {
            refuse(PSTR("invalidName"), 400);
        } else if (mode == Controller::SaveSource::This && !ctl.currentSource().valid) {
            refuse(PSTR("noSource"), 409);
        } else if (!ctl.saveProfile(slot, name, mode)) {
            refuse(PSTR("failed"), 500);
        }
    } else if (strcmp(action, "rename") == 0) {
        if (!ctl.profiles().exists(slot)) {
            refuse(PSTR("noProfile"), 404);
        } else if (name[0] == '\0' || !Controller::validProfileName(name)) {
            refuse(PSTR("invalidName"), 400);
        } else if (!ctl.renameProfile(slot, name)) {
            refuse(PSTR("failed"), 500);
        }
    } else if (strcmp(action, "source") == 0) {
        const bool on = doc["on"] | false;
        if (!ctl.profiles().exists(slot)) {
            refuse(PSTR("noProfile"), 404);
        } else if (on && !ctl.currentSource().valid) {
            refuse(PSTR("noSource"), 409);
        } else if (!ctl.setProfileSource(slot, on)) {
            refuse(PSTR("failed"), 500);
        }
    } else if (strcmp(action, "delete") == 0) {
        if (!ctl.deleteProfile(slot)) {
            refuse(PSTR("noProfile"), 404);
        }
    } else if (strcmp(action, "import") == 0) {
        JsonVariantConst in = doc["profile"];
        Profile p;
        strncpy(p.name, in["name"] | "", sizeof p.name - 1);
        const char* given = in["name"] | "";
        if (!in.is<JsonObjectConst>() || !in["values"].is<JsonObjectConst>()) {
            refuse(PSTR("expectedProfile"), 400);
        } else if (strlen(given) >= kProfileNameCap || !Controller::validProfileName(given)) {
            refuse(PSTR("invalidName"), 400);
        } else if (!parseSource(in["source"], p.signature)) {
            refuse(PSTR("invalidSource"), 400);
        } else {
            for (JsonPairConst kv : in["values"].as<JsonObjectConst>()) {
                Id id{};
                int16_t value = 0;
                if (importValue(kv.key().c_str(), kv.value(), id, value)) {
                    p.values[static_cast<uint8_t>(id)] = value;
                    p.has[static_cast<uint8_t>(id)] = true;
                }
            }
            if (!ctl.importProfile(slot, p)) {
                refuse(PSTR("failed"), 500);
            } else {
                out.status(200);
                JsonOut j(out);
                j.rawP(PSTR("{\"ok\":true,\"skipped\":["));
                bool first = true;
                for (JsonPairConst kv : in["values"].as<JsonObjectConst>()) {
                    Id id{};
                    int16_t value = 0;
                    if (!importValue(kv.key().c_str(), kv.value(), id, value)) {
                        j.raw(first ? "" : ",").str(kv.key().c_str());
                        first = false;
                    }
                }
                j.rawP(PSTR("]}"));
                return 200;
            }
        }
    } else {
        refuse(PSTR("unknownAction"), 404);
    }
    out.status(code);
    JsonOut j(out);
    if (reason != nullptr) {
        answer(j, reason);
    } else {
        j.rawP(PSTR("{\"ok\":true}"));
    }
    return code;
}

int postSwitcher(Controller& ctl, const char* body, size_t len, uint32_t nowMs, Sink& out) {
    char line[40];
    size_t n = len < sizeof line - 1 ? len : sizeof line - 1;
    memcpy(line, body, n);
    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n' || line[n - 1] == ' ')) {
        --n;
    }
    line[n] = '\0';
    uint8_t slot = 0;
    bool current = false;
    if (len >= sizeof line || !parseSwitcherCommand(line, slot, current)) {
        out.status(400);
        JsonOut j(out);
        answer(j, PSTR("expectedSwitcherLine"));
        return 400;
    }
    return loadAnswer(ctl, ctl.switcherInput(slot, current, nowMs), nowMs, out);
}

int getBackup(const Controller& ctl, const char* firmware, Sink& out) {
    out.status(200);
    JsonOut j(out);
    j.rawP(PSTR("{\"format\":\"gbsr-backup\",\"version\":1,\"firmware\":")).str(firmware);
    j.rawP(PSTR(",\"schema\":")).str(settings::kSchemaHash).rawP(PSTR(",\"settings\":{"));
    char name[32];
    char value[24];
    bool first = true;
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        const Id id = static_cast<Id>(i);
        if (!settings::available(id)) {
            continue;
        }
        settings::idName(id, name, sizeof name);
        settings::formatValue(id, ctl.get(id), value, sizeof value);
        j.raw(first ? "" : ",").str(name).rawP(PSTR(":")).str(value);
        first = false;
    }
    j.rawP(PSTR("},\"levels\":{"));
    for (uint8_t port = 0; port < 2; ++port) {
        uint8_t g[3];
        ctl.levels(port, g);
        j.rawP(port == 0 ? PSTR("\"component\":[") : PSTR(",\"rgb\":["));
        j.num(g[0]).rawP(PSTR(",")).num(g[1]).rawP(PSTR(",")).num(g[2]).rawP(PSTR("]"));
    }
    j.rawP(PSTR("},\"profiles\":{"));
    first = true;
    for (uint8_t slot = 1; slot <= kProfileSlots; ++slot) {
        Profile p;
        if (!ctl.profiles().read(slot, p)) {
            continue;
        }
        char key[4];
        snprintf(key, sizeof key, "%u", slot);
        j.raw(first ? "" : ",").str(key).rawP(PSTR(":{"));
        profileMembers(j, p);
        j.rawP(PSTR("}"));
        first = false;
    }
    j.rawP(PSTR("}}"));
    return 200;
}

int postLevels(Controller& ctl, const char* body, size_t len, Sink& out) {
    JsonDocument doc;
    uint8_t gains[2][3] = {};
    bool ok = deserializeJson(doc, body, len) == DeserializationError::Ok;
    static const char* const kPorts[] = {"component", "rgb"};
    for (uint8_t port = 0; ok && port < 2; ++port) {
        JsonArrayConst a = doc[kPorts[port]];
        ok = a.size() == 3;
        for (uint8_t c = 0; ok && c < 3; ++c) {
            const long v = a[c] | -1L;
            ok = v >= 0 && v <= 255;
            gains[port][c] = static_cast<uint8_t>(v);
        }
    }
    if (!ok) {
        out.status(400);
        JsonOut j(out);
        answer(j, PSTR("expectedLevels"));
        return 400;
    }
    if (!ctl.restoreLevels(gains[0], gains[1])) {
        out.status(409);
        JsonOut j(out);
        answer(j, PSTR("safeMode"));
        return 409;
    }
    out.status(200);
    JsonOut j(out);
    j.rawP(PSTR("{\"ok\":true}"));
    return 200;
}

} // namespace gbsr::core
