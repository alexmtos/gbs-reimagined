#pragma once

#include "video/control.h"

namespace gbsr::test {

// Records what the controller asks of the video path.
class FakeVideo : public video::VideoControl {
public:
    video::OutputMode output = video::OutputMode::Auto;
    video::InputPreference input = video::InputPreference::Auto;
    bool frameLock = false;
    bool autoGain = false;
    video::PictureAdjust picture;
    video::GeometryAdjust geometry;
    uint8_t base[2][3] = {{0x7B, 0x7B, 0x7B}, {0x7B, 0x7B, 0x7B}};
    uint16_t levelsSeq = 0;
    int outputCalls = 0;

    void setOutputPreference(video::OutputMode m) override {
        output = m;
        ++outputCalls;
    }
    void setInputPreference(video::InputPreference p) override { input = p; }
    void setFrameLock(bool on) override { frameLock = on; }
    void setAutoGain(bool on) override { autoGain = on; }
    void setPicture(const video::PictureAdjust& a) override { picture = a; }
    void setGeometry(const video::GeometryAdjust& a) override { geometry = a; }
    void setBaseGains(uint8_t port, const uint8_t (&g)[3]) override {
        for (int c = 0; c < 3; ++c) {
            base[port][c] = g[c];
        }
    }
    void baseGains(uint8_t port, uint8_t (&g)[3]) const override {
        for (int c = 0; c < 3; ++c) {
            g[c] = base[port][c];
        }
    }
    uint16_t levelsSequence() const override { return levelsSeq; }
    bool calibrateLevels() override { return true; }
};

} // namespace gbsr::test
