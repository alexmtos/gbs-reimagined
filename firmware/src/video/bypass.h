#pragma once

#include <stdint.h>

#include "video/ops.h"
#include "video/sense.h"
#include "video/tuning.h"

namespace gbsr::video {

// Pass-through paths (docs/research/gbs-control-mechanics.md §6.5). EXPERIMENTAL: written from the
// community firmware's sequences and not yet verified on this bench, which has no HD or PC source
// (docs/bench/M1.md; user decision 2026-10-13).
//
// HD bypass: the ADC samples go through the HD block straight to the DAC; the scaler, memory and
// display PLL idle. Used for HD sources (720p, 1080i/p, PC over component), which the programs do
// not scale, and for any source when the user picks "Passthrough".
void enterHdBypass(Ops& ops, Sense& sense, PathContext& ctx);

// RGBHV bypass: the ADC feeds the DAC directly for computer sources with separate (or composite)
// sync that match no TV mode. Leaves the output on.
void enterRgbhvBypass(Ops& ops, PathContext& ctx);

// The H-PLL charge-pump band for an RGBHV source's measured H-PLL rate (Sense::pllRate): sets KS,
// FS and ICP when the band changes. Returns the band (1..5), or 0 when the rate is implausible.
uint8_t tuneRgbhvBand(Ops& ops, uint32_t pllRate, uint8_t currentBand);

} // namespace gbsr::video
