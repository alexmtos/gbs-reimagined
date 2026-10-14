#pragma once

#include <stdint.h>

#include "chip/programs.h"
#include "chip/tv5725.h"
#include "video/types.h"

namespace gbsr::video {

// What the sampling path is currently set to. Owned by the engine; the primitives keep it in step
// with the registers they write.
struct LockState {
    uint8_t phaseSp = 16;  // sync-processor sampling phase, 0..31
    uint8_t phaseAdc = 16; // ADC sampling phase, 0..31
    uint8_t sogLevel = 5;  // sync-on-green slicer, 0..31
    // input.syncThreshold / input.phase: a fixed value the automatic code cannot move (-1: none).
    // The fields above keep the automatic choice; the registers get the override.
    int8_t sogManual = -1;
    int8_t phaseAdcManual = -1;
    uint8_t osr = 0; // oversampling ratio in effect: 1, 2 or 4 (0 = not set)
    uint8_t adcOffset[3] = {0x40, 0x40, 0x40}; // R, G, B black levels; boot calibration
    uint8_t adcGain[3] = {0x7B, 0x7B, 0x7B};   // R, G, B; higher values dim the picture (a range)
};

// What the load-time patches need to know about the source.
struct LoadContext {
    Standard standard = Standard::None;
    bool inputIsYpbpr = false;
    bool syncCsync = false;
    bool hdmiSyncFix = false; // keep DAC and sync out off while loading (HDMI converter re-lock)
    uint8_t medResLineCount = 0x33;
};

// Primitive operations on the TV5725 video path: resets, latches, constant sections, program
// writes, sampling controls. No policy lives here; the machines decide when to call what.
// Sequences and constants follow the community firmware's proven bring-up
// (docs/research/gbs-control-mechanics.md, cited per function).
class Ops {
public:
    Ops(tv5725::Chip& chip, LockState& lock) : chip_(chip), lock_(lock) {}

    tv5725::Chip& chip() { return chip_; }
    LockState& lock() { return lock_; }

    // --- whole-chip state -------------------------------------------------------------------
    void zeroAll();      // every block in reset, all six segments cleared (§2.1)
    void idleBaseline(); // detection-ready state between programs (§2.3)
    void prepareSyncProcessor(Standard standard, bool rgbhvBypass); // SP baseline (§3.6)
    bool checkPower(); // scratch write/read-back on ADC_UNUSED_69

    // --- programs and sections ---------------------------------------------------------------
    void loadHdBypassSection();
    void loadModeDetectSection(const LoadContext& ctx);
    void loadDeinterlacerSection();
    // Writes a baseline program with the load-time patches (§2.4): reset registers kept, IF sync
    // bits, input select and clamps from the context, SP defaults, clamp held off, PIP cleared.
    void writeProgram(tv5725::ProgramId id, const LoadContext& ctx);

    // --- resets and latches (§4.4) -------------------------------------------------------------
    void resetDigital(bool hdBypass);
    void resetPllad();
    void latchPllad();
    void resetPll();
    void resetSdram();
    void resetSyncProcessor();
    void resetModeDetect();
    void nudgeModeDetect();
    void resetDebugPort();
    void clearInterrupts();             // reset all latched IRQ status
    void clearInterruptsExceptSogBad(); // the 3 s housekeeping (§5.7)
    void resetSogBadIrq();
    void resetSogSwitchIrq();
    void resetNoHsyncIrq();

    // --- sampling ----------------------------------------------------------------------------
    void setSogLevel(uint8_t level); // also re-latches both phases and the PLLAD (§3.6)
    uint8_t sogLevelInEffect() const;
    uint8_t phaseAdcInEffect() const;
    void latchPhaseSp();
    void latchPhaseAdc();
    void togglePhaseUnits();
    void setOverSampleRatio(uint8_t ratio, bool prepareOnly, Standard standard, bool hdBypass);
    void setCsVsStart(uint16_t start);
    void setCsVsStop(uint16_t stop);
    uint16_t csVsStop();
    void applyAdcLevels(); // offsets and gain from the lock state
    void applyRgbPatches();
    void applyYuvPatches();

    // --- picture path ------------------------------------------------------------------------
    void freeze();
    void unfreeze();
    void outputVga(bool inputIsYpbpr); // RGB out with sync (component output is not offered)
    void setDac(bool on);
    void setSyncOut(bool on);
    void enableMotionAdaptive(Standard standard);
    void disableMotionAdaptive();

private:
    tv5725::Chip& chip_;
    LockState& lock_;
};

} // namespace gbsr::video
