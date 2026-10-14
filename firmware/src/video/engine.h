#pragma once

#include <stdint.h>

#include "chip/programs.h"
#include "chip/tv5725.h"
#include "hal/pulse_timer.h"
#include "hal/si5351.h"
#include "video/adc_levels.h"
#include "video/bypass.h"
#include "video/control.h"
#include "video/geometry.h"
#include "video/ops.h"
#include "video/picture.h"
#include "video/pixel_clock.h"
#include "video/scanlines.h"
#include "video/sense.h"
#include "video/tuning.h"
#include "video/types.h"

namespace gbsr::video {

// Snapshot of what the engine is doing, refreshed every tick, read by the front ends.
struct EngineStatus {
    EngineState state = EngineState::NoPower;
    Headline headline = Headline::ScalerMissing;
    Standard standard = Standard::None;
    InputPort input = InputPort::Rgb;
    SyncType sync = SyncType::None;
    bool interlaced = false;
    bool deinterlacing = false;
    bool bob = false;         // deinterlacing in bob mode
    bool scanlines = false;   // the scanline mixer is on
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
    uint8_t adcGain[3] = {};   // R, G, B
    bool offsetCalibrationFailed = false;
    uint8_t levelCalibration = 0; // GainCalibrator::Outcome of the last "calibrate levels"
    bool autoGain = false;
    GeometryRegs geometry; // as last written, while geometryValid
    bool geometryValid = false;
    uint32_t loads = 0; // programs loaded since boot
    uint32_t lastLoadMs = 0;
};

// The video path's state machine (docs/design/video-engine.md). Call tick() every 20 ms from the
// main loop; requests from the front ends take effect on the next tick.
class Engine : public VideoControl {
public:
    Engine(tv5725::Chip& chip, hal::Si5351* clockGen, hal::PulseTimer* timer);

    // Deterministic bring-up from any prior chip state. `clockGenPresent` from the board probe.
    void begin(bool clockGenPresent);
    void tick();

    void setOutputPreference(OutputMode mode) override;
    void setInputPreference(InputPreference pref) override;
    OutputMode outputPreference() const { return outputPref_; }
    InputPreference inputPreference() const { return inputPref_; }

    // Re-run the program load for the current source (used after a risky setting change).
    void requestReload();

    // "Calibrate levels": with a white screen showing, set each channel's gain so white sits just
    // under the ADC ceiling; kept per input for every later source. False when there is no stable
    // picture to measure.
    bool calibrateLevels() override;
    // input.autoGain: keep lowering the gain while the picture clips; restarts per source.
    void setAutoGain(bool on) override;
    // output.frameLock: with it off the display runs on the chip's own clock at the program's rate.
    void setFrameLock(bool on) override;
    // picture.*: applied now and after every program load.
    void setPicture(const PictureAdjust& adj) override;
    // output.aspect / position / size: applied now and after every program load, always from
    // the loaded program's own geometry.
    void setGeometry(const GeometryAdjust& adj) override;
    // picture.scanlines / scanlineStrength / deinterlace: followed every settled tick.
    void setLines(const LineAdjust& adj) override;

    // Base gains per input (0 component, 1 RGB), the gains a new source starts with: restored from
    // storage at boot, replaced by each successful "calibrate levels". levelsSequence() moves on
    // every replacement so the owner knows when to save.
    void setBaseGains(uint8_t port, const uint8_t (&gains)[3]) override;
    void baseGains(uint8_t port, uint8_t (&gains)[3]) const override;
    uint16_t levelsSequence() const override { return levelsSequence_; }

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
        DetectVsyncProbe,
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
    OutputMode effectiveMode(Standard standard) const;
    tv5725::ProgramId chooseProgram(Standard standard) const;
    void beginLoad(Standard standard);
    void runLoad();
    void startHdBypass(Standard standard);
    void startRgbhvBypass();
    void watchRgbhv();
    void finishSettle();

    // Watcher
    void watch();
    void watchSyncLoss(bool genuineLoss);
    void watchFormatChange(Standard detected);
    void watchStable(bool genuineLoss);
    void followInterlace();
    void loopLevel();
    void lockFrameRate();
    void applyGeometry();
    void followScanlines();
    void scanlinesOff();

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
    VsyncProber vsyncProber_;
    GainCalibrator gainCal_;
    AutoGain autoGain_;
    bool autoGainOn_ = false;
    bool frameLockOn_ = true;
    PictureAdjust picture_;
    GeometryAdjust geometry_;
    GeometryBase geometryBase_; // of the loaded program; invalid in bypass
    LineAdjust lines_;
    ScanlineBase scanlineBase_; // of the loaded program; invalid in bypass
    bool scanlinesOn_ = false;
    uint8_t scanlineLevel_ = 0;      // as written while on
    bool progressiveStable_ = false; // followInterlace has confirmed a progressive source
    uint8_t scanlineHold_ = 0;       // ticks to wait after the deinterlacer disengages
    // Gains a new source starts with, per input (0 component, 1 RGB): calibrated or the preset.
    uint8_t baseGain_[2][3] = {{0x7B, 0x7B, 0x7B}, {0x7B, 0x7B, 0x7B}};
    uint16_t levelsSequence_ = 0;
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
    bool bypass_ = false; // an HD or RGBHV pass-through path is set up instead of a program
    uint8_t rgbhvBand_ = 0;
    uint16_t rgbhvUnstable_ = 0;
    uint32_t lastBandCheckMs_ = 0;
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
