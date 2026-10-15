#include "core/profiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/record.h"

namespace gbsr::core {

using settings::Id;

namespace {

constexpr uint32_t kProfileMagic = 0x50524247; // "GBRP"
constexpr uint8_t kProfileVersion = 1;
constexpr uint32_t kActiveMagic = 0x41524247; // "GBRA"
constexpr uint8_t kActiveVersion = 1;
constexpr char kActivePath[] = "/profile-active.bin";
// name, signature (valid, port, sync, standard, lines), count, then (hash, value) pairs
constexpr size_t kHead = kProfileNameCap + 6 + 1;
constexpr size_t kEntry = 6;
constexpr size_t kPayloadCap = kHead + settings::kCount * kEntry;
static_assert(kPayloadCap <= kMaxRecordPayload, "a full profile must fit one record");

void path(uint8_t slot, char* out, size_t cap) {
    snprintf(out, cap, "/profile%u.bin", static_cast<unsigned>(slot));
}

void put32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        p[i] = static_cast<uint8_t>(v >> (8 * i));
    }
}

uint32_t get32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
           static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

bool validSlot(uint8_t slot) {
    return slot >= 1 && slot <= kProfileSlots;
}

} // namespace

uint8_t signatureMatch(const InputSignature& saved, const InputSignature& now) {
    if (!saved.valid || !now.valid || saved.port != now.port) {
        return 0;
    }
    if (saved.standard != now.standard) {
        return 1;
    }
    const bool linesClose = saved.lines + 2 >= now.lines && now.lines + 2 >= saved.lines;
    return saved.sync == now.sync && linesClose ? 3 : 2;
}

bool inProfile(Id id) {
    char name[40];
    settings::idName(id, name, sizeof name);
    return strncmp(name, "system.", 7) != 0;
}

bool parseSwitcherCommand(const char* line, uint8_t& slot, bool& current) {
    while (*line == ' ') {
        ++line;
    }
    const char* number = nullptr;
    if (strncasecmp(line, "SVS NEW INPUT=", 14) == 0) {
        number = line + 14;
        current = false;
    } else if (strncasecmp(line, "SVS CURRENT INPUT=", 18) == 0) {
        number = line + 18;
        current = true;
    } else if (strncasecmp(line, "remote prof", 11) == 0) {
        number = line + 11;
        current = false;
    } else {
        return false;
    }
    char* end = nullptr;
    const long n = strtol(number, &end, 10);
    while (end != number && *end == ' ') {
        ++end;
    }
    if (end == number || *end != '\0' || n < 1 || n > kProfileSlots) {
        return false;
    }
    slot = static_cast<uint8_t>(n);
    return true;
}

void ProfileStore::begin() {
    Profile p;
    for (uint8_t slot = 1; slot <= kProfileSlots; ++slot) {
        exists_[slot - 1] = read(slot, p);
        signatures_[slot - 1] = exists_[slot - 1] ? p.signature : InputSignature{};
    }
    uint8_t buf[2];
    if (loadRecord(fs_, kActivePath, kActiveMagic, kActiveVersion, buf, sizeof buf) == 2 &&
        exists(buf[0])) {
        active_ = buf[0];
        modified_ = buf[1] != 0;
        read(active_, p);
        memcpy(activeName_, p.name, sizeof activeName_);
    }
}

bool ProfileStore::exists(uint8_t slot) const {
    return validSlot(slot) && exists_[slot - 1];
}

const InputSignature& ProfileStore::signature(uint8_t slot) const {
    static const InputSignature kNone;
    return validSlot(slot) ? signatures_[slot - 1] : kNone;
}

bool ProfileStore::read(uint8_t slot, Profile& out) const {
    out = Profile{};
    if (!validSlot(slot)) {
        return false;
    }
    char file[20];
    path(slot, file, sizeof file);
    uint8_t buf[kPayloadCap];
    const int n = loadRecord(fs_, file, kProfileMagic, kProfileVersion, buf, sizeof buf);
    if (n < static_cast<int>(kHead)) {
        return false;
    }
    memcpy(out.name, buf, kProfileNameCap);
    out.name[kProfileNameCap - 1] = '\0';
    const uint8_t* s = buf + kProfileNameCap;
    out.signature.valid = s[0] != 0;
    out.signature.port = s[1];
    out.signature.sync = s[2];
    out.signature.standard = s[3];
    out.signature.lines = static_cast<uint16_t>(s[4] | s[5] << 8);
    const uint8_t count = s[6];
    const uint8_t* e = buf + kHead;
    for (uint8_t i = 0; i < count && e + kEntry <= buf + n; ++i, e += kEntry) {
        const uint32_t hash = get32(e);
        const int16_t value = static_cast<int16_t>(e[4] | e[5] << 8);
        for (uint8_t k = 0; k < settings::kCount; ++k) {
            const Id id = static_cast<Id>(k);
            const settings::Def d = settings::def(id);
            if (d.hash == hash && inProfile(id) && value >= d.min && value <= d.max) {
                out.values[k] = value;
                out.has[k] = true;
                break;
            }
        }
    }
    return true;
}

bool ProfileStore::write(uint8_t slot, const Profile& p) {
    if (!validSlot(slot)) {
        return false;
    }
    uint8_t buf[kPayloadCap] = {};
    strncpy(reinterpret_cast<char*>(buf), p.name, kProfileNameCap - 1);
    uint8_t* s = buf + kProfileNameCap;
    s[0] = p.signature.valid ? 1 : 0;
    s[1] = p.signature.port;
    s[2] = p.signature.sync;
    s[3] = p.signature.standard;
    s[4] = static_cast<uint8_t>(p.signature.lines);
    s[5] = static_cast<uint8_t>(p.signature.lines >> 8);
    uint8_t count = 0;
    uint8_t* e = buf + kHead;
    for (uint8_t k = 0; k < settings::kCount; ++k) {
        const Id id = static_cast<Id>(k);
        if (!p.has[k] || !inProfile(id)) {
            continue;
        }
        put32(e, settings::def(id).hash);
        e[4] = static_cast<uint8_t>(p.values[k]);
        e[5] = static_cast<uint8_t>(static_cast<uint16_t>(p.values[k]) >> 8);
        e += kEntry;
        ++count;
    }
    s[6] = count;
    char file[20];
    path(slot, file, sizeof file);
    if (!saveRecord(fs_, file, kProfileMagic, kProfileVersion, buf,
                    static_cast<uint16_t>(e - buf))) {
        return false;
    }
    exists_[slot - 1] = true;
    signatures_[slot - 1] = p.signature;
    if (slot == active_) {
        memcpy(activeName_, p.name, sizeof activeName_);
        activeName_[kProfileNameCap - 1] = '\0';
    }
    return true;
}

bool ProfileStore::remove(uint8_t slot) {
    if (!validSlot(slot)) {
        return false;
    }
    char file[20];
    path(slot, file, sizeof file);
    fs_.remove(file);
    exists_[slot - 1] = false;
    signatures_[slot - 1] = InputSignature{};
    if (active_ == slot) {
        setActive(0, false);
    }
    return true;
}

uint8_t ProfileStore::match(const InputSignature& now) const {
    uint8_t best = 0;
    uint8_t bestScore = 0;
    for (uint8_t slot = 1; slot <= kProfileSlots; ++slot) {
        if (!exists_[slot - 1]) {
            continue;
        }
        const uint8_t score = signatureMatch(signatures_[slot - 1], now);
        if (score > bestScore) {
            best = slot;
            bestScore = score;
        }
    }
    return best;
}

void ProfileStore::setActive(uint8_t slot, bool modified, bool persist) {
    if (!exists(slot)) {
        slot = 0;
        modified = false;
    }
    if (slot == active_ && modified == modified_) {
        return;
    }
    if (slot != active_) {
        Profile p;
        read(slot, p); // an empty name for slot 0
        memcpy(activeName_, p.name, sizeof activeName_);
    }
    active_ = slot;
    modified_ = modified;
    if (persist) {
        persistActive();
    }
}

void ProfileStore::persistActive() {
    const uint8_t buf[2] = {active_, static_cast<uint8_t>(modified_ ? 1 : 0)};
    saveRecord(fs_, kActivePath, kActiveMagic, kActiveVersion, buf, sizeof buf);
}

} // namespace gbsr::core
