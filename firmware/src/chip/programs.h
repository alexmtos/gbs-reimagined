#pragma once

#include <stdint.h>

#include "chip/programs.gen.h"
#include "platform/progmem.h"

namespace gbsr::tv5725 {

enum class Standard : uint8_t { Any, Ntsc, Pal };

// A baseline register image (schema/programs/*.yaml), stored in flash as a sequence of records:
// segment, first register, count, then `count` values. Records never cross a 16-byte boundary,
// which matches the chip's maximum burst.
struct ProgramInfo {
    ProgramId id;
    Standard standard;
    const char* name;       // PROGMEM
    const uint8_t* records; // PROGMEM
    uint16_t recordsSize;
    uint16_t valueCount;
};

extern const ProgramInfo kPrograms[] PROGMEM;

// Copies the descriptor of `id` out of flash.
ProgramInfo programInfo(ProgramId id);

} // namespace gbsr::tv5725
