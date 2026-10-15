#pragma once

#include <stdint.h>

#include "core/profiles.h"
#include "core/settings_store.h"
#include "platform/storage.h"
#include "video/control.h"

namespace gbsr::core {

// Where a change came from. Risky changes made where the user may not see the result (web,
// console) arm the timed revert; the OLED sits next to the display, so its changes confirm
// themselves (docs/design/ux.md, Safety).
enum class Origin : uint8_t { Console, Web, Oled, Startup };

// The one place that changes settings: it validates, applies to the video path, persists, and
// guards risky changes with a timed revert. Every front end goes through it, so the web, the
// console and the OLED can never disagree about what a change does.
class Controller {
public:
    enum class Outcome : uint8_t {
        Applied,
        Unchanged,
        Invalid,     // out of range
        Unavailable, // a setting of a later milestone
        RevertArmed, // applied; reverts unless kept in time
    };

    Controller(platform::FileStore& fs, video::VideoControl& video);

    // Loads the saved settings and level calibration and applies everything. In safe mode the
    // saved settings are left alone: defaults apply, the output is 480p, and nothing is saved.
    void begin(bool safeMode, uint32_t nowMs);
    // Saves deferred changes and new calibrations, and reverts an unconfirmed risky change.
    void tick(uint32_t nowMs);

    Outcome set(settings::Id id, int16_t value, Origin origin, uint32_t nowMs);
    int16_t get(settings::Id id) const { return store_.get(id); }
    uint16_t sequence() const { return store_.sequence(); }

    bool revertPending() const { return revertArmed_; }
    uint32_t revertRemainingMs(uint32_t nowMs) const;
    void keep();
    void revert();

    // Defaults for every setting, saved; the level calibration is forgotten too. Profiles are kept
    // (they are deleted one by one); none is active afterwards.
    void factoryReset(uint32_t nowMs);

    // ---- profiles (ADR 0014)
    enum class LoadKind : uint8_t {
        Manual,   // chosen on the web or the console: arms the revert when a risky setting changes
        Oled,     // chosen at the board: confirms itself, like the OLED's own changes
        Auto,     // its signature matched a newly settled source
        Switcher, // SVS NEW INPUT / remote prof<n>: the switcher is the consent
        Startup,  // system.startupProfile
    };
    enum class ProfileResult : uint8_t {
        Loaded,
        RevertArmed, // loaded; everything returns unless kept in time
        Unchanged,   // SVS CURRENT INPUT for the profile already in use
        Missing,     // no such profile
        Refused,     // safe mode
    };

    ProfileResult loadProfile(uint8_t slot, LoadKind kind, uint32_t nowMs);
    // SVS NEW INPUT=<n> / remote prof<n> (`current` false) and SVS CURRENT INPUT=<n> (`current`
    // true: only when profile n is not already active and unmodified).
    ProfileResult switcherInput(uint8_t slot, bool current, uint32_t nowMs);
    // Which signature a saved profile carries: the slot's own (none for a new slot), the settled
    // source's, or none.
    enum class SaveSource : uint8_t { Keep, This, None };
    // The current Output/Input/Picture values into a slot. An empty name keeps the slot's name
    // ("Profile <n>" for a new slot). Refused in safe mode, while a revert is pending, for an
    // invalid slot, for a name the OLED cannot draw, or for SaveSource::This without a settled
    // source.
    bool saveProfile(uint8_t slot, const char* name, SaveSource source);
    bool renameProfile(uint8_t slot, const char* name);
    bool setProfileSource(uint8_t slot, bool forThisSource);
    bool deleteProfile(uint8_t slot);
    // A profile from an export or a backup into a slot, with its own name (empty: "Profile <n>")
    // and signature. Replacing the profile in use leaves none active. Refused in safe mode or for
    // a name the OLED cannot draw.
    bool importProfile(uint8_t slot, const Profile& p);
    const ProfileStore& profiles() const { return profiles_; }
    uint8_t activeProfile() const { return profiles_.active(); }
    bool profileModified() const { return profiles_.modified(); }

    // The settled source, every status refresh (an invalid signature while there is none). A new
    // source loads the profile whose signature fits it best.
    void onSource(const InputSignature& now, uint32_t nowMs);
    const InputSignature& currentSource() const { return currentSource_; }

    // Whether `name` can be a profile name: up to 23 bytes of printable Latin-1 in UTF-8.
    static bool validProfileName(const char* name);
    bool calibrateLevels() { return video_.calibrateLevels(); }
    // The level calibration (port 0 component, 1 RGB), and a backup's copy of it put back.
    void levels(uint8_t port, uint8_t (&gains)[3]) const { video_.baseGains(port, gains); }
    bool restoreLevels(const uint8_t (&component)[3], const uint8_t (&rgb)[3]);
    bool safeMode() const { return safeMode_; }

private:
    void apply(settings::Id id);
    void applyPicture();
    void applyGeometry();
    void applyLines();
    void applySampling();
    void loadLevels();
    void saveLevels();
    uint32_t revertWindowMs() const;
    // The active profile counts as modified when a value it carries differs from the current one.
    void refreshModified();

    platform::FileStore& fs_;
    video::VideoControl& video_;
    settings::Store store_;
    ProfileStore profiles_;
    bool safeMode_ = false;

    bool revertArmed_ = false;
    uint32_t revertDeadlineMs_ = 0;
    int16_t snapshot_[settings::kCount] = {};
    // A profile load armed the revert: it restores every value, and the profile that was active
    // when the snapshot was taken.
    bool revertAll_ = false;
    uint8_t revertActive_ = 0;

    InputSignature currentSource_;
    InputSignature evaluatedSource_; // the last source auto-load looked at
    // The first source after boot keeps the restored profile when it is that profile's best match.
    bool startupHold_ = false;
    // A manual or switcher load owns the next source to settle shortly after it (the load may
    // itself have changed the port, or the switcher may not have switched yet).
    bool loadHold_ = false;
    uint32_t loadHoldUntilMs_ = 0;

    uint16_t savedLevelsSequence_ = 0;
};

} // namespace gbsr::core
