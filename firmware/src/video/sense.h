#pragma once

#include <stdint.h>

#include "hal/pulse_timer.h"
#include "video/ops.h"
#include "video/types.h"

namespace gbsr::video {

// Reads what the chip reports about the source: the mode detector's classification, sync
// activity, and rates measured on the debug pin. Read-only towards the picture, except where the
// reference behaviour requires a nudge (noted per function).
// Does the source carry a usable discrete VSync? Judged by a steady line count (median of up to
// nine reads 20 ms apart, two thirds agreeing within ±2), not by the VS-active flag, which some
// sources never raise. A machine (ADR 0013): the HV input needs 260 ms to settle, then one read
// per step; leaves the HV input and SOG mode as found.
struct VsyncProbe {
    bool found = false;
    bool flaggedByChip = false;
    uint16_t lines = 0;
};

class VsyncProber {
public:
    static constexpr uint32_t kSettleMs = 260; // the sync processor's counters need this long
    static constexpr uint32_t kSampleEveryMs = 20;
    static constexpr uint8_t kSamples = 9;

    void start(Ops& ops, uint32_t nowMs);
    // Returns true while still running.
    bool step(Ops& ops, uint32_t nowMs);
    const VsyncProbe& result() const { return result_; }

private:
    void finish(Ops& ops);

    bool running_ = false;
    uint32_t extSync_ = 0;
    uint32_t sogMode_ = 0;
    uint32_t nextMs_ = 0;
    uint8_t taken_ = 0;
    uint16_t samples_[kSamples] = {};
    VsyncProbe result_;
};

class Sense {
public:
    Sense(Ops& ops, hal::PulseTimer* timer) : ops_(ops), timer_(timer) {}

    // The mode detector's verdict (mechanics §3.5). `current` is the standard the path is set up
    // for; RGBHV modes are judged by sync activity only. Counts consecutive stable-but-unnamed
    // passes and reports Unknown after 255 of them. When the source looks like a graphic mode the
    // detector refuses, the VGA..SXGA horizontal targets are jittered by -2..+1 (the detector's
    // horizontal match is very strict) — the one write this class makes on its own.
    Standard videoMode(Standard current, bool inputIsYpbpr);

    // HSync active per the sync processor; for 15 kHz standards it must also be active-low.
    bool hsStable(Standard current);

    // Sync present on the selected input at all (test-bus readout above 0x180).
    bool syncPresent();

    // Field rate of the source in millihertz, 0 if not measurable (needs a running path).
    uint32_t sourceFieldRateMilliHz(bool useSyncProcessorBus, bool csync);
    // Frame rate of the output in millihertz, 0 if not measurable.
    uint32_t outputFrameRateMilliHz();
    // H-PLL rate as edges per second (classifies RGBHV sampling bands).
    uint32_t pllRate(bool csync);

    void forgetVgaTargets() { vgaTargetsCaptured_ = false; }

private:
    uint32_t measureMilliHz();
    void nudgeVgaTargets();
    uint8_t nextSkew();

    Ops& ops_;
    hal::PulseTimer* timer_;
    uint8_t notRecognized_ = 0;
    bool vgaTargetsCaptured_ = false;
    uint8_t vgaTargets_[12] = {};
    uint32_t rng_ = 0x9E3779B9u;
};

// Rates outside this window are measurement errors, not sources.
constexpr uint32_t kMinFieldMilliHz = 47'000;
constexpr uint32_t kMaxFieldMilliHz = 86'000;

} // namespace gbsr::video
