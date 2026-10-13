#pragma once

#include <stdint.h>

#include "video/ops.h"

namespace gbsr::video {

// Black-level calibration of the three ADC channels, run once per bring-up before detection
// (docs/design/video-engine.md, boot step 6; procedure in docs/research/gbs-control-mechanics.md
// §4.2). With the ADC isolated on the unconnected input, each channel's offset is raised from 0x3D
// until the decimator's test output reads near zero. The results land in LockState::adcOffset and
// are applied with every program (Ops::applyAdcLevels). Each step reads for a bounded slice, so the
// main loop stays responsive (ADR 0013); a full run takes about two seconds.
class OffsetCalibrator {
public:
    // Isolates the ADC and starts with the green channel. The caller restores the idle baseline
    // once step() returns false.
    void start(Ops& ops);
    // Returns true while still running.
    bool step(Ops& ops);
    bool busy() const { return stage_ != Stage::Idle; }
    // The last run hit the offset ceiling on some channel and fell back to the defaults.
    bool failed() const { return failed_; }

private:
    enum class Stage : uint8_t { Idle, Settle, Measure };

    void select(Ops& ops, uint8_t index);
    void advance(Ops& ops);
    void settle(uint32_t ms);

    Stage stage_ = Stage::Idle;
    uint8_t index_ = 0; // position in the measuring order: G, R, B
    uint8_t offset_ = 0;
    uint8_t misses_ = 0;
    uint16_t hits_ = 0;
    bool failed_ = false;
    uint32_t stageStartMs_ = 0;
    uint32_t settleMs_ = 0;
    uint8_t results_[3] = {}; // in measuring order
};

} // namespace gbsr::video
