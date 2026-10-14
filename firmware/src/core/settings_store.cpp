#include "core/settings_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/record.h"

namespace gbsr::settings {

namespace {

constexpr char kPath[] = "/settings.bin";
constexpr uint32_t kMagic = 0x53524247; // "GBRS"
constexpr uint8_t kVersion = 1;
constexpr size_t kEntry = 6; // hash (4) + value (2), little-endian

// Copies the `index`-th NUL-separated string of a PROGMEM list starting at `offset`.
bool listItem(const char* list, uint16_t offset, int16_t index, char* out, size_t cap) {
    const char* p = list + offset;
    for (int16_t i = 0; i < index; ++i) {
        const size_t len = strlen_P(p);
        if (len == 0) {
            return false;
        }
        p += len + 1;
    }
    if (strlen_P(p) == 0) {
        return false;
    }
    strncpy_P(out, p, cap - 1);
    out[cap - 1] = '\0';
    return true;
}

bool equalsIgnoreCase(const char* a, const char* b) {
    return strcasecmp(a, b) == 0;
}

} // namespace

Def def(Id id) {
    Def d{};
    memcpy_P(&d, &kDefs[static_cast<uint8_t>(id)], sizeof d);
    return d;
}

bool available(Id id) {
    return def(id).milestone() <= kMilestone;
}

void idName(Id id, char* out, size_t cap) {
    strncpy_P(out, kIdNames + def(id).idOffset, cap - 1);
    out[cap - 1] = '\0';
}

bool findId(const char* name, Id& out) {
    char buf[32];
    for (uint8_t i = 0; i < kCount; ++i) {
        idName(static_cast<Id>(i), buf, sizeof buf);
        if (equalsIgnoreCase(buf, name)) {
            out = static_cast<Id>(i);
            return true;
        }
    }
    return false;
}

void formatValue(Id id, int16_t value, char* out, size_t cap) {
    const Def d = def(id);
    if (d.type == Type::Bool) {
        snprintf(out, cap, "%s", value ? "on" : "off");
        return;
    }
    if (d.type == Type::Enum && listItem(kValueIds, d.valuesOffset, value, out, cap)) {
        return;
    }
    for (int16_t s : d.special) {
        if (s != kNoSpecial && s == value) {
            snprintf(out, cap, "auto"); // the only special name in the schema so far
            return;
        }
    }
    snprintf(out, cap, "%d", value);
}

bool parseValue(Id id, const char* text, int16_t& out) {
    const Def d = def(id);
    if (text == nullptr || *text == '\0') {
        return false;
    }
    if (d.type == Type::Bool) {
        if (equalsIgnoreCase(text, "on") || equalsIgnoreCase(text, "true") ||
            strcmp(text, "1") == 0) {
            out = 1;
            return true;
        }
        if (equalsIgnoreCase(text, "off") || equalsIgnoreCase(text, "false") ||
            strcmp(text, "0") == 0) {
            out = 0;
            return true;
        }
        return false;
    }
    if (d.type == Type::Enum) {
        char item[24];
        for (int16_t i = 0; i <= d.max; ++i) {
            if (listItem(kValueIds, d.valuesOffset, i, item, sizeof item) &&
                equalsIgnoreCase(item, text)) {
                out = i;
                return true;
            }
        }
    }
    if (equalsIgnoreCase(text, "auto")) {
        for (int16_t s : d.special) {
            if (s != kNoSpecial) {
                out = s;
                return true;
            }
        }
        return false;
    }
    char* end = nullptr;
    const long v = strtol(text, &end, 10);
    if (end == text || *end != '\0' || v < d.min || v > d.max) {
        return false;
    }
    out = static_cast<int16_t>(v);
    return true;
}

Store::Store(platform::FileStore& fs) : fs_(fs) {
    for (uint8_t i = 0; i < kCount; ++i) {
        values_[i] = def(static_cast<Id>(i)).def;
    }
}

void Store::resetToDefaults() {
    for (uint8_t i = 0; i < kCount; ++i) {
        values_[i] = def(static_cast<Id>(i)).def;
    }
    dirty_ = true;
    ++sequence_;
}

bool Store::load() {
    uint8_t buf[kCount * kEntry + 16 * kEntry]; // room for settings this build no longer has
    const int n = core::loadRecord(fs_, kPath, kMagic, kVersion, buf, sizeof buf);
    if (n < 0 || n % kEntry != 0) {
        return false;
    }
    for (int off = 0; off < n; off += kEntry) {
        const uint32_t hash = static_cast<uint32_t>(buf[off]) | buf[off + 1] << 8 |
                              static_cast<uint32_t>(buf[off + 2]) << 16 |
                              static_cast<uint32_t>(buf[off + 3]) << 24;
        const int16_t value = static_cast<int16_t>(buf[off + 4] | buf[off + 5] << 8);
        for (uint8_t i = 0; i < kCount; ++i) {
            const Def d = def(static_cast<Id>(i));
            if (d.hash == hash && value >= d.min && value <= d.max) {
                values_[i] = value;
                break;
            }
        }
    }
    ++sequence_;
    return true;
}

Store::SetResult Store::set(Id id, int16_t value, uint32_t nowMs) {
    const Def d = def(id);
    if (value < d.min || value > d.max) {
        return SetResult::OutOfRange;
    }
    int16_t& slot = values_[static_cast<uint8_t>(id)];
    if (slot == value) {
        return SetResult::Unchanged;
    }
    slot = value;
    dirty_ = true;
    changedMs_ = nowMs;
    ++sequence_;
    return SetResult::Changed;
}

void Store::tick(uint32_t nowMs) {
    if (dirty_ && nowMs - changedMs_ >= kSaveDelayMs && !saveNow()) {
        changedMs_ = nowMs; // try again after another delay rather than on every pass
    }
}

bool Store::saveNow() {
    uint8_t buf[kCount * kEntry];
    for (uint8_t i = 0; i < kCount; ++i) {
        const uint32_t hash = def(static_cast<Id>(i)).hash;
        uint8_t* p = buf + i * kEntry;
        for (int b = 0; b < 4; ++b) {
            p[b] = static_cast<uint8_t>(hash >> (8 * b));
        }
        p[4] = static_cast<uint8_t>(values_[i]);
        p[5] = static_cast<uint8_t>(static_cast<uint16_t>(values_[i]) >> 8);
    }
    const bool ok = core::saveRecord(fs_, kPath, kMagic, kVersion, buf, sizeof buf);
    if (ok) {
        dirty_ = false;
    }
    return ok;
}

} // namespace gbsr::settings
