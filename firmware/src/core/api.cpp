#include "core/api.h"

#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

#include "core/settings_store.h"

namespace gbsr::core {

using settings::Id;

namespace {

const char* outcomeText(Controller::Outcome o) {
    switch (o) {
    case Controller::Outcome::Applied:
        return "applied";
    case Controller::Outcome::Unchanged:
        return "unchanged";
    case Controller::Outcome::Invalid:
        return "invalid";
    case Controller::Outcome::Unavailable:
        return "unavailable";
    case Controller::Outcome::RevertArmed:
        return "revertArmed";
    }
    return "invalid";
}

void revertMember(JsonOut& j, const Controller& ctl, uint32_t nowMs) {
    if (ctl.revertPending()) {
        j.raw(",\"revert\":{\"secondsLeft\":")
            .num(static_cast<long>((ctl.revertRemainingMs(nowMs) + 999) / 1000))
            .raw("}");
    }
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
    j.raw("{\"schema\":").str(settings::kSchemaHash).raw(",\"values\":{");
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
        j.raw(first ? "" : ",").str(name).raw(":").str(value);
        first = false;
    }
    j.raw("}");
    revertMember(j, ctl, nowMs);
    j.raw("}");
    return 200;
}

int postSettings(Controller& ctl, const char* body, size_t len, uint32_t nowMs, Sink& out) {
    JsonDocument doc;
    if (deserializeJson(doc, body, len) != DeserializationError::Ok || !doc.is<JsonObject>()) {
        out.status(400);
        JsonOut j(out);
        j.raw("{\"error\":\"expected an object of setting values\"}");
        return 400;
    }
    out.status(200);
    JsonOut j(out);
    j.raw("{\"results\":{");
    bool first = true;
    for (JsonPair kv : doc.as<JsonObject>()) {
        const char* key = kv.key().c_str();
        const char* outcome = "unknown";
        Id id{};
        if (settings::findId(key, id)) {
            char text[24];
            JsonVariant v = kv.value();
            if (v.is<bool>()) {
                snprintf(text, sizeof text, "%s", v.as<bool>() ? "on" : "off");
            } else if (v.is<long>()) {
                snprintf(text, sizeof text, "%ld", v.as<long>());
            } else if (v.is<const char*>()) {
                snprintf(text, sizeof text, "%s", v.as<const char*>());
            } else {
                text[0] = '\0';
            }
            int16_t value = 0;
            outcome = settings::parseValue(id, text, value)
                          ? outcomeText(ctl.set(id, value, Origin::Web, nowMs))
                          : "invalid";
        }
        j.raw(first ? "" : ",").str(key).raw(":").str(outcome);
        first = false;
    }
    j.raw("}");
    revertMember(j, ctl, nowMs);
    j.raw("}");
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
        reason = "expectedName";
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
                reason = "noStablePicture";
            }
        } else if (strcmp(name, "reboot") == 0) {
            ran = Command::Reboot;
        } else if (strcmp(name, "factoryReset") == 0) {
            ctl.factoryReset(nowMs);
            ran = Command::FactoryReset;
        } else {
            code = 404;
            reason = "unknownCommand";
        }
    }
    out.status(code);
    JsonOut j(out);
    if (reason == nullptr) {
        j.raw("{\"ok\":true}");
    } else {
        j.raw("{\"ok\":false,\"reason\":").str(reason).raw("}");
    }
    return code;
}

} // namespace gbsr::core
