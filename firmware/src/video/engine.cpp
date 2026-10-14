#include "video/engine.h"

#include "chip/fields.gen.h"
#include "platform/time.h"

namespace gbsr::video {

using namespace tv5725::fields;
using platform::nowMs;
using platform::sleepMs;

namespace {

constexpr uint32_t kDetectIntervalMs = 500;
constexpr uint32_t kFlipSettleMs = 200;
constexpr uint32_t kVsyncWaitMs = 360;
constexpr uint32_t kHsyncWaitMs = 400;
constexpr uint32_t kHsyncSettleMs = 120;
constexpr uint32_t kWalkWindowMs = 6000;
constexpr uint32_t kLoadPrewaitMs = 400;
constexpr uint32_t kSettleSyncMs = 2002;
constexpr uint32_t kSettleModeMs = 1505;
constexpr uint32_t kSogSettleMs = 300;
constexpr uint8_t kClampTries = 25;
constexpr uint16_t kTeardownCount = 0x07FE;
constexpr uint32_t kStatusRefreshMs = 250;
constexpr uint32_t kAdoptWatchdogMs = 10'000;

bool isInterlacedCount(uint16_t v) {
    return v == 522 || v == 524 || v == 526 || v == 622 || v == 624 || v == 626;
}

bool isProgressiveCount(uint16_t v) {
    return v == 521 || v == 523 || v == 525 || v == 623 || v == 625 || v == 627;
}

bool isHd(Standard s) {
    return s == Standard::Hd720p || s == Standard::Hd1080i || s == Standard::Hd1080p ||
           s == Standard::PcOnYpbpr;
}

uint32_t gStatusRefreshedMs = 0;
uint32_t gNotAdoptedSinceMs = 0;

} // namespace

Engine::Engine(tv5725::Chip& chip, hal::Si5351* clockGen, hal::PulseTimer* timer)
    : chip_(chip), ops_(chip, lock_), sense_(ops_, timer), clock_(ops_, sense_, clockGen) {}

void Engine::enterPhase(Phase p) {
    phase_ = p;
    phaseStartMs_ = nowMs();
    pollCount_ = 0;
}

uint32_t Engine::inPhaseMs() const {
    return nowMs() - phaseStartMs_;
}

void Engine::setOutputPreference(OutputMode mode) {
    if (mode != outputPref_) {
        outputPref_ = mode;
        reloadRequested_ = true;
    }
}

void Engine::setInputPreference(InputPreference pref) {
    inputPref_ = pref;
    if (pref == InputPreference::Auto || !sourceConnected_) {
        return; // detection picks the input up on its next attempt
    }
    // Choosing an input while a source on the other one is shown switches to it now
    // (bench 2026-10-13: with RGB and component both connected the choice did nothing).
    const bool wantYpbpr = pref == InputPreference::Component;
    if (wantYpbpr != ctx_.inputIsYpbpr) {
        goLowPower();
        chip_.write(ADC_INPUT_SEL, wantYpbpr ? 0 : 1);
    }
}

void Engine::requestReload() {
    reloadRequested_ = true;
}

bool Engine::calibrateLevels() {
    if (phase_ != Phase::Running || loadedProgram_ == tv5725::ProgramId::Count ||
        stableCounter_ < 10 || ctx_.noSyncCounter != 0 || gainCal_.busy()) {
        return false;
    }
    gainCal_.start(ops_, ctx_.inputIsYpbpr);
    return true;
}

void Engine::setFrameLock(bool on) {
    if (on == frameLockOn_) {
        return;
    }
    frameLockOn_ = on;
    if (loadedProgram_ == tv5725::ProgramId::Count) {
        return;
    }
    if (!on) {
        clock_.release(tv5725::programInfo(loadedProgram_).displayClock);
    } else if (phase_ == Phase::Running && clock_.present()) {
        doneStagePending_ = true; // adopt and lock at the next settled moment
    }
}

void Engine::setPicture(const PictureAdjust& adj) {
    if (adj == picture_) {
        return;
    }
    picture_ = adj;
    if (loadedProgram_ != tv5725::ProgramId::Count) {
        applyPicture(ops_, ctx_.inputIsYpbpr, picture_);
    }
}

void Engine::setGeometry(const GeometryAdjust& adj) {
    if (adj == geometry_) {
        return;
    }
    geometry_ = adj;
    applyGeometry();
}

void Engine::applyGeometry() {
    if (bypass_ || loadedProgram_ == tv5725::ProgramId::Count) {
        status_.geometryValid = false;
        return;
    }
    if (!geometryBase_.valid) {
        // Nothing has been written over the program yet, so its geometry can still be read.
        geometryBase_ = readGeometryBase(chip_, ctx_);
        if (!geometryBase_.valid) {
            status_.geometryValid = false;
            return;
        }
    }
    status_.geometry =
        computeGeometry(geometryBase_, geometry_, static_cast<uint16_t>(chip_.get(VDS_HSYNC_RST)));
    status_.geometryValid = true;
    writeGeometry(chip_, status_.geometry);
}

void Engine::setBaseGains(uint8_t port, const uint8_t (&gains)[3]) {
    for (uint8_t c = 0; c < 3; ++c) {
        baseGain_[port ? 1 : 0][c] = gains[c];
    }
}

void Engine::baseGains(uint8_t port, uint8_t (&gains)[3]) const {
    for (uint8_t c = 0; c < 3; ++c) {
        gains[c] = baseGain_[port ? 1 : 0][c];
    }
}

void Engine::setAutoGain(bool on) {
    if (on && !autoGainOn_) {
        autoGain_.restart();
    }
    autoGainOn_ = on;
}

bool Engine::busy() const {
    return phase_ == Phase::Load || phase_ == Phase::LoadPrewait || phase_ == Phase::SettleSync ||
           phase_ == Phase::SettleMode || phase_ == Phase::SettleSog ||
           phase_ == Phase::SettleSogWait || phase_ == Phase::SettleClamp;
}

// ------------------------------------------------------------------------------------------------
// Bring-up

void Engine::begin(bool clockGenPresent) {
    clock_.setPresent(clockGenPresent);
    if (!chip_.probe()) {
        enterPhase(Phase::NoPower);
        refreshStatus();
        return;
    }
    // Deterministic from any prior state: the scaler keeps its registers across an ESP reset.
    ops_.zeroAll();
    ops_.idleBaseline();
    ops_.prepareSyncProcessor(Standard::None, false);
    ops_.idleBaseline();
    ctx_ = PathContext{};
    sourceConnected_ = false;
    loadedProgram_ = tv5725::ProgramId::Count;
    adcCal_.start(ops_);
    enterPhase(Phase::CalibrateAdc);
    refreshStatus();
}

void Engine::goLowPower() {
    gainCal_.abort(ops_);
    ops_.chip().write(OUT_SYNC_CNTRL, 0);
    ops_.setDac(false);
    ops_.idleBaseline();
    ops_.prepareSyncProcessor(Standard::None, false);
    ctx_ = PathContext{};
    clock_.unlock();
    sourceConnected_ = false;
    doneStagePending_ = false;
    motionAdaptiveActive_ = false;
    loadedProgram_ = tv5725::ProgramId::Count;
    bypass_ = false;
    geometryBase_ = GeometryBase{};
    status_.geometryValid = false;
    newModeCounter_ = stableCounter_ = syncLossDebounce_ = sourceGoneProbes_ = 0;
    fallbackTried_ = crtcHold_ = sourceWasClassified_ = false;
    lastDetectMs_ = nowMs();
    enterPhase(Phase::DetectIdle);
}

// ------------------------------------------------------------------------------------------------
// Main tick

void Engine::tick() {
    const uint32_t now = nowMs();
    if (phase_ == Phase::NoPower) {
        if (now - lastPowerCheckMs_ >= kDetectIntervalMs) {
            lastPowerCheckMs_ = now;
            if (chip_.probe()) {
                begin(clock_.present());
            }
        }
        refreshStatus();
        return;
    }

    if (sourceConnected_ && now - lastIrqClearMs_ > 3000) {
        ops_.clearInterruptsExceptSogBad(); // latched IRQs except SOG-bad, which the watcher reads
        lastIrqClearMs_ = now;
    }

    switch (phase_) {
    case Phase::CalibrateAdc:
        if (!adcCal_.step(ops_)) {
            // The calibration borrowed the sync processor, clocks and test bus: back to idle.
            ops_.prepareSyncProcessor(Standard::None, false);
            ops_.idleBaseline();
            lastDetectMs_ = nowMs() - kDetectIntervalMs;
            lastIrqClearMs_ = nowMs();
            enterPhase(Phase::DetectIdle);
        }
        break;
    case Phase::DetectIdle:
        if (now - lastDetectMs_ >= kDetectIntervalMs) {
            lastDetectMs_ = now;
            if (!ops_.checkPower()) {
                enterPhase(Phase::NoPower);
                break;
            }
            // Walk the SOG slicer 6..2 between attempts so a marginal sync gets several chances.
            const uint8_t sog = static_cast<uint8_t>(chip_.get(ADC_SOGCTRL));
            lock_.sogLevel = sog >= 3 ? sog - 1 : 6;
            chip_.write(ADC_SOGCTRL, lock_.sogLevel);
            startDetect();
        }
        break;
    case Phase::DetectFlipWait:
        if (inPhaseMs() >= kFlipSettleMs) {
            enterPhase(Phase::DetectIdle);
        }
        break;
    case Phase::DetectRgbVsync:
        if (chip_.get(STATUS_SYNC_PROC_VSACT)) {
            chip_.write(MD_SEL_VGA60, 1); // VGA 640x480 likelier than EDTV
            enterPhase(Phase::DetectRgbHsync);
        } else if (inPhaseMs() >= kVsyncWaitMs) {
            // The VS-active flag stays low for some HV sources; judge by the line count instead.
            vsyncProber_.start(ops_, now);
            enterPhase(Phase::DetectVsyncProbe);
        }
        break;
    case Phase::DetectVsyncProbe:
        if (!vsyncProber_.step(ops_, now)) {
            if (vsyncProber_.result().found) {
                chip_.write(MD_SEL_VGA60, 1);
                enterPhase(Phase::DetectRgbHsync);
            } else {
                ctx_.syncCsync = true; // RGBS: composite sync or sync on green
                chip_.write(MD_SEL_VGA60, 0);
                enterPhase(Phase::DetectWalkRgbs);
            }
        }
        break;
    case Phase::DetectRgbHsync:
        if (chip_.get(STATUS_SYNC_PROC_HSACT)) {
            chip_.write(SP_H_PROTECT, 1); // needed for csync with a vertical present too
            enterPhase(Phase::DetectRgbHsyncSettle);
        } else if (inPhaseMs() >= kHsyncWaitMs) {
            chip_.write(SP_SOG_MODE, 1);
            ops_.resetSyncProcessor();
            ops_.resetModeDetect();
            enterPhase(Phase::DetectLostSettle);
        }
        break;
    case Phase::DetectRgbHsyncSettle:
        if (inPhaseMs() >= kHsyncSettleMs) {
            // Vertical present and H present: separate HV sync, or csync on the HS pin?
            uint8_t decoded = 0;
            for (uint8_t i = 0; i < 3; ++i) {
                if (sense_.sourceFieldRateMilliHz(true, true) > 40'000)
                    ++decoded;
            }
            ctx_.syncCsync = decoded >= 2;
            if (ctx_.syncCsync) {
                chip_.write(SP_PRE_COAST, 0x10);
            }
            medResSteps_ = 0;
            enterPhase(Phase::DetectMedRes);
        }
        break;
    case Phase::DetectMedRes:
        if (sense_.videoMode(Standard::None, false) == Standard::MedRes) {
            ops_.setSogLevel(lock_.sogLevel);
            ctx_.medResLineCount = static_cast<uint8_t>(chip_.get(MD_HD1250P_CNTRL));
            detectFound(false);
        } else if (++medResSteps_ >= 16) {
            // Separate H/V sync that is not a known TV mode: an RGBHV computer source, passed
            // through (experimental, video/bypass.h).
            detectFound(false);
            beginLoad(Standard::RgbhvBypass);
        } else {
            chip_.write(MD_HD1250P_CNTRL, chip_.get(MD_HD1250P_CNTRL) + 1);
        }
        break;
    case Phase::DetectWalkRgbs:
    case Phase::DetectWalkComponent: {
        const bool rgb = phase_ == Phase::DetectWalkRgbs;
        for (uint8_t i = 0; i < 10; ++i) {
            sleepMs(2);
            const Standard mode = sense_.videoMode(Standard::None, !rgb);
            if (mode != Standard::None && (!rgb || mode != Standard::MedRes)) {
                detectFound(!rgb);
                return;
            }
            ++pollCount_;
            if (pollCount_ % (rgb ? 150 : 180) == 0) {
                uint8_t sog = lock_.sogLevel == 1 ? 2 : lock_.sogLevel + 2;
                if (sog >= (rgb ? 15 : 16))
                    sog = 1;
                ops_.setSogLevel(sog);
                if (!rgb)
                    ctx_.maxSogLevel = sog;
            }
            if (rgb) {
                if (mode == Standard::MedRes) {
                    ctx_.maxSogLevel = 13;
                    ops_.setSogLevel(13);
                    ctx_.medResLineCount = static_cast<uint8_t>(chip_.get(MD_HD1250P_CNTRL));
                    detectFound(false);
                    return;
                }
                const uint32_t line = chip_.get(MD_HD1250P_CNTRL);
                chip_.write(MD_HD1250P_CNTRL, line < 0x3C ? line + 1 : 0x33);
            }
        }
        if (inPhaseMs() >= kWalkWindowMs) {
            if (!rgb) {
                ctx_.maxSogLevel = 14;
                ops_.setSogLevel(14);
            }
            detectFound(!rgb); // let the watcher deal with it
        }
        break;
    }
    case Phase::DetectLostSettle:
        if (inPhaseMs() >= 40) {
            ops_.setSogLevel(2);
            const uint32_t input = chip_.get(ADC_INPUT_SEL);
            if (inputPref_ == InputPreference::Auto)
                chip_.write(ADC_INPUT_SEL, input ? 0 : 1);
            enterPhase(Phase::DetectFlipWait);
        }
        break;
    case Phase::Watching:
    case Phase::Running:
        if (reloadRequested_ && phase_ == Phase::Running && ctx_.standard != Standard::None) {
            reloadRequested_ = false;
            hdmiSyncFix_ = true; // a format change for the display: let the HDMI converter re-lock
            beginLoad(ctx_.standard);
            break;
        }
        watch();
        if (phase_ == Phase::Running || phase_ == Phase::Watching) {
            loopLevel();
        }
        break;
    case Phase::LoadPrewait:
        if (inPhaseMs() >= kLoadPrewaitMs) {
            enterPhase(Phase::Load);
        }
        break;
    case Phase::Load:
        runLoad();
        break;
    case Phase::SettleSog:
        if (!sogOpt_.step(ops_, ctx_)) {
            enterPhase(Phase::SettleSync);
        }
        break;
    case Phase::SettleSync:
        updateSpDynamic(ops_, sense_, ctx_, false);
        if (sense_.hsStable(ctx_.standard) || inPhaseMs() >= kSettleSyncMs) {
            const uint32_t spent = inPhaseMs();
            enterPhase(Phase::SettleMode);
            phaseStartMs_ -= spent; // the mode wait shares the sync wait's clock
        }
        break;
    case Phase::SettleMode: {
        updateSpDynamic(ops_, sense_, ctx_, false);
        const bool named = sense_.videoMode(ctx_.standard, ctx_.inputIsYpbpr) != Standard::None;
        if (named || inPhaseMs() >= kSettleModeMs) {
            if (inPhaseMs() >= 1500 && lock_.sogLevel >= 7) {
                sogOpt_.start(ops_, ctx_);
                enterPhase(Phase::SettleSogWait);
            } else {
                clampTries_ = 0;
                enterPhase(Phase::SettleClamp);
            }
        }
        break;
    }
    case Phase::SettleSogWait:
        if (sogOpt_.busy()) {
            sogOpt_.step(ops_, ctx_);
            phaseStartMs_ = nowMs();
        } else if (inPhaseMs() >= kSogSettleMs) {
            clampTries_ = 0;
            enterPhase(Phase::SettleClamp);
        }
        break;
    case Phase::SettleClamp:
        updateClampPosition(ops_, sense_, ctx_);
        if (ctx_.clampSet || ++clampTries_ >= kClampTries) {
            finishSettle();
        }
        break;
    case Phase::NoPower:
        break;
    }
    refreshStatus();
}

// ------------------------------------------------------------------------------------------------
// Detector

void Engine::startDetect() {
    sleepMs(10);
    const uint32_t input = chip_.get(ADC_INPUT_SEL);
    // A chosen input is the only one searched, even when the other one carries a signal.
    const bool wrongInput = (inputPref_ == InputPreference::Rgb && input != 1) ||
                            (inputPref_ == InputPreference::Component && input != 0);
    if (!wrongInput && sense_.hsStable(Standard::None)) {
        if (input == 1) {
            ctx_.inputIsYpbpr = false;
            ops_.setSogLevel(13);
            enterPhase(Phase::DetectRgbVsync);
            return;
        }
        if (input == 0) {
            ctx_.inputIsYpbpr = true;
            chip_.write(MD_SEL_VGA60, 0); // EDTV likelier than VGA
            enterPhase(Phase::DetectWalkComponent);
            return;
        }
    }
    // Nothing on this input: try the other one next time (unless the user fixed the input).
    uint32_t next = input ? 0 : 1;
    if (inputPref_ == InputPreference::Rgb)
        next = 1;
    if (inputPref_ == InputPreference::Component)
        next = 0;
    chip_.write(ADC_INPUT_SEL, next);
    enterPhase(Phase::DetectFlipWait);
}

void Engine::detectFound(bool ypbpr) {
    ctx_.inputIsYpbpr = ypbpr;
    sourceConnected_ = true;
    // A new source starts from the base gain of its input; the first load applies it.
    for (uint8_t c = 0; c < 3; ++c) {
        lock_.adcGain[c] = baseGain_[ypbpr ? 0 : 1][c];
    }
    autoGain_.restart();
    ops_.resetDebugPort();
    if (ypbpr) {
        ops_.applyYuvPatches();
    } else {
        ops_.applyRgbPatches();
    }
    newModeCounter_ = stableCounter_ = syncLossDebounce_ = 0;
    ctx_.noSyncCounter = 0;
    enterPhase(Phase::Watching);
}

// ------------------------------------------------------------------------------------------------
// Loader

OutputMode Engine::effectiveMode(Standard standard) const {
    if (outputPref_ != OutputMode::Auto) {
        return outputPref_;
    }
    // 960 is an exact 4x of 240p and 2x of 480i; PAL's 288p fits 1024 better.
    return isPalFamily(standard) ? OutputMode::R1024p : OutputMode::R960p;
}

tv5725::ProgramId Engine::chooseProgram(Standard standard) const {
    using P = tv5725::ProgramId;
    const bool pal = isPalFamily(standard);
    switch (effectiveMode(standard)) {
    case OutputMode::R1080p:
        return pal ? P::Pal1080p : P::Ntsc1080p;
    case OutputMode::R1024p:
        return pal ? P::Pal1024p : P::Ntsc1024p;
    case OutputMode::R720p:
        return pal ? P::Pal720p : P::Ntsc720p;
    case OutputMode::R480p:
        return pal ? P::Pal576p : P::Ntsc480p;
    case OutputMode::R15kHz:
        return pal ? P::PalDownscale : P::NtscDownscale;
    case OutputMode::R960p:
    default:
        return pal ? P::Pal960p : P::Ntsc960p;
    }
}

void Engine::beginLoad(Standard standard) {
    gainCal_.abort(ops_);
    pendingStandard_ = standard;
    const bool fromNothing =
        ctx_.standard == Standard::None || ctx_.hdBypass || ctx_.standard == Standard::RgbhvBypass;
    if (fromNothing) {
        // Release IF, VDS and DEC early and give the digital resets time, so the first lock after
        // a load does not fail (reference: "min ~300 ms").
        chip_.write(SFTRST_IF_RSTZ, 1);
        chip_.write(SFTRST_VDS_RSTZ, 1);
        chip_.write(SFTRST_DEC_RSTZ, 1);
        enterPhase(Phase::LoadPrewait);
    } else {
        enterPhase(Phase::Load);
    }
}

void Engine::runLoad() {
    const Standard standard = pendingStandard_;
    if (standard == Standard::RgbhvBypass) {
        startRgbhvBypass();
        return;
    }
    // The programs do not scale HD sources; any source passes through on request.
    if (isHd(standard) || outputPref_ == OutputMode::Passthrough) {
        startHdBypass(standard);
        return;
    }
    bypass_ = false;
    reloadRequested_ = false; // this load uses the preferences in effect
    const tv5725::ProgramId program = chooseProgram(standard);
    const tv5725::ProgramInfo info = tv5725::programInfo(program);
    ctx_.standard = standard;
    ctx_.presetId = info.presetId;
    ctx_.hdBypass = false;
    motionAdaptiveActive_ = false;
    maPending_ = 0;
    clock_.unlock();

    LoadContext lc;
    lc.standard = standard;
    lc.inputIsYpbpr = ctx_.inputIsYpbpr;
    lc.syncCsync = ctx_.syncCsync;
    lc.hdmiSyncFix = hdmiSyncFix_;
    lc.medResLineCount = ctx_.medResLineCount;
    ops_.writeProgram(program, lc);
    sense_.forgetVgaTargets();
    const bool wantSog = postLoadTuning(ops_, sense_, ctx_);
    applyPicture(ops_, ctx_.inputIsYpbpr, picture_);
    clock_.resetForProgram(info.displayClock);
    hdmiSyncFix_ = false;
    loadedProgram_ = program;
    geometryBase_ = readGeometryBase(chip_, ctx_);
    applyGeometry();
    status_.loads++;
    status_.lastLoadMs = nowMs();
    status_.outputMode = effectiveMode(standard);
    stableCounter_ = 0;
    newModeCounter_ = 0;
    ctx_.noSyncCounter = 0;
    doneStagePending_ = false;
    if (wantSog) {
        sogOpt_.start(ops_, ctx_);
        enterPhase(Phase::SettleSog);
    } else {
        enterPhase(Phase::SettleSync);
    }
}

void Engine::startHdBypass(Standard standard) {
    reloadRequested_ = false;
    ctx_.standard = standard;
    motionAdaptiveActive_ = false;
    maPending_ = 0;
    clock_.resetForProgram(0x35); // the display clock is not used in bypass
    enterHdBypass(ops_, sense_, ctx_);
    loadedProgram_ = tv5725::ProgramId::Count;
    bypass_ = true;
    geometryBase_ = GeometryBase{};
    status_.geometryValid = false;
    hdmiSyncFix_ = false;
    status_.loads++;
    status_.lastLoadMs = nowMs();
    status_.outputMode = OutputMode::Passthrough;
    stableCounter_ = 0;
    newModeCounter_ = 0;
    ctx_.noSyncCounter = 0;
    doneStagePending_ = false;
    enterPhase(Phase::SettleSync);
}

void Engine::startRgbhvBypass() {
    reloadRequested_ = false;
    motionAdaptiveActive_ = false;
    maPending_ = 0;
    clock_.resetForProgram(0x35);
    enterRgbhvBypass(ops_, ctx_);
    loadedProgram_ = tv5725::ProgramId::Count;
    bypass_ = true;
    geometryBase_ = GeometryBase{};
    status_.geometryValid = false;
    rgbhvBand_ = 0;
    rgbhvUnstable_ = 0;
    lastBandCheckMs_ = nowMs();
    status_.loads++;
    status_.lastLoadMs = nowMs();
    status_.outputMode = OutputMode::Passthrough;
    stableCounter_ = 0;
    ctx_.noSyncCounter = 0;
    doneStagePending_ = false;
    enterPhase(Phase::Running);
}

// RGBHV pass-through supervision, reduced from the community firmware's branch (mechanics §5.6):
// stability by sync status, a return to detection after a long loss, and the H-PLL band every
// 900 ms. Not done yet: scaling RGBHV sources and following the sync polarity.
void Engine::watchRgbhv() {
    const bool csync = ctx_.syncCsync;
    const bool stable = csync ? chip_.get(STATUS_INT_SOG_BAD) == 0 && (chip_.get(STATUS_00) & 0x04)
                              : (chip_.get(STATUS_16) & 0x0A) == 0x0A;
    if (stable) {
        rgbhvUnstable_ = 0;
        if (stableCounter_ < 255) {
            ++stableCounter_;
        }
    } else if (++rgbhvUnstable_ > (csync ? 200 : 300)) {
        goLowPower(); // the computer went away or changed: detect again
        return;
    }
    if (nowMs() - lastBandCheckMs_ >= 900 && chip_.get(STATUS_INT_SOG_BAD) == 0) {
        lastBandCheckMs_ = nowMs();
        const uint8_t band = tuneRgbhvBand(ops_, sense_.pllRate(csync), rgbhvBand_);
        rgbhvBand_ = band != 0 ? band : rgbhvBand_;
        ops_.resetSogBadIrq();
    }
}

void Engine::finishSettle() {
    if (ctx_.clampSet && chip_.get(SP_NO_CLAMP_REG) == 1) {
        chip_.write(SP_NO_CLAMP_REG, 0); // lock the clamp before the picture is visible again
    }
    ops_.setDac(true); // black is black now
    updateSpDynamic(ops_, sense_, ctx_, false);
    ops_.setSogLevel(lock_.sogLevel);
    chip_.write(INTERRUPT_CONTROL_01, 0xFF);
    ops_.clearInterrupts();
    ops_.outputVga(ctx_.inputIsYpbpr);
    ops_.unfreeze();
    doneStagePending_ = true;
    gNotAdoptedSinceMs = nowMs();
    enterPhase(Phase::Running);
}

// ------------------------------------------------------------------------------------------------
// Watcher (mechanics §5)

void Engine::watch() {
    if (ctx_.standard == Standard::RgbhvBypass) {
        watchRgbhv();
        return;
    }
    Standard detected = sense_.videoMode(ctx_.standard, ctx_.inputIsYpbpr);
    const bool hsStable = sense_.hsStable(ctx_.standard);

    if (ctx_.standard == Standard::PcOnYpbpr && detected == Standard::None &&
        chip_.get(STATUS_INT_SOG_BAD) == 0) {
        detected = Standard::PcOnYpbpr; // flaky graphic modes: keep it
    }

    // SOG guard for csync RGB: count "bad HS" events while the source is demonstrably alive and
    // lower the slicer when they pile up (mechanics §5.2).
    if (ctx_.syncCsync && !ctx_.inputIsYpbpr && newModeCounter_ == 0) {
        if (chip_.get(STATUS_INT_SOG_BAD) == 1 || chip_.get(STATUS_INT_SOG_SW) == 1) {
            ops_.resetSogSwitchIrq();
            if (nowMs() - sogWindowStartMs_ > 3000) {
                sogWindowStartMs_ = nowMs();
                badHsActive_ = 0;
            }
        }
        if (nowMs() - sogWindowStartMs_ < 3000) {
            for (uint8_t i = 0; i < 16; ++i) {
                if (chip_.get(STATUS_INT_SOG_BAD) == 1 || chip_.get(STATUS_SYNC_PROC_HSACT) == 0) {
                    ops_.resetSogBadIrq();
                    const uint32_t hlow = ctx_.standard == Standard::None
                                              ? 777
                                              : chip_.get(STATUS_SYNC_PROC_HLOW_LEN);
                    for (uint8_t a = 0; a < 20; ++a) {
                        if (chip_.get(STATUS_SYNC_PROC_HLOW_LEN) != hlow) {
                            ++badHsActive_;
                            break;
                        }
                    }
                }
                if (i % 3 == 0)
                    sleepMs(1);
            }
            if (badHsActive_ >= 17) {
                if (lock_.sogLevel >= 2) {
                    ops_.setSogLevel(lock_.sogLevel - 1);
                    updateSpDynamic(ops_, sense_, ctx_, false);
                    badHsActive_ = 0;
                    lastAdjustInWindow_ = true;
                } else if (badHsActive_ > 40) {
                    sogOpt_.start(ops_, ctx_);
                    badHsActive_ = 0;
                    lastAdjustInWindow_ = true;
                }
                sogWindowStartMs_ = nowMs();
            }
        } else if (lastAdjustInWindow_) {
            lastAdjustInWindow_ = false;
            if (lock_.sogLevel >= 8) {
                ops_.setSogLevel(lock_.sogLevel - 1);
                updateSpDynamic(ops_, sense_, ctx_, false);
                badHsActive_ = 0;
                ctx_.phaseSet = false;
            }
        }
    }

    // Loss of sync, debounced: dark content makes the HS-active flag flicker for a frame.
    const bool rawLoss =
        !hsStable || (detected == Standard::None && ctx_.standard == Standard::None);
    bool genuineLoss = rawLoss;
    const uint8_t s = static_cast<uint8_t>(ctx_.standard);
    if (s >= 1 && s <= 13) {
        syncLossDebounce_ = rawLoss ? (syncLossDebounce_ < 255 ? syncLossDebounce_ + 1 : 255) : 0;
        genuineLoss = syncLossDebounce_ >= 3;
    } else {
        syncLossDebounce_ = 0;
    }

    if (detected != Standard::None) {
        sourceWasClassified_ = true;
    }

    // A long line that overflows the 12-bit H total while HS stays active (CRTC timing change):
    // hold all recovery until the mode returns (mechanics §5.2).
    if (crtcHold_) {
        if (!genuineLoss && detected == Standard::None) {
            return;
        }
        crtcHold_ = false;
        modeUnknownCounter_ = 0;
        if (!genuineLoss) {
            ctx_.phaseSet = false;
        }
    }
    if (!genuineLoss && detected == Standard::None && s > 0 && s < 14 &&
        chip_.get(STATUS_SYNC_PROC_HTOTAL) < 100) {
        if (++modeUnknownCounter_ >= 5) {
            crtcHold_ = true;
            modeUnknownCounter_ = 0;
            return;
        }
    } else if (!crtcHold_) {
        modeUnknownCounter_ = 0;
    }

    if (!genuineLoss) {
        fallbackTried_ = false;
    }
    if (genuineLoss && ctx_.standard != Standard::RgbhvBypass) {
        watchSyncLoss(genuineLoss);
        if (phase_ != Phase::Running && phase_ != Phase::Watching) {
            return; // the loss handling started a load or a teardown
        }
    }

    const bool changed = detected != Standard::None && detected != ctx_.standard;
    if (changed && ctx_.standard != Standard::RgbhvBypass) {
        watchFormatChange(detected);
    } else if (hsStable && ctx_.standard != Standard::RgbhvBypass &&
               ((detected != Standard::None && ctx_.standard == detected) ||
                (ctx_.syncCsync && isSd(ctx_.standard) && !genuineLoss))) {
        watchStable(genuineLoss);
    }
}

void Engine::watchSyncLoss(bool /*genuineLoss*/) {
    gainCal_.abort(ops_);
    uint16_t& noSync = ctx_.noSyncCounter;
    ++noSync;
    stableCounter_ = 0;
    if (noSync == 1) {
        ops_.freeze();
        return;
    }
    ctx_.phaseSet = false;
    if (noSync <= 3 || chip_.get(STATUS_SYNC_PROC_HSACT) == 0) {
        ops_.freeze();
    }
    if (newModeCounter_ == 0 && noSync == 2 && lock_.sogLevel <= 1 && isSd(ctx_.standard)) {
        ops_.setSogLevel(lock_.sogLevel + 1);
    }
    if (noSync == 8) {
        chip_.write(SP_H_CST_ST, 0x10);
        chip_.write(SP_H_CST_SP, 0x100);
        if (isSd(ctx_.standard)) {
            chip_.write(SP_PRE_COAST, 9); // can detect a mode change early
            chip_.write(SP_POST_COAST, 9);
            const uint32_t ignore = chip_.get(SP_H_PULSE_IGNOR);
            if (ignore >= 0x33 && !ctx_.syncCsync) {
                chip_.write(SP_H_PULSE_IGNOR, ignore / 2);
            }
        }
        ctx_.coastSet = false;
    }
    if (noSync % 27 == 0) {
        updateSpDynamic(ops_, sense_, ctx_, true); // must come first to allow SD > HDTV detection
    }
    if (noSync % 32 == 0) {
        if (chip_.get(STATUS_SYNC_PROC_HSACT) == 1) {
            ops_.unfreeze();
        } else {
            ops_.freeze();
        }
    }
    if (ctx_.inputIsYpbpr && noSync == 34) {
        chip_.write(SP_NO_CLAMP_REG, 1);
        ctx_.clampSet = false;
    }
    if (noSync == 38) {
        ops_.nudgeModeDetect();
    }
    if (noSync == 61 || noSync == 62) {
        if (!ops_.checkPower()) {
            enterPhase(Phase::NoPower);
            return;
        }
        noSync = 63;
    }
    // HS present but no mode ever named: load the 480p program once so the PLL can lock and the
    // normal machinery can take over (Atari ST high resolution, for instance).
    if (noSync >= 200 && !fallbackTried_ && ctx_.standard == Standard::None &&
        chip_.get(STATUS_SYNC_PROC_HSACT) == 1) {
        fallbackTried_ = true;
        noSync = 0;
        beginLoad(Standard::Edtv60);
        return;
    }
    if (ctx_.syncCsync && noSync > 47 && noSync % 16 == 0) {
        chip_.write(SP_H_PROTECT, chip_.get(SP_H_PROTECT) ^ 1u);
    }
    if (noSync % 150 == 0) {
        chip_.write(SP_H_COAST, 0);
        chip_.write(SP_H_PROTECT, 0);
        chip_.write(SP_H_CST_ST, 0x10);
        chip_.write(SP_H_CST_SP, 0x100);
        chip_.write(SP_CS_CLP_ST, 32); // neutral clamp
        chip_.write(SP_CS_CLP_SP, 48);
        ctx_.clampSet = false;
        updateSpDynamic(ops_, sense_, ctx_, true);
        ops_.nudgeModeDetect();
        sleepMs(40);
        // Is the source still alive? Its H sync width keeps changing if so.
        uint32_t hlowStart = chip_.get(STATUS_SYNC_PROC_HLOW_LEN);
        if (chip_.get(PLLAD_VCORST) == 1) {
            hlowStart = 777; // PLLAD not running yet: HLOW_LEN reads 0
        }
        bool alive = false;
        for (uint8_t a = 0; a < 128; ++a) {
            if (chip_.get(STATUS_SYNC_PROC_HLOW_LEN) != hlowStart) {
                alive = true;
                break;
            }
        }
        if (alive) {
            sourceGoneProbes_ = 0;
            if (noSync % 450 == 0) {
                ops_.setSogLevel(0); // worst case: unstable, but at least detects
            } else if (!sogOpt_.busy()) {
                sogOpt_.start(ops_, ctx_);
            }
        } else {
            ops_.setSogLevel(5);
            if (sourceGoneProbes_ < 255)
                ++sourceGoneProbes_;
        }
        if (sourceGoneProbes_ >= 2 && chip_.get(DAC_RGBS_PWDNZ) == 1) {
            // Blank instead of showing a frozen frame, and stop sync so the display can sleep.
            ops_.setDac(false);
            ops_.setSyncOut(false);
        }
        ops_.resetSyncProcessor();
        sleepMs(8);
        ops_.resetModeDetect();
        sleepMs(8);
    }
    if (noSync % 413 == 0 && inputPref_ == InputPreference::Auto) {
        const uint32_t input = chip_.get(ADC_INPUT_SEL);
        chip_.write(ADC_INPUT_SEL, input ? 0 : 1);
        sleepMs(40);
        bool found = false;
        for (uint8_t i = 0; i < 21 && !found; ++i) {
            found = sense_.hsStable(Standard::None);
            if (!found)
                sleepMs(10);
        }
        if (found) {
            noSync = kTeardownCount; // new source on the other input: start over
        } else {
            chip_.write(ADC_INPUT_SEL, input);
        }
    }
    newModeCounter_ = 0;
    if (noSync >= kTeardownCount) {
        goLowPower();
    }
}

void Engine::watchFormatChange(Standard detected) {
    if (newModeCounter_ < 255) {
        ++newModeCounter_;
        stableCounter_ = 0;
        if (newModeCounter_ == 3) {
            ops_.freeze();
            chip_.write(SP_H_CST_ST, 0x10);
            chip_.write(SP_H_CST_SP, 0x100);
            ctx_.coastSet = false;
            sleepMs(10);
            if (sense_.videoMode(ctx_.standard, ctx_.inputIsYpbpr) == Standard::None) {
                updateSpDynamic(ops_, sense_, ctx_, true); // NTSC to 480p and back
                sleepMs(40);
            }
        }
    }
    if (newModeCounter_ < 8) {
        return;
    }
    bool stable = true;
    for (uint8_t a = 0; a < 30; ++a) {
        const Standard again = sense_.videoMode(ctx_.standard, ctx_.inputIsYpbpr);
        if (again != detected) {
            stable = false;
            break;
        }
    }
    newModeCounter_ = 0;
    if (!stable) {
        ops_.unfreeze();
        if (ctx_.standard == Standard::None) {
            ctx_.noSyncCounter = 0x05FF; // from standby: return there soon
        }
        return;
    }
    ctx_.syncCsync = chip_.get(SP_SOG_MODE) == 1;
    // Crossing the 60/50 families (or arriving from nothing) changes the output rate: drop DAC and
    // sync during the load so the HDMI converter re-locks cleanly.
    const bool was60 = ctx_.standard == Standard::Ntsc || ctx_.standard == Standard::Edtv60;
    const bool was50 = isPalFamily(ctx_.standard);
    const bool is60 = detected == Standard::Ntsc || detected == Standard::Edtv60;
    const bool is50 = isPalFamily(detected);
    hdmiSyncFix_ = ctx_.standard == Standard::None || (was60 && is50) || (was50 && is60);
    badHsActive_ = 0;
    sogWindowStartMs_ = nowMs();
    beginLoad(detected);
}

void Engine::watchStable(bool /*genuineLoss*/) {
    if (stableCounter_ < 255) {
        ++stableCounter_;
    }
    if (ctx_.noSyncCounter >= 150) {
        ctx_.coastSet = false; // the source was gone a while: re-arm everything
        ctx_.phaseSet = false;
        doFullRestore_ = true;
    }
    ctx_.noSyncCounter = 0;
    newModeCounter_ = 0;
    sourceGoneProbes_ = 0;
    if (stableCounter_ == 1 && !doFullRestore_) {
        ops_.unfreeze();
    }
    if (stableCounter_ == 2) {
        updateSpDynamic(ops_, sense_, ctx_, false);
        if (doFullRestore_) {
            sogOpt_.start(ops_, ctx_);
            doFullRestore_ = false;
        }
        ops_.unfreeze();
        if (chip_.get(DAC_RGBS_PWDNZ) == 0 &&
            (loadedProgram_ != tv5725::ProgramId::Count || bypass_) && !doneStagePending_) {
            ops_.setDac(true); // blanked on source loss: back on with the source
            ops_.setSyncOut(true);
        }
    }
    if (!ctx_.phaseSet && stableCounter_ >= 10 && stableCounter_ < 61 && stableCounter_ % 10 == 0 &&
        !phaseOpt_.busy() && (loadedProgram_ != tv5725::ProgramId::Count || bypass_)) {
        phaseOpt_.start(ops_, ctx_);
    }
    if (stableCounter_ == 160) {
        ops_.resetSogBadIrq();
    }
    if (stableCounter_ == 45) {
        chip_.write(ADC_UNUSED_67, 0);
        ctx_.clampSet = false; // refresh the clamp occasionally
    }
    if (stableCounter_ % 31 == 0) {
        updateSpDynamic(ops_, sense_, ctx_, false);
    }
    if (stableCounter_ >= 3 && isSd(ctx_.standard) && !ctx_.hdBypass && ctx_.noSyncCounter == 0 &&
        loadedProgram_ != tv5725::ProgramId::Count) {
        followInterlace();
    }
}

void Engine::followInterlace() {
    // Interlace is the parity of VPERIOD_IF (even = interlaced); STATUS_IF_INP_INT reads 1 for
    // 240p too. Switching 240p<->480i never reloads a program: only the deinterlacer engages.
    const uint16_t vp = static_cast<uint16_t>(chip_.get(VPERIOD_IF));
    if (vp != lastVPeriod_) {
        interlacedReads_ = progressiveReads_ = 0;
    }
    const uint8_t threshold = ctx_.syncCsync ? 8 : 2; // csync VPERIOD_IF is less reliable
    auto armRetime = [&]() {
        if (ctx_.syncCsync)
            return; // clock recalibration on noisy csync data causes jumps
        if (timingAdjustDelay_ == 0) {
            timingAdjustDelay_ = 11;
            oddEvenWhenArmed_ = vp % 2;
        } else {
            timingAdjustDelay_ = 0;
        }
    };
    if (isInterlacedCount(vp)) {
        progressiveReads_ = 0;
        if (++interlacedReads_ >= threshold) {
            if (!motionAdaptiveActive_) {
                ops_.enableMotionAdaptive(ctx_.standard);
                motionAdaptiveActive_ = true;
                maPending_ = 3; // select the motion-adaptive path ~60 ms later
                armRetime();
            }
            interlacedReads_ = 0;
        }
    } else if (isProgressiveCount(vp)) {
        interlacedReads_ = 0;
        if (++progressiveReads_ >= threshold) {
            if (motionAdaptiveActive_) {
                ops_.disableMotionAdaptive();
                motionAdaptiveActive_ = false;
                maPending_ = 0;
                armRetime();
            }
            progressiveReads_ = 0;
        }
    } else {
        interlacedReads_ = progressiveReads_ = 0;
    }
    lastVPeriod_ = vp;
    if (timingAdjustDelay_ != 0 && vp % 2 == oddEvenWhenArmed_) {
        if (--timingAdjustDelay_ == 0 && clock_.adopted()) {
            lockFrameRate();
        }
    }
}

void Engine::loopLevel() {
    if (maPending_ > 0 && --maPending_ == 0 && motionAdaptiveActive_) {
        chip_.write(MAPDT_VT_SEL_PRGV, 0);
    }
    if (phaseOpt_.busy()) {
        phaseOpt_.step(ops_, ctx_);
    }
    if (sogOpt_.busy()) {
        sogOpt_.step(ops_, ctx_);
    }
    if (loadedProgram_ == tv5725::ProgramId::Count && !bypass_) {
        return;
    }
    const uint8_t s = static_cast<uint8_t>(ctx_.standard);
    if (s != 0 && s <= 14 && !ctx_.coastSet) {
        const bool ready = (stableCounter_ >= 7 &&
                            sense_.videoMode(ctx_.standard, ctx_.inputIsYpbpr) == ctx_.standard) ||
                           (ctx_.syncCsync && isSd(ctx_.standard) &&
                            sense_.hsStable(ctx_.standard) && ctx_.noSyncCounter == 0);
        if (ready && sense_.hsStable(ctx_.standard)) {
            updateCoastPosition(ops_, sense_, ctx_);
            if (ctx_.coastSet && isSd(ctx_.standard)) {
                chip_.write(SP_DIS_SUB_COAST, 0);
                chip_.write(SP_H_PROTECT, 0);
            }
        }
    }
    if (s != 0 && stableCounter_ >= 4 && !ctx_.clampSet) {
        updateClampPosition(ops_, sense_, ctx_);
        if (ctx_.clampSet && chip_.get(SP_NO_CLAMP_REG) == 1) {
            chip_.write(SP_NO_CLAMP_REG, 0);
        }
    }
    // Done stage: output on and the display moved onto the external clock, at a settled moment.
    if (doneStagePending_ && stableCounter_ > 35) {
        ops_.setDac(true);
        ops_.setSyncOut(true);
        if (clock_.present() && frameLockOn_ && !bypass_ &&
            ctx_.standard != Standard::RgbhvScaled) {
            clock_.adopt();
            lockFrameRate();
        } else if (!bypass_) {
            // Rates for the status. Not in bypass: the scaler produces no output frame to time, and
            // waiting for one cost 400 ms (bench 2026-10-13).
            clock_.measure(ctx_.syncCsync);
        }
        doneStagePending_ = false;
        gNotAdoptedSinceMs = nowMs();
    }
    if (gainCal_.busy()) {
        if (!gainCal_.step(ops_) && gainCal_.outcome() == GainCalibrator::Outcome::Done) {
            for (uint8_t c = 0; c < 3; ++c) {
                baseGain_[ctx_.inputIsYpbpr ? 0 : 1][c] = lock_.adcGain[c];
            }
            ++levelsSequence_;
        }
    } else if (autoGainOn_ && stableCounter_ > 90 && ctx_.clampSet && ctx_.noSyncCounter == 0 &&
               !phaseOpt_.busy() && !sogOpt_.busy()) {
        // Only with the line PLL locked to the source and the slicer content (mechanics §4.2).
        const uint32_t htotal = chip_.get(STATUS_SYNC_PROC_HTOTAL);
        const uint32_t pllad = chip_.get(PLLAD_MD);
        if (htotal + 3 > pllad && htotal < pllad + 3 && chip_.get(STATUS_INT_SOG_BAD) == 0) {
            autoGain_.step(ops_);
        }
    }
    // Watchdog: a missed adoption leaves the output free-running ~17 % fast with nothing on
    // screen to show it (video-path lessons §3.6).
    if (!doneStagePending_ && clock_.present() && frameLockOn_ && !bypass_ && stableCounter_ > 45) {
        if (clock_.adopted()) {
            gNotAdoptedSinceMs = nowMs();
        } else if (nowMs() - gNotAdoptedSinceMs > kAdoptWatchdogMs) {
            clock_.adopt();
            lockFrameRate();
            gNotAdoptedSinceMs = nowMs();
        }
    }
}

void Engine::lockFrameRate() {
    PixelClock::SyncResult r = clock_.syncToSource(ctx_.syncCsync);
    if (r == PixelClock::SyncResult::NeedsShorterLine && clock_.requiredHz() != 0) {
        // Shorten the line so the output runs a little faster than the source at the maximum
        // clock (0.2 % margin), then let the generator come down to the exact rate.
        const uint64_t htotal = chip_.get(VDS_HSYNC_RST);
        const uint64_t best = htotal * PixelClock::kMaxHz * 998u /
                              (static_cast<uint64_t>(clock_.requiredHz()) * 1000u);
        if (applyBestHTotal(ops_, sense_, ctx_, static_cast<uint16_t>(best), geometryBase_,
                            geometry_)) {
            if (geometryBase_.valid) {
                status_.geometry =
                    computeGeometry(geometryBase_, geometry_, static_cast<uint16_t>(best));
            }
            r = clock_.syncToSource(ctx_.syncCsync);
        }
    }
    status_.lockResult = static_cast<uint8_t>(r);
}

// ------------------------------------------------------------------------------------------------

void Engine::refreshStatus() {
    const uint32_t now = nowMs();
    EngineStatus& st = status_;
    switch (phase_) {
    case Phase::NoPower:
        st.state = EngineState::NoPower;
        st.headline = Headline::ScalerMissing;
        return;
    case Phase::CalibrateAdc:
        st.state = EngineState::Starting;
        st.headline = Headline::NoSignal;
        break;
    case Phase::DetectIdle:
    case Phase::DetectFlipWait:
    case Phase::DetectRgbVsync:
    case Phase::DetectVsyncProbe:
    case Phase::DetectRgbHsync:
    case Phase::DetectRgbHsyncSettle:
    case Phase::DetectMedRes:
    case Phase::DetectWalkRgbs:
    case Phase::DetectWalkComponent:
    case Phase::DetectLostSettle:
        st.state = EngineState::Searching;
        st.headline = Headline::NoSignal;
        break;
    case Phase::Watching:
        st.state = EngineState::Searching;
        st.headline = Headline::Settling;
        break;
    case Phase::LoadPrewait:
    case Phase::Load:
        st.state = EngineState::Loading;
        st.headline = Headline::Settling;
        break;
    case Phase::SettleSog:
    case Phase::SettleSync:
    case Phase::SettleMode:
    case Phase::SettleSogWait:
    case Phase::SettleClamp:
        st.state = EngineState::Settling;
        st.headline = Headline::Settling;
        break;
    case Phase::Running:
        st.state = bypass_ ? EngineState::Bypass : EngineState::Running;
        if (ctx_.noSyncCounter > 0) {
            st.headline = Headline::SyncUnstable;
        } else if (doneStagePending_) {
            st.headline = Headline::Locking;
        } else {
            st.headline = Headline::Scaling;
        }
        break;
    }
    st.standard = ctx_.standard;
    st.sync =
        !sourceConnected_ ? SyncType::None : (ctx_.syncCsync ? SyncType::Csync : SyncType::Hv);
    st.program = loadedProgram_;
    st.sogLevel = lock_.sogLevel;
    st.phaseSp = lock_.phaseSp;
    st.phaseAdc = lock_.phaseAdc;
    st.stableCounter = stableCounter_;
    st.noSyncCounter = ctx_.noSyncCounter;
    st.deinterlacing = motionAdaptiveActive_;
    st.pixelClockHz = clock_.frequency();
    st.frameLocked = clock_.locked();
    st.sourceMilliHz = clock_.lastSourceMilliHz();
    st.outputMilliHz = clock_.lastOutputMilliHz();
    for (uint8_t i = 0; i < 3; ++i) {
        st.adcOffset[i] = lock_.adcOffset[i];
    }
    for (uint8_t i = 0; i < 3; ++i) {
        st.adcGain[i] = lock_.adcGain[i];
    }
    st.offsetCalibrationFailed = adcCal_.failed();
    st.levelCalibration = static_cast<uint8_t>(gainCal_.outcome());
    st.autoGain = autoGainOn_;
    if (now - gStatusRefreshedMs < kStatusRefreshMs) {
        return;
    }
    gStatusRefreshedMs = now;
    st.input = chip_.get(ADC_INPUT_SEL) == 0 ? InputPort::Component : InputPort::Rgb;
    st.sourceLines = static_cast<uint16_t>(chip_.get(STATUS_SYNC_PROC_VTOTAL));
    st.hPeriodIf = static_cast<uint16_t>(chip_.get(HPERIOD_IF));
    st.vPeriodIf = static_cast<uint16_t>(chip_.get(VPERIOD_IF));
    st.interlaced = isSd(ctx_.standard) && isInterlacedCount(st.vPeriodIf);
    st.clockAdopted = clock_.present() && clock_.adopted();
}

} // namespace gbsr::video
