#pragma once

#include <stdint.h>

#include "video/ops.h"

namespace gbsr::video {

// ADC input levels: black-level calibration at bring-up, the "calibrate levels" action on a white
// screen, and the optional continuous auto gain. All three read the ADC through the decimator's
// test output; how that output behaves is recorded in docs/bench/M1.md ("Reading the ADC levels").

enum class AdcChannel : uint8_t { R, G, B };

// Levels are 7-bit (0..0x7F); 0x7F is the ADC's ceiling.
constexpr uint8_t kLevelCeiling = 0x7F;

// Borrows the debug pin and the test bus to read one channel's level, and restores them when it
// goes out of scope. Red and green are read directly. Blue reads as a colour difference while the
// decimator's matrix is active, so the matrix is bypassed for as long as blue is tapped: the
// picture's colours are wrong for that moment.
class LevelTap {
public:
    LevelTap(Ops& ops, AdcChannel channel);
    ~LevelTap();
    LevelTap(const LevelTap&) = delete;
    LevelTap& operator=(const LevelTap&) = delete;

    uint8_t read(); // 0..kLevelCeiling

private:
    tv5725::Chip& chip_;
    bool bypassedMatrix_;
    uint8_t pin_, bus_, busEn_, sel_, enable_;
};

// A histogram of 7-bit levels.
struct LevelStats {
    uint16_t hist[kLevelCeiling + 1] = {};
    uint16_t count = 0;

    void clear();
    void add(uint8_t level);
    // The highest level that holds at least `permille` of the samples (0 when none does).
    uint8_t top(uint16_t permille) const;
    // Share of the samples in [from, to], in permille.
    uint16_t share(uint8_t from, uint8_t to) const;
};

// Reads `count` samples of one channel into `stats` (for the console and bench work).
void sampleLevels(Ops& ops, AdcChannel channel, uint16_t count, LevelStats& stats);

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

// The "calibrate levels" action. With a white field on screen, finds for each channel the most gain
// that keeps white just under the ADC's ceiling, by bisection over the gain register (a higher
// value is less gain), then checks that the field really was white. Component sources calibrate Y
// (the green channel) and give Pb and Pr the same gain. Results land in LockState::adcGain.
class GainCalibrator {
public:
    enum class Outcome : uint8_t {
        None, // never run
        Running,
        Done,
        NotWhite, // no channel showed a large, bright, uniform area
        TooHot,   // white clips even at the lowest gain
        Aborted,  // the source changed or went away
    };

    // White level aimed for, ~4 % under the ceiling: at 0x7C, 1-2 % of the reads touched 0x7F from
    // noise around the plateau (bench 2026-10-13, docs/bench/M1.md).
    static constexpr uint8_t kWhiteTarget = 0x7A;
    static constexpr uint8_t kMostGain = 0x30;
    static constexpr uint8_t kLeastGain = 0xFF;

    void start(Ops& ops, bool ypbpr);
    // Returns true while still running; each call reads for a bounded slice.
    bool step(Ops& ops);
    // Stops a run and puts back the gains that were in effect before it.
    void abort(Ops& ops);
    bool busy() const { return outcome_ == Outcome::Running; }
    Outcome outcome() const { return outcome_; }

private:
    enum class Stage : uint8_t { Settle, Measure };

    void beginRound(Ops& ops);
    void endRound(Ops& ops);
    void finish(Ops& ops, Outcome outcome);
    bool active(uint8_t c) const { return !ypbpr_ || c == 1; }
    bool nextChannel();

    Outcome outcome_ = Outcome::None;
    Stage stage_ = Stage::Settle;
    bool ypbpr_ = false;
    bool verifying_ = false;
    uint8_t round_ = 0;
    uint8_t channel_ = 0; // being measured in this round
    uint32_t stageStartMs_ = 0;
    uint8_t before_[3] = {};
    uint8_t probe_[3] = {};
    // Bisection per channel: white is above the target at `bad_` and at or below it at `good_`.
    uint8_t bad_[3] = {}, good_[3] = {};
    uint8_t white_[3] = {}; // last measured white level per channel
    uint16_t whiteShare_[3] = {};
    LevelStats stats_;
};

// input.autoGain: while the picture is stable, watches green and red for samples at the ADC
// ceiling and lowers all three gains by 2 when it finds two in a window of twenty (the community
// firmware's rule, with the flag bit masked and red watched too; blue cannot be read without
// disturbing the picture). It only ever lowers; the engine restarts it for each new source.
class AutoGain {
public:
    void restart();
    // One short burst of reads; returns true if it changed the gains.
    bool step(Ops& ops);

private:
    uint32_t lastChangeMs_ = 0;
    bool watchRed_ = false; // alternates between green and red
};

} // namespace gbsr::video
