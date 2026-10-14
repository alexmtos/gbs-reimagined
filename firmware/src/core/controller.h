#pragma once

#include <stdint.h>

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

    // Defaults for every setting, saved; the level calibration is forgotten too.
    void factoryReset(uint32_t nowMs);
    bool calibrateLevels() { return video_.calibrateLevels(); }
    bool safeMode() const { return safeMode_; }

private:
    void apply(settings::Id id);
    void applyPicture();
    void applyGeometry();
    void applyLines();
    void loadLevels();
    void saveLevels();
    uint32_t revertWindowMs() const;

    platform::FileStore& fs_;
    video::VideoControl& video_;
    settings::Store store_;
    bool safeMode_ = false;

    bool revertArmed_ = false;
    uint32_t revertDeadlineMs_ = 0;
    int16_t snapshot_[settings::kCount] = {};

    uint16_t savedLevelsSequence_ = 0;
};

} // namespace gbsr::core
