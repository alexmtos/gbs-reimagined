#pragma once

#include <stdint.h>

#include "core/settings.gen.h"
#include "platform/progmem.h"

namespace gbsr::settings {

enum class Type : uint8_t { Enum, Int, Bool };

constexpr int16_t kNoSpecial = -32768;
constexpr uint8_t kFlagRisky = 0x01;
constexpr uint8_t kFlagAdvanced = 0x02;
constexpr uint8_t kMilestoneShift = 4; // bits 4..5
constexpr uint8_t kFlagPending = 0x40;

// One setting, as generated from schema/settings.yaml. Values are stored as int16: enum index,
// 0/1 for bool, or the integer itself. `special` lists up to two integer values with a name
// (e.g. -1 = "auto").
struct Def {
    uint32_t hash;         // FNV-1a of the id; the key used in persisted files
    uint16_t idOffset;     // into kIdNames
    uint16_t valuesOffset; // into kValueIds, or 0xFFFF
    int16_t min;
    int16_t max;
    int16_t def;
    int16_t special[2];
    Type type;
    uint8_t flags;

    bool risky() const { return flags & kFlagRisky; }
    bool advanced() const { return flags & kFlagAdvanced; }
    uint8_t milestone() const { return (flags >> kMilestoneShift) & 0x03; }
    // Planned for the current milestone but not implemented yet.
    bool pending() const { return flags & kFlagPending; }
};

extern const char kIdNames[] PROGMEM;
extern const char kValueIds[] PROGMEM;
extern const Def kDefs[] PROGMEM;

constexpr uint8_t kCount = static_cast<uint8_t>(Id::Count);

} // namespace gbsr::settings
