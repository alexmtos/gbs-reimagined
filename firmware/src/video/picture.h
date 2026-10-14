#pragma once

#include <stdint.h>

#include "video/ops.h"

namespace gbsr::video {

// The user's picture adjustments (picture.* settings).
struct PictureAdjust {
    int8_t brightness = 0; // -32..32: luma offset, one code per step
    int8_t contrast = 0;   // -32..32: luma gain, 0.5x..1.5x
    int8_t saturation = 0; // -32..32: chroma gain, 0 (grey)..2x
    uint8_t sharpness = 0; // 0 off, 1 low, 2 medium, 3 high: horizontal peaking

    bool operator==(const PictureAdjust& o) const {
        return brightness == o.brightness && contrast == o.contrast && saturation == o.saturation &&
               sharpness == o.sharpness;
    }
};

// Owns the output colour-space and peaking fields (VDS_Y_GAIN, VDS_Y/U/V_OFST, VDS_UCOS/VCOS_GAIN,
// VDS_PK_LB/LH_GAIN, VDS_PK_Y_H_BYPS): writes the neutral values for the input type plus the
// adjustments. The engine runs it after every program load, since a program rewrites them.
void applyPicture(Ops& ops, bool inputIsYpbpr, const PictureAdjust& adj);

} // namespace gbsr::video
