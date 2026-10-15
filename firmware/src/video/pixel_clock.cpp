#include "video/pixel_clock.h"

#include "chip/fields.gen.h"
#include "platform/time.h"
#include "video/raster.h"

namespace gbsr::video {

using namespace tv5725::fields;

namespace {

constexpr uint8_t kExternalClockCode = 0x75; // PLL648_CONTROL_01: display clock from pin 40
constexpr uint32_t kSmoothThresholdHz = 750'000;
constexpr uint32_t kSmoothStepHz = 1'000;

} // namespace

uint32_t PixelClock::frequencyForCode(uint8_t code) {
    const uint32_t hz = displayClockHz(code);
    return hz != 0 ? hz : 81'000'000; // unused (0x35, bypass) or nothing loaded
}

bool PixelClock::setFrequency(uint32_t hz) {
    if (hz < kMinHz)
        hz = kMinHz;
    if (hz > kMaxHz)
        hz = kMaxHz;
    if (si_ == nullptr || !si_->setFrequency(hz)) {
        return false;
    }
    frequencyHz_ = hz;
    return true;
}

void PixelClock::rampTo(uint32_t hz) {
    if (hz < kMinHz)
        hz = kMinHz;
    if (hz > kMaxHz)
        hz = kMaxHz;
    const uint32_t diff = hz > frequencyHz_ ? hz - frequencyHz_ : frequencyHz_ - hz;
    if (diff >= kSmoothThresholdHz || frequencyHz_ == 0) {
        setFrequency(hz);
        return;
    }
    // Small corrections walk in 1 kHz steps so the HDMI converter follows without dropping.
    while (frequencyHz_ != hz) {
        const uint32_t step =
            (hz > frequencyHz_ ? hz - frequencyHz_ : frequencyHz_ - hz) > kSmoothStepHz
                ? kSmoothStepHz
                : (hz > frequencyHz_ ? hz - frequencyHz_ : frequencyHz_ - hz);
        if (!setFrequency(hz > frequencyHz_ ? frequencyHz_ + step : frequencyHz_ - step)) {
            return;
        }
    }
}

void PixelClock::resetForProgram(uint8_t displayClockCode) {
    locked_ = false;
    if (!present()) {
        return;
    }
    si_->setOutputEnabled(false);
    setFrequency(frequencyForCode(displayClockCode));
    ops_.chip().write(PAD_CKIN_ENZ, 0);
}

void PixelClock::adopt() {
    if (!present()) {
        return;
    }
    tv5725::Chip& chip = ops_.chip();
    const uint32_t code = chip.get(PLL648_CONTROL_01);
    if (code == kExternalClockCode || code == 0x35) {
        return; // already external, or the clock is unused (bypass)
    }
    si_->setOutputEnabled(true);
    platform::sleepUs(800);
    chip.write(PLL648_CONTROL_01, kExternalClockCode);
}

void PixelClock::release(uint8_t displayClockCode) {
    locked_ = false;
    if (!present()) {
        return;
    }
    if (adopted()) {
        ops_.chip().write(PLL648_CONTROL_01, displayClockCode);
    }
    si_->setOutputEnabled(false);
    setFrequency(frequencyForCode(displayClockCode));
}

bool PixelClock::adopted() {
    return ops_.chip().get(PLL648_CONTROL_01) == kExternalClockCode;
}

void PixelClock::measure(bool csync) {
    sourceMilliHz_ = sense_.sourceFieldRateMilliHz(false, csync);
    outputMilliHz_ = sense_.outputFrameRateMilliHz();
}

PixelClock::SyncResult PixelClock::syncToSource(bool csync) {
    if (!present() || ops_.chip().get(PAD_CKIN_ENZ) != 0 || !adopted()) {
        return SyncResult::NotReady;
    }
    sourceMilliHz_ = sense_.sourceFieldRateMilliHz(false, csync);
    if (sourceMilliHz_ < kMinFieldMilliHz || sourceMilliHz_ > kMaxFieldMilliHz) {
        return SyncResult::NotReady;
    }
    outputMilliHz_ = sense_.outputFrameRateMilliHz();
    if (outputMilliHz_ < kMinFieldMilliHz || outputMilliHz_ > kMaxFieldMilliHz) {
        return SyncResult::NotReady;
    }
    const uint64_t target = static_cast<uint64_t>(frequencyHz_) * sourceMilliHz_ / outputMilliHz_;
    requiredHz_ = target > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(target);
    if (target > kMaxHz) {
        // A program tuned at the DAC's maximum (960p, 1024p NTSC) cannot follow a source running
        // faster than its nominal rate by raising the clock: the line has to get shorter instead.
        return target <= kMaxHz + kMaxHz / 16 ? SyncResult::NeedsShorterLine
                                              : SyncResult::OutOfRange;
    }
    if (target < kMinHz) {
        return SyncResult::OutOfRange;
    }
    rampTo(static_cast<uint32_t>(target));
    locked_ = true;
    return SyncResult::Locked;
}

} // namespace gbsr::video
