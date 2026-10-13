#pragma once

#include <stdint.h>

#include "chip/programs.h"
#include "chip/tv5725.h"
#include "hal/pulse_timer.h"
#include "hal/si5351.h"
#include "video/adc_levels.h"
#include "video/ops.h"
#include "video/pixel_clock.h"
#include "video/sense.h"
#include "video/tuning.h"
#include "video/types.h"

namespace gbsr::video {

enum class InputPreference : uint8_t { Auto, Rgb, Component };

// Snapshot of what the engine is doing, refreshed every tick, read by the front ends.
struct EngineStatus {
    EngineState state = EngineState::NoPower;
    Headline headline = Headline::ScalerMissing;
    Standard standard = Standard::None;
    InputPort input = InputPort::Rgb;
    SyncType sync = SyncType::None;
    bool interlaced = false;
    bool deinterlacing = false;
    uint16_t sourceLines = 0; // lines per field from the sync processor
    uint16_t hPeriodIf = 0;   // input formatter's line period count
    uint16_t vPeriodIf = 0;   // lines after the formatter (parity = interlace)
    tv5725::ProgramId program = tv5725::ProgramId::Count;
    OutputMode outputMode = OutputMode::Auto; // the mode in effect (never Auto once loaded)
    bool clockAdopted = false;
    bool frameLocked = false;
    uint32_t pixelClockHz = 0;
    uint32_t sourceMilliHz = 0;
    uint32_t outputMilliHz = 0;
    uint8_t sogLevel = 0;
    uint8_t phaseSp = 0;
    uint8_t phaseAdc = 0;
    uint8_t stableCounter = 0;
    uint16_t noSyncCounter = 0;
    uint8_t lockResult = 1;    // PixelClock::SyncResult of the last frame-lock attempt
    uint8_t adcOffset[3] = {}; // R, G, B
    uint8_t adcGain = 0;
    bool adcCalibrationFailed = false;
    uint32_t loads = 0; // programs loaded since boot
    uint32_t lastLoadMs = 0;
};

// The video path's state machine (docs/design/video-engine.md). Call tick() every 20 ms from the
// main loop; requests from the front ends take effect on the next tick.
class Engine {
public:
    Engine(tv5725::Chip& chip, hal::Si5351* clockGen, hal::PulseTimer* timer);

    // Deterministic bring-up from any prior chip state. `clockGenPresent` from the board probe.
    void begin(bool clockGenPresent);
    void tick();

    void setOutputPreference(OutputMode mode);
    void setInputPreference(InputPreference pref);
    OutputMode outputPreference() const { return outputPref_; }
    InputPreference inputPreference() const { return inputPref_; }

    // Re-run the program load for the current source (used after a risky setting change).
    void requestReload();

    bool busy() const;
    const EngineStatus& status() const { return status_; }
    Ops& ops() { return ops_; }

private:
    enum class Phase : uint8_t {
        NoPower,
        CalibrateAdc,
        DetectIdle,
        DetectFlipWait,
        DetectRgbVsync,
        DetectRgbHsync,
        DetectRgbHsyncSettle,
        DetectMedRes,
        DetectWalkRgbs,
        DetectWalkComponent,
        DetectLostSettle,
        Watching, // source connected; watcher decides when to load
        LoadPrewait,
        Load,
        SettleSync,
        SettleMode,
        SettleSog,
        SettleSogWait,
        SettleClamp,
        Running,
    };

    // Detector
    void startDetect();
    void detectFound(bool ypbpr);
    void goLowPower();

    // Loader
    tv5725::ProgramId chooseProgram(Standard standard) const;
    void beginLoad(Standard standard);
    void runLoad();
    void finishSettle();

    // Watcher
    void watch();
    void watchSyncLoss(bool genuineLoss);
    void watchFormatChange(Standard detected);
    void watchStable(bool genuineLoss);
    void followInterlace();
    void loopLevel();
    void lockFrameRate();

    void enterPhase(Phase p);
    uint32_t inPhaseMs() const;
    void refreshStatus();

    tv5725::Chip& chip_;
    LockState lock_;
    Ops ops_;
    Sense sense_;
    PixelClock clock_;
    PathContext ctx_;
    SogOptimizer sogOpt_;
    PhaseOptimizer phaseOpt_;
    OffsetCalibrator adcCal_;
    EngineStatus status_;

    OutputMode outputPref_ = OutputMode::Auto;
    InputPreference inputPref_ = InputPreference::Auto;
    bool reloadRequested_ = false;

    Phase phase_ = Phase::NoPower;
    uint32_t phaseStartMs_ = 0;
    uint32_t lastDetectMs_ = 0;
    uint32_t lastIrqClearMs_ = 0;
    uint32_t lastPowerCheckMs_ = 0;
    uint16_t pollCount_ = 0;
    uint8_t medResSteps_ = 0;
    uint8_t clampTries_ = 0;

    bool sourceConnected_ = false;
    bool hdmiSyncFix_ = false;
    bool doneStagePending_ = false;
    tv5725::ProgramId loadedProgram_ = tv5725::ProgramId::Count;
    Standard pendingStandard_ = Standard::None;

    // watcher memory
    uint8_t newModeCounter_ = 0;
    uint8_t stableCounter_ = 0;
    uint8_t syncLossDebounce_ = 0;
    uint8_t sourceGoneProbes_ = 0;
    bool fallbackTried_ = false;
    bool crtcHold_ = false;
    uint8_t modeUnknownCounter_ = 0;
    uint16_t noModeCsyncCounter_ = 0;
    bool sourceWasClassified_ = false;
    bool doFullRestore_ = false;
    uint32_t sogWindowStartMs_ = 0;
    uint16_t badHsActive_ = 0;
    bool lastAdjustInWindow_ = false;
    // interlace follow
    bool motionAdaptiveActive_ = false;
    uint8_t maPending_ = 0; // ticks until MAPDT_VT_SEL_PRGV goes to 0 after enabling
    uint16_t lastVPeriod_ = 0;
    uint8_t interlacedReads_ = 0;
    uint8_t progressiveReads_ = 0;
    uint8_t timingAdjustDelay_ = 0;
    uint8_t oddEvenWhenArmed_ = 0;
};

} // namespace gbsr::video
