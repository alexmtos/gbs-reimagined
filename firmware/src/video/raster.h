#pragma once

#include <stdint.h>

#include "video/geometry.h"

namespace gbsr::video {

// Output rasters computed from a standard timing instead of a baseline program (ADR 0004: an
// experimental M2 feature). Pure: the engine writes what these return.

// A display timing as the standards give it, in pixels and lines (VESA DMT, CVT-RB, CEA-861).
struct Timing {
    uint16_t hActive, hFront, hSync, hBack;
    uint16_t vActive, vFront, vSync, vBack;
    bool hNegative, vNegative;
    uint8_t shapeW, shapeH; // the screen's shape at this timing
    uint16_t hTotal() const { return static_cast<uint16_t>(hActive + hFront + hSync + hBack); }
    uint16_t vTotal() const { return static_cast<uint16_t>(vActive + vFront + vSync + vBack); }
};

// The timing table of the bench spike, by name ("1024x768", "1080p", ...).
bool timingByName(const char* name, Timing& out);

// The sync registers of a raster; the windows and scales come from the geometry.
struct RasterRegs {
    uint16_t hsyncRst = 0, vsyncRst = 0; // total − 1
    uint16_t hsSt = 0, hsSp = 0;         // ST > SP: a negative pulse (mechanics §6.2)
    uint16_t vsSt = 0, vsSp = 0;
    uint16_t vsynSize = 0; // VDS_VSYN_SIZE1/2, as the post-load writes them: VSYNC_RST + 2
};

struct Raster {
    RasterRegs regs;
    GeometryBase base; // invalid when the donor's is, or the timing does not fit
};

// `t` drawn with `hUnits` output-clock units per line, on a loaded program's capture (`donor`, its
// baseline): an oversampled line (several units per pixel) gives the horizontal scaler room, as it
// only enlarges. The baseline's window is the active area showing the donor's capture across it,
// with the donor's fetch distances; the geometry then shapes it like any program's.
Raster makeRaster(const GeometryBase& donor, const Timing& t, uint16_t hUnits);

// The baseline on a line of `newHTotal` (VDS_HSYNC_RST) at the same duration (the clock follows):
// every horizontal position and the scale follow the line, so the picture stays as it was until
// the geometry uses the finer units, e.g. to narrow 1080p's 1:1 picture.
GeometryBase stretchBase(const GeometryBase& b, uint16_t newHTotal);

// A horizontal position on a line of `fromHTotal` moved to the same time on `toHTotal`.
uint16_t stretchPosition(uint16_t pos, uint16_t fromHTotal, uint16_t toHTotal);

// The chip's internal display-clock codes (PLL648_CONTROL_01; RD-5725 p.81): 0x25 40.5 MHz ...
// 0xA5 162 MHz. 0 for a code that is not one of them.
uint32_t displayClockHz(uint8_t code);

// What the loader writes for an extra output mode (ADR 0015): the internal clock code its line is
// drawn for, so the chip already runs it at `nominalMilliHz` before the generator takes over, and
// the raster. The highest clock that leaves the scaler a 4:3 divisor of at least 683, so a 16:9
// picture still keeps 512 (below that, columns were displaced: docs/bench/M2.md).
struct LoadRaster {
    bool valid = false;
    uint8_t code = 0;
    Raster raster;
};
LoadRaster rasterForMode(const GeometryBase& donor, const Timing& t, uint32_t nominalMilliHz);

// A program's line made longer, at a higher internal clock and the same rate, when `adj`'s picture
// (aspect, integer scale; not the user's size) would need to shrink by more than 2 %: PAL 4:3 and
// integer scale at 1080p. The lowest clock that leaves the divisor 1 % under 1:1. Invalid when no
// longer line is needed or none fits.
struct LoadStretch {
    bool valid = false;
    uint8_t code = 0;
    uint16_t htotal = 0; // VDS_HSYNC_RST
    GeometryBase base;
};
LoadStretch stretchFor(const GeometryBase& program, const GeometryAdjust& adj, uint8_t programCode);

} // namespace gbsr::video
