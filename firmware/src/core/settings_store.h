#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/settings_def.h"
#include "platform/storage.h"

namespace gbsr::settings {

Def def(Id id);
// Offered to the user: of this milestone (kMilestone, generated) and not pending.
bool available(Id id);

// The schema id ("output.resolution") into `out`.
void idName(Id id, char* out, size_t cap);
bool findId(const char* name, Id& out);

// Text form of a value, shared by the console and the web API: the enum value id ("r1080p"),
// "on"/"off", a special value's name ("auto", "lastUsed"), or the integer.
void formatValue(Id id, int16_t value, char* out, size_t cap);
// Accepts what formatValue produces, plus 1/0, true/false and a bare enum index.
bool parseValue(Id id, const char* text, int16_t& out);

// The values in effect, persisted in /settings.bin as (id hash, value) pairs so the schema can
// evolve: unknown hashes are ignored, missing ones keep their defaults. Saves are deferred until
// three seconds after the last change; nothing is written at boot.
class Store {
public:
    enum class SetResult : uint8_t { Changed, Unchanged, OutOfRange };

    static constexpr uint32_t kSaveDelayMs = 3000;

    explicit Store(platform::FileStore& fs);

    void resetToDefaults(); // in memory; saved like any change
    // Loads the file; returns false (everything at its default) when it is absent or damaged.
    bool load();

    int16_t get(Id id) const { return values_[static_cast<uint8_t>(id)]; }
    bool on(Id id) const { return get(id) != 0; }
    SetResult set(Id id, int16_t value, uint32_t nowMs);

    // Writes a pending change once it has rested for kSaveDelayMs.
    void tick(uint32_t nowMs);
    bool savePending() const { return dirty_; }
    bool saveNow();
    // Increments on every change, so observers can tell something moved.
    uint16_t sequence() const { return sequence_; }

private:
    platform::FileStore& fs_;
    int16_t values_[kCount];
    bool dirty_ = false;
    uint32_t changedMs_ = 0;
    uint16_t sequence_ = 0;
};

} // namespace gbsr::settings
