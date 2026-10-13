#include "video/adc_levels.h"

#include "chip/fields.gen.h"
#include "platform/time.h"

namespace gbsr::video {

using namespace tv5725::fields;
using platform::nowMs;

namespace {

// Offset calibration
constexpr uint8_t kStartOffset = 0x3D;
constexpr uint8_t kParkedOffset = 0x7F; // the channels not being measured
constexpr uint8_t kCalibrationGain = 0x7F;
constexpr uint8_t kCeilingOffset = 0x52; // reached: the channel never read near zero
constexpr uint8_t kFallbackOffset = 0x40;
constexpr uint16_t kNearZero = 7;
constexpr uint16_t kHitsNeeded = 1500;
constexpr uint8_t kMissesTolerated = 3; // the fourth miss in a row raises the offset
constexpr uint32_t kSelectSettleMs = 20;
constexpr uint32_t kRaiseSettleMs = 10;
constexpr uint32_t kQuietWindowMs = 800; // no raise for this long: the offset is accepted
constexpr uint32_t kSliceMs = 12;

// Measuring order G, R, B; DEC_TEST_SEL picks the channel as 1, 2, 3 in that order.
constexpr tv5725::RwField kOffsetField[3] = {ADC_GOFCTRL, ADC_ROFCTRL, ADC_BOFCTRL};
constexpr uint8_t kLockIndex[3] = {1, 0, 2}; // LockState::adcOffset is R, G, B

// Gain calibration
constexpr tv5725::RwField kGainField[3] = {ADC_RGCTRL, ADC_GGCTRL, ADC_BGCTRL};
constexpr uint32_t kGainSettleMs = 40;
constexpr uint16_t kSamplesPerChannel = 400;
constexpr uint16_t kWhitePermille = 20; // white must hold 2 % of the samples: sparkle is ignored
constexpr uint8_t kWhiteBand = 4;
constexpr uint16_t kWhiteShareMin = 350; // a white field fills most of the line (blanking ~30 %)
constexpr uint8_t kWhiteFloor = GainCalibrator::kWhiteTarget - 8;

// Auto gain
constexpr uint32_t kFastPhaseMs = 30'000;
constexpr uint8_t kFastSamples = 61;
constexpr uint8_t kSlowSamples = 8;
constexpr uint8_t kGainStep = 2;

} // namespace

// ------------------------------------------------------------------------------------------------
// Reading levels

LevelTap::LevelTap(Ops& ops, AdcChannel channel) : chip_(ops.chip()), bypassedMatrix_(false) {
    static constexpr uint8_t kSel[3] = {2, 1, 3}; // DEC_TEST_SEL for R, G, B
    pin_ = static_cast<uint8_t>(chip_.get(PAD_BOUT_EN));
    bus_ = static_cast<uint8_t>(chip_.get(TEST_BUS_SEL));
    busEn_ = static_cast<uint8_t>(chip_.get(TEST_BUS_EN));
    sel_ = static_cast<uint8_t>(chip_.get(DEC_TEST_SEL));
    enable_ = static_cast<uint8_t>(chip_.get(DEC_TEST_ENABLE));
    chip_.write(PAD_BOUT_EN, 0);
    chip_.write(DEC_TEST_ENABLE, 1);
    chip_.write(DEC_TEST_SEL, kSel[static_cast<uint8_t>(channel)]);
    chip_.write(TEST_BUS_SEL, 0x0B); // decimator
    chip_.write(TEST_BUS_EN, 1);
    if (channel == AdcChannel::B && chip_.get(DEC_MATRIX_BYPS) == 0) {
        chip_.write(DEC_MATRIX_BYPS, 1);
        bypassedMatrix_ = true;
    }
}

LevelTap::~LevelTap() {
    if (bypassedMatrix_) {
        chip_.write(DEC_MATRIX_BYPS, 0);
    }
    chip_.write(TEST_BUS_EN, busEn_);
    chip_.write(TEST_BUS_SEL, bus_);
    chip_.write(DEC_TEST_SEL, sel_);
    chip_.write(DEC_TEST_ENABLE, enable_);
    chip_.write(PAD_BOUT_EN, pin_);
}

uint8_t LevelTap::read() {
    // Bit 7 of the high byte is a flag that toggles between reads; bits 0..6 are the level.
    return static_cast<uint8_t>(chip_.get(TEST_BUS_2F) & kLevelCeiling);
}

void LevelStats::clear() {
    for (uint16_t& h : hist) {
        h = 0;
    }
    count = 0;
}

void LevelStats::add(uint8_t level) {
    ++hist[level & kLevelCeiling];
    ++count;
}

uint8_t LevelStats::top(uint16_t permille) const {
    if (count == 0) {
        return 0;
    }
    for (int level = kLevelCeiling; level > 0; --level) {
        if (static_cast<uint32_t>(hist[level]) * 1000 >= static_cast<uint32_t>(permille) * count) {
            return static_cast<uint8_t>(level);
        }
    }
    return 0;
}

uint16_t LevelStats::share(uint8_t from, uint8_t to) const {
    if (count == 0) {
        return 0;
    }
    uint32_t n = 0;
    for (uint16_t level = from; level <= to && level <= kLevelCeiling; ++level) {
        n += hist[level];
    }
    return static_cast<uint16_t>(n * 1000 / count);
}

void sampleLevels(Ops& ops, AdcChannel channel, uint16_t count, LevelStats& stats) {
    stats.clear();
    LevelTap tap(ops, channel);
    for (uint16_t i = 0; i < count; ++i) {
        stats.add(tap.read());
    }
}

// ------------------------------------------------------------------------------------------------
// Black levels

void OffsetCalibrator::start(Ops& ops) {
    tv5725::Chip& chip = ops.chip();
    chip.write(PAD_BOUT_EN, 0);
    chip.write(PLL648_CONTROL_01, 0xA5); // display clock running, 162 MHz
    chip.write(ADC_INPUT_SEL, 2);        // R2/G2/B2: not wired on this board
    chip.write(DEC_MATRIX_BYPS, 1);
    chip.write(DEC_TEST_ENABLE, 1);
    chip.write(ADC_5_03, 0x31); // bottom clamps, 40 MHz filter
    chip.write(ADC_TEST_04, 0x00);
    chip.write(SP_CS_CLP_ST, 0);
    chip.write(SP_CS_CLP_SP, 0);
    chip.write(SP_5_56, 0x05);
    chip.write(SP_5_57, 0x80);
    chip.write(ADC_5_00, 0x02);
    chip.write(TEST_BUS_SEL, 0x0B); // decimator
    chip.write(TEST_BUS_EN, 1);
    ops.resetDigital(false);
    chip.write(ADC_RGCTRL, kCalibrationGain);
    chip.write(ADC_GGCTRL, kCalibrationGain);
    chip.write(ADC_BGCTRL, kCalibrationGain);
    for (const tv5725::RwField& f : kOffsetField) {
        chip.write(f, kParkedOffset);
    }
    failed_ = false;
    select(ops, 0);
}

void OffsetCalibrator::select(Ops& ops, uint8_t index) {
    tv5725::Chip& chip = ops.chip();
    if (index > 0) {
        chip.write(kOffsetField[index - 1], kParkedOffset);
    }
    index_ = index;
    offset_ = kStartOffset;
    chip.write(kOffsetField[index], offset_);
    chip.write(DEC_TEST_SEL, static_cast<uint32_t>(index + 1));
    settle(kSelectSettleMs);
}

void OffsetCalibrator::settle(uint32_t ms) {
    hits_ = 0;
    misses_ = 0;
    stage_ = Stage::Settle;
    stageStartMs_ = nowMs();
    settleMs_ = ms;
}

void OffsetCalibrator::advance(Ops& ops) {
    results_[index_] = offset_;
    if (offset_ >= kCeilingOffset) {
        failed_ = true;
    }
    if (index_ + 1 < 3) {
        select(ops, static_cast<uint8_t>(index_ + 1));
        return;
    }
    LockState& lock = ops.lock();
    for (uint8_t i = 0; i < 3; ++i) {
        lock.adcOffset[kLockIndex[i]] = failed_ ? kFallbackOffset : results_[i];
    }
    stage_ = Stage::Idle;
    ops.applyAdcLevels();
}

bool OffsetCalibrator::step(Ops& ops) {
    if (stage_ == Stage::Idle) {
        return false;
    }
    if (stage_ == Stage::Settle) {
        if (nowMs() - stageStartMs_ < settleMs_) {
            return true;
        }
        stage_ = Stage::Measure;
        stageStartMs_ = nowMs(); // the quiet window starts after each settle
    }
    tv5725::Chip& chip = ops.chip();
    const uint32_t sliceStart = nowMs();
    while (nowMs() - sliceStart < kSliceMs) {
        if (hits_ > kHitsNeeded || nowMs() - stageStartMs_ >= kQuietWindowMs) {
            advance(ops);
            return busy();
        }
        const uint32_t readout = chip.get(TEST_BUS) & 0x7FFF;
        if (readout < kNearZero) {
            ++hits_;
            misses_ = 0;
        } else if (misses_++ >= kMissesTolerated) {
            ++offset_;
            chip.write(kOffsetField[index_], offset_);
            if (offset_ >= kCeilingOffset) {
                advance(ops);
                return busy();
            }
            settle(kRaiseSettleMs);
            return true;
        }
    }
    return true;
}

// ------------------------------------------------------------------------------------------------
// Gain on a white screen

void GainCalibrator::start(Ops& ops, bool ypbpr) {
    LockState& lock = ops.lock();
    for (uint8_t c = 0; c < 3; ++c) {
        before_[c] = lock.adcGain[c];
        bad_[c] = kMostGain;
        good_[c] = kLeastGain;
        white_[c] = 0;
        whiteShare_[c] = 0;
    }
    ypbpr_ = ypbpr;
    verifying_ = false;
    round_ = 0;
    outcome_ = Outcome::Running;
    beginRound(ops);
}

void GainCalibrator::beginRound(Ops& ops) {
    tv5725::Chip& chip = ops.chip();
    for (uint8_t c = 0; c < 3; ++c) {
        if (verifying_) {
            probe_[c] = good_[c];
        } else if (round_ == 0) {
            probe_[c] = kLeastGain; // can white get under the target at all?
        } else if (round_ == 1) {
            probe_[c] = kMostGain; // does white reach the target at all?
        } else if (good_[c] - bad_[c] > 1) {
            probe_[c] = static_cast<uint8_t>((bad_[c] + good_[c]) / 2);
        } else {
            probe_[c] = good_[c];
        }
    }
    for (uint8_t c = 0; c < 3; ++c) {
        // Component: Pb and Pr follow Y.
        chip.write(kGainField[c], ypbpr_ ? probe_[1] : probe_[c]);
    }
    stage_ = Stage::Settle;
    stageStartMs_ = nowMs();
    channel_ = ypbpr_ ? 1 : 0;
    stats_.clear();
}

bool GainCalibrator::nextChannel() {
    for (uint8_t c = static_cast<uint8_t>(channel_ + 1); c < 3; ++c) {
        if (active(c)) {
            channel_ = c;
            stats_.clear();
            return true;
        }
    }
    return false;
}

bool GainCalibrator::step(Ops& ops) {
    if (outcome_ != Outcome::Running) {
        return false;
    }
    if (stage_ == Stage::Settle) {
        if (nowMs() - stageStartMs_ < kGainSettleMs) {
            return true;
        }
        stage_ = Stage::Measure;
    }
    {
        LevelTap tap(ops, static_cast<AdcChannel>(channel_));
        const uint32_t sliceStart = nowMs();
        while (stats_.count < kSamplesPerChannel && nowMs() - sliceStart < kSliceMs) {
            stats_.add(tap.read());
        }
    }
    if (stats_.count < kSamplesPerChannel) {
        return true;
    }
    white_[channel_] = stats_.top(kWhitePermille);
    const uint8_t from = white_[channel_] > kWhiteBand ? white_[channel_] - kWhiteBand : 0;
    whiteShare_[channel_] = stats_.share(from, white_[channel_]);
    if (!nextChannel()) {
        endRound(ops);
    }
    return busy();
}

void GainCalibrator::endRound(Ops& ops) {
    if (verifying_) {
        for (uint8_t c = 0; c < 3; ++c) {
            if (active(c) && (white_[c] < kWhiteFloor || whiteShare_[c] < kWhiteShareMin)) {
                finish(ops, Outcome::NotWhite);
                return;
            }
        }
        finish(ops, Outcome::Done);
        return;
    }
    bool searching = false;
    for (uint8_t c = 0; c < 3; ++c) {
        if (!active(c)) {
            continue;
        }
        const bool under = white_[c] <= kWhiteTarget;
        if (round_ == 0) {
            if (!under) {
                finish(ops, Outcome::TooHot);
                return;
            }
        } else if (round_ == 1) {
            if (under) {
                bad_[c] = good_[c] = kMostGain; // never reaches the target: the check decides
            }
        } else if (good_[c] - bad_[c] > 1) {
            (under ? good_[c] : bad_[c]) = probe_[c];
        }
        searching |= good_[c] - bad_[c] > 1;
    }
    ++round_;
    verifying_ = round_ >= 2 && !searching;
    beginRound(ops);
}

void GainCalibrator::finish(Ops& ops, Outcome outcome) {
    LockState& lock = ops.lock();
    for (uint8_t c = 0; c < 3; ++c) {
        lock.adcGain[c] = outcome == Outcome::Done ? (ypbpr_ ? good_[1] : good_[c]) : before_[c];
    }
    ops.applyAdcLevels();
    outcome_ = outcome;
}

void GainCalibrator::abort(Ops& ops) {
    if (outcome_ == Outcome::Running) {
        finish(ops, Outcome::Aborted);
    }
}

// ------------------------------------------------------------------------------------------------
// Continuous auto gain

void AutoGain::restart() {
    lastChangeMs_ = nowMs();
    watchRed_ = false;
}

bool AutoGain::step(Ops& ops) {
    tv5725::Chip& chip = ops.chip();
    // Fast for 30 s after a start or a change, then a light watch.
    const uint8_t samples = nowMs() - lastChangeMs_ < kFastPhaseMs ? kFastSamples : kSlowSamples;
    const uint32_t mode = chip.get(STATUS_00);
    LevelTap tap(ops, watchRed_ ? AdcChannel::R : AdcChannel::G);
    watchRed_ = !watchRed_;
    uint8_t clipped = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        if (i % 20 == 0) {
            clipped = 0;
        }
        if (tap.read() != kLevelCeiling) {
            continue;
        }
        if (chip.get(STATUS_00) != mode) {
            return false; // the source is changing: not evidence of clipping
        }
        if (++clipped == 2) {
            LockState& lock = ops.lock();
            for (uint8_t& g : lock.adcGain) {
                g = g <= 0xFF - kGainStep ? static_cast<uint8_t>(g + kGainStep) : 0xFF;
            }
            ops.applyAdcLevels();
            lastChangeMs_ = nowMs();
            return true;
        }
    }
    return false;
}

} // namespace gbsr::video
