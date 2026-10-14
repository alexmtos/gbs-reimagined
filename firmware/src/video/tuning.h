#pragma once

#include <stdint.h>

#include "video/ops.h"
#include "video/sense.h"
#include "video/types.h"

namespace gbsr::video {

// What the tuning steps need to know about the current source and path. Owned by the engine.
struct PathContext {
    Standard standard = Standard::None;
    bool inputIsYpbpr = false;
    bool syncCsync = false;
    bool hdBypass = false;
    uint8_t presetId = 0; // of the loaded program (0x01 960p … 0x06 15 kHz, +0x10 PAL)
    uint16_t noSyncCounter = 0;
    bool coastSet = false;
    bool clampSet = false;
    bool phaseSet = false;
    uint8_t maxSogLevel = 31; // 31 = the SOG search has not run for this source
    uint8_t medResLineCount = 0x33;
};

// The 15 kHz output programs, whose vertical downscale has its own line-buffer setup.
bool isDownscale(uint8_t presetId);

// The synchronous part of the post-load sequence (mechanics §4.1 steps 1–30, minus the auto best
// HTotal the external clock makes unnecessary): sync processor, clamp held off, colour patches,
// phase and SOG, per-standard input-formatter / ADC / PLLAD tuning, line-count shifts, mirrored
// blanking, memory settings, resets. Leaves the DAC off; the settle machine turns it on once the
// clamp is locked. Returns true if the SOG level should be optimised before settling (csync with
// an empty sync-processor bus).
bool postLoadTuning(Ops& ops, Sense& sense, PathContext& ctx);

// Per-standard sync-processor parameters (coast, DLT, pulse ignore, H timer). With `modeCheck`,
// an unknown mode during sync loss alternates the DLT so that SD and HDTV can both be detected.
void updateSpDynamic(Ops& ops, Sense& sense, PathContext& ctx, bool modeCheck);

// Horizontal coast window from eight agreeing HPERIOD_IF reads. Sets ctx.coastSet on success.
void updateCoastPosition(Ops& ops, Sense& sense, PathContext& ctx);

// Clamp window from sixteen agreeing line-length reads. Sets ctx.clampSet on success.
void updateClampPosition(Ops& ops, Sense& sense, PathContext& ctx);

// PAL 15 kHz sources show all 576 lines (user decision 2026-10-13). The community programs scale
// by whole numbers and capture 520-548 of them, cutting the top and bottom of the picture; this
// sets the vertical scale that fits 576 lines into the visible height and starts the capture at
// the first active line. Values measured per program with a PAL PS2 (docs/bench/M1.md; 576p in
// docs/bench/M2.md). Leaves the 15 kHz program, not measured, as it is.
void fitPalVertical(Ops& ops, const PathContext& ctx);

// Lowers the SOG slicer from its maximum until sync holds; one level per step (≤ ~70 ms each).
class SogOptimizer {
public:
    void start(Ops& ops, PathContext& ctx);
    // Returns true while still running.
    bool step(Ops& ops, PathContext& ctx);
    bool busy() const { return running_; }

private:
    bool running_ = false;
    uint8_t busBackup_ = 0;
    uint8_t spBackup_ = 0;
};

// Finds the sync-processor phase opposite the worst one by counting H-total mismatches over 34
// phase steps (mechanics §4.4), in chunks so one step stays short.
class PhaseOptimizer {
public:
    void start(Ops& ops, PathContext& ctx);
    // Returns true while still running; on completion ctx.phaseSet holds the verdict.
    bool step(Ops& ops, PathContext& ctx);
    bool busy() const { return running_; }

private:
    void finish(Ops& ops, PathContext& ctx, bool measured);
    bool running_ = false;
    uint16_t pixelClock_ = 0;
    uint8_t steps_ = 0;
    uint8_t bad_ = 0, prevBad_ = 0, prevPrevBad_ = 0;
    uint8_t worstBad_ = 0, worstPhase_ = 0, goodSteps_ = 0;
};

} // namespace gbsr::video
