#pragma once

#include <stdint.h>

#include "chip/tv5725.h"

namespace gbsr::video {

// Scanlines reuse the motion-adaptive line mixer: with the motion index forced, it darkens one of
// each pair of lines the line doubler makes, before the scaler (gbs-control-lessons-videopath §5.1;
// no scanline feature in the datasheet). The mixer is the deinterlacer's, so scanlines run only on
// progressive sources, and the engine takes them off before it engages the deinterlacer.
//
// While they are on, this module owns the fields below. Turning them off writes back the values the
// program left, captured right after each load (not what a disengaged deinterlacer left: with the
// path select back at 1 the mixer is out of the path either way).
struct ScanlineBase {
    bool valid = false;
    uint8_t uvdlyPdSp = 0, uvdlyPdSt = 0, enUvDeint = 0, uvMiDetByps = 0, uvMiOffset = 0;
    uint8_t moAdpUvEn = 0, bobPldyRamByps = 0, pdRamByps = 0, rffYuvDeinterlace = 0;
    uint8_t yMiDetByps = 0, wlevGain = 0, wlevByps = 0, viirCoef = 0, viirByps = 0;
    uint8_t yMiOffset = 0, rffLineFlip = 0, vtSelPrgv = 0;
    bool operator==(const ScanlineBase&) const = default;
};

constexpr uint8_t kScanlineLevels = 8;

// picture.scanlineStrength 1..8 → the motion index the mixer is forced to: 0x7F leaves lines as
// they are, 0 darkens them most; level 5 is the reference's default (0x30).
uint8_t scanlineOffset(uint8_t level);

ScanlineBase readScanlineBase(tv5725::Chip& chip);
void enableScanlines(tv5725::Chip& chip, uint8_t level);
void setScanlineStrength(tv5725::Chip& chip, uint8_t level);
void disableScanlines(tv5725::Chip& chip, const ScanlineBase& base);

} // namespace gbsr::video
