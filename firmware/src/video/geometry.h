#pragma once

#include <stdint.h>

#include "chip/tv5725.h"
#include "video/ops.h"
#include "video/sense.h"
#include "video/tuning.h"
#include "video/types.h"

namespace gbsr::video {

// The geometry a program and its post-load leave on the chip. Every adjustment is computed from
// this, never from the chip's current values, so repeated changes cannot drift: a scale derived
// from its own previous output walked down and tore the picture (videopath lessons §3.5).
struct GeometryBase {
    bool valid = false;
    uint8_t presetId = 0;
    uint16_t htotal = 0; // VDS_HSYNC_RST as loaded
    uint16_t disHbSt = 0, disHbSp = 0, hbSt = 0, hbSp = 0;
    uint16_t syncEnd = 0;   // last unit of the HSync pulse
    uint16_t hscale = 1024; // 1024 = bypassed (1:1)
    uint16_t disVbSt = 0, disVbSp = 0, vbSt = 0, vbSp = 0;
    uint16_t vtotal = 0;   // VDS_VSYNC_RST
    uint16_t vsyncEnd = 0; // last line of the VSync pulse
    uint16_t vscale = 1024;
    uint16_t ifLine = 0; // input-formatter samples per source line
    uint16_t ifHbSp2 = 0;
    uint16_t ifVbSp = 0, ifVbGap = 2; // capture start, and IF_VB_SP − IF_VB_ST
    uint16_t ifFrame = 0;             // where IF_VB_ST wraps when the capture starts at the top
    uint8_t lineStep = 1;             // input-formatter lines per source line
    // What "4:3" means for this source, in input-formatter units: the width of a 4:3 picture and
    // its full height. Known for SD and EDTV (BT.601); otherwise the program's own capture.
    uint16_t width43 = 0;
    uint16_t fullHeight = 0;
    // What the HDMI converter shows of this raster: the active share of the standard timing it
    // recognises (frameActive of frameTotal), that frame's lines and its shape. Zero for the 15 kHz
    // programs, which bypass the converter: position only.
    uint16_t frameActive = 0, frameTotal = 0, frameLines = 0;
    uint8_t shapeW = 0, shapeH = 0;
    bool hasFrame() const { return frameTotal != 0 && frameLines != 0; }
};

// The fields the geometry owns once a program is loaded. Scales of 1024 mean bypassed.
struct GeometryRegs {
    uint16_t hscale = 1024, vscale = 1024;
    uint16_t disHbSt = 0, disHbSp = 0, hbSt = 0, hbSp = 0;
    uint16_t disVbSt = 0, disVbSp = 0, vbSt = 0, vbSp = 0;
    uint16_t ifHbSp2 = 0, ifVbSt = 0, ifVbSp = 0;
    bool operator==(const GeometryRegs&) const = default;
};

// Reads the baseline right after the post-load (and before any frame-lock line change). Invalid
// in bypass, after a bus error, or for windows this code does not understand.
GeometryBase readGeometryBase(tv5725::Chip& chip, const PathContext& ctx);

// The registers for `adj` on a line of `htotal` (VDS_HSYNC_RST, which the frame lock may have
// shortened since the load; an implausible value falls back to the baseline's). Pure: same inputs,
// same result. Meaningless for an invalid baseline: callers check `valid` first.
GeometryRegs computeGeometry(const GeometryBase& base, const GeometryAdjust& adj, uint16_t htotal);

// The horizontal divisor `adj`'s shape asks for on a line of `htotal`, before the user's size and
// the scaler's limits: above 1024 the picture would have to shrink, which only a longer line
// allows (video/raster.h). 1024 without a frame or a valid baseline.
int32_t neededHScale(const GeometryBase& base, const GeometryAdjust& adj, uint16_t htotal);

// Writes every owned field; the external blanking mirrors the display blanking, as after a load.
void writeGeometry(tv5725::Chip& chip, const GeometryRegs& regs);

// Changes the output line length (VDS_HSYNC_RST) to `bestHTotal` for the frame lock, with the
// windows recomputed for the new line and HSync kept inside the blanking, written at a field start
// (mechanics §6.2, "Changing H total at runtime"). Without a valid baseline only the line and
// HSync change. Refuses differences above 6 % of the line (16 % for medium-resolution sources).
// Returns true when applied or nothing was needed.
bool applyBestHTotal(Ops& ops, Sense& sense, PathContext& ctx, uint16_t bestHTotal,
                     const GeometryBase& base, const GeometryAdjust& adj);

} // namespace gbsr::video
