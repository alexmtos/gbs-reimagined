#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/settings_store.h"
#include "platform/storage.h"

namespace gbsr::core {

// Profiles (ADR 0014): saved values of the Output, Input and Picture settings, one file per slot
// (flat names: no directories needed on LittleFS).

constexpr uint8_t kProfileSlots = 16;  // numbered 1..16
constexpr size_t kProfileNameCap = 24; // 23 bytes and the terminator

// What a settled source looks like: the port, the sync type, the standard and the lines per field.
// 240p and 480i are one standard, so they share a signature (lines differ by half a line).
struct InputSignature {
    bool valid = false;
    uint8_t port = 0;     // video::InputPort
    uint8_t sync = 0;     // video::SyncType
    uint8_t standard = 0; // video::Standard
    uint16_t lines = 0;
    bool operator==(const InputSignature&) const = default;
};

// How well a saved signature fits a source: 0 not at all, 1 the port, 2 port and standard, 3 port,
// sync type, standard and lines within two.
uint8_t signatureMatch(const InputSignature& saved, const InputSignature& now);

// Whether a setting belongs in a profile: every group but System.
bool inProfile(settings::Id id);

// The switcher lines of ADR 0014, as switchers and their bridges send them to RetroTINK scalers:
// "SVS NEW INPUT=<n>", "SVS CURRENT INPUT=<n>" (`current` true) and "remote prof<n>". Case and
// surrounding spaces do not matter. False for any other line, or a slot outside 1..16.
bool parseSwitcherCommand(const char* line, uint8_t& slot, bool& current);

struct Profile {
    char name[kProfileNameCap] = {};
    InputSignature signature; // valid: loaded automatically for matching sources
    int16_t values[settings::kCount] = {};
    bool has[settings::kCount] = {}; // false for ids the file did not carry (or unknown to it)
};

// Profiles on flash, /profile<n>.bin, plus the active slot and its modified flag in
// /profile-active.bin. An index of which slots exist and their signatures stays in RAM so auto-load
// matches without reading files.
class ProfileStore {
public:
    explicit ProfileStore(platform::FileStore& fs) : fs_(fs) {}

    void begin(); // builds the index and reads the active slot

    bool exists(uint8_t slot) const;
    const InputSignature& signature(uint8_t slot) const;
    bool read(uint8_t slot, Profile& out) const;
    bool write(uint8_t slot, const Profile& p);
    bool remove(uint8_t slot);
    // The best slot for a source among the profiles with a signature (lowest slot on a tie), 0 when
    // none matches.
    uint8_t match(const InputSignature& now) const;

    uint8_t active() const { return active_; } // 0: none
    bool modified() const { return modified_; }
    const char* activeName() const { return activeName_; } // "" when none
    // Persists when something changed, unless `persist` is false (a load a revert may still undo);
    // persistActive() writes it later.
    void setActive(uint8_t slot, bool modified, bool persist = true);
    void persistActive();

private:
    platform::FileStore& fs_;
    bool exists_[kProfileSlots] = {};
    InputSignature signatures_[kProfileSlots];
    uint8_t active_ = 0;
    bool modified_ = false;
    char activeName_[kProfileNameCap] = {};
};

} // namespace gbsr::core
