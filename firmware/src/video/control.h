#pragma once

#include <stdint.h>

#include "video/picture.h"
#include "video/types.h"

namespace gbsr::video {

// What the settings layer may ask of the video path (implemented by Engine). Kept narrow so the
// controller can be tested without a chip.
class VideoControl {
public:
    virtual ~VideoControl() = default;
    virtual void setOutputPreference(OutputMode mode) = 0;
    virtual void setInputPreference(InputPreference pref) = 0;
    virtual void setFrameLock(bool on) = 0;
    virtual void setAutoGain(bool on) = 0;
    virtual void setPicture(const PictureAdjust& adj) = 0;
    virtual void setBaseGains(uint8_t port, const uint8_t (&gains)[3]) = 0;
    virtual void baseGains(uint8_t port, uint8_t (&gains)[3]) const = 0;
    virtual uint16_t levelsSequence() const = 0;
    virtual bool calibrateLevels() = 0;
};

} // namespace gbsr::video
