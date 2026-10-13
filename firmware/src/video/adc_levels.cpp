#include "video/adc_levels.h"

#include "chip/fields.gen.h"
#include "platform/time.h"

namespace gbsr::video {

using namespace tv5725::fields;
using platform::nowMs;

namespace {

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

} // namespace

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

} // namespace gbsr::video
