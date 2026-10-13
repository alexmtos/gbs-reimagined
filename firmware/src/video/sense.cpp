#include "video/sense.h"

#include "chip/fields.gen.h"
#include "platform/time.h"

namespace gbsr::video {

using namespace tv5725::fields;
using platform::sleepMs;

namespace {

constexpr uint32_t kPulseTimeoutMs = 100; // one field at 10 Hz; a missing edge costs this, no more

} // namespace

Standard Sense::videoMode(Standard current, bool inputIsYpbpr) {
    tv5725::Chip& chip = ops_.chip();
    if (static_cast<uint8_t>(current) >= 14) {
        // RGBHV is not classified by the detector: H or V activity keeps the mode.
        return (chip.get(STATUS_16) & 0x0A) ? current : Standard::None;
    }

    uint32_t s00 = chip.get(STATUS_00);
    if ((s00 & 0x07) == 0x07) {
        if (s00 & 0x80) { // standard-definition family
            if (s00 & 0x08)
                return Standard::Ntsc;
            if (s00 & 0x20)
                return Standard::Pal;
            if (s00 & 0x10)
                return Standard::Edtv60;
            if (s00 & 0x40)
                return Standard::Edtv50;
        }
        if (chip.get(STATUS_03) & 0x10)
            return Standard::Hd720p;
        if (current == Standard::Edtv50 && chip.get(STATUS_04) == 0x80)
            return Standard::Edtv50;
    }

    const uint32_t s04 = chip.get(STATUS_04);
    if (s04 & 0x20) {
        // 576p tends to be misread as 1080i (PS2: h 215 v 1249 against a real 1080i's v 1124).
        if ((s04 & 0x61) == 0x61 && chip.get(VPERIOD_IF) < 1160)
            return Standard::Hd1080i;
        if (s04 & 0x10) {
            // The HD2376_1250P flag is reused by the mode-detect section for ~25 kHz sources.
            return (s04 & 0x04) ? Standard::MedRes : Standard::Hd1080p;
        }
    }

    if ((chip.get(STATUS_05) & 0x0C) == 0 && chip.get(STATUS_00) == 0x07) {
        if (chip.get(STATUS_03) & 0x02) {
            return inputIsYpbpr ? Standard::PcOnYpbpr : Standard::RgbhvBypass;
        }
        nudgeVgaTargets();
    }

    s00 = chip.get(STATUS_00);
    bool counted = false;
    if ((s00 & 0x2F) == 0x07 && (chip.get(STATUS_16) & 0x02)) {
        const uint32_t lines = chip.get(STATUS_SYNC_PROC_VTOTAL);
        bool steady = lines != 0;
        for (uint8_t i = 0; i < 2 && steady; ++i) {
            sleepMs(2);
            const uint32_t again = chip.get(STATUS_SYNC_PROC_VTOTAL);
            steady =
                again + 1 >= lines && again <= lines + 1 && (chip.get(STATUS_00) & 0x2F) == 0x07;
        }
        if (steady) {
            if (notRecognized_ < 255)
                ++notRecognized_;
            counted = true;
        }
    }
    if (!counted) {
        notRecognized_ = 0;
    }
    return notRecognized_ == 255 ? Standard::Unknown : Standard::None;
}

void Sense::nudgeVgaTargets() {
    tv5725::Chip& chip = ops_.chip();
    if (!vgaTargetsCaptured_) {
        vgaTargets_[0] = chip.get(MD_XGA_60HZ_CNTRL);
        vgaTargets_[1] = chip.get(MD_XGA_70HZ_CNTRL);
        vgaTargets_[2] = chip.get(MD_XGA_75HZ_CNTRL);
        vgaTargets_[3] = chip.get(MD_XGA_85HZ_CNTRL);
        vgaTargets_[4] = chip.get(MD_SXGA_60HZ_CNTRL);
        vgaTargets_[5] = chip.get(MD_SXGA_75HZ_CNTRL);
        vgaTargets_[6] = chip.get(MD_SXGA_85HZ_CNTRL);
        vgaTargets_[7] = chip.get(MD_SVGA_60HZ_CNTRL);
        vgaTargets_[8] = chip.get(MD_SVGA_75HZ_CNTRL);
        vgaTargets_[9] = chip.get(MD_SVGA_85HZ_CNTRL);
        vgaTargets_[10] = chip.get(MD_VGA_75HZ_CNTRL);
        vgaTargets_[11] = chip.get(MD_VGA_85HZ_CNTRL);
        vgaTargetsCaptured_ = true;
    }
    const uint8_t skew = nextSkew(); // added modulo 256: 254, 255, 0, 1 = -2..+1
    chip.write(MD_XGA_60HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[0] + skew));
    chip.write(MD_XGA_70HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[1] + skew) & 0x7F);
    chip.write(MD_XGA_75HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[2] + skew) & 0x7F);
    chip.write(MD_XGA_85HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[3] + skew) & 0x7F);
    chip.write(MD_SXGA_60HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[4] + skew) & 0x7F);
    chip.write(MD_SXGA_75HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[5] + skew) & 0x7F);
    chip.write(MD_SXGA_85HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[6] + skew) & 0x7F);
    chip.write(MD_SVGA_60HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[7] + skew));
    chip.write(MD_SVGA_75HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[8] + skew));
    chip.write(MD_SVGA_85HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[9] + skew));
    chip.write(MD_VGA_75HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[10] + skew));
    chip.write(MD_VGA_85HZ_CNTRL, static_cast<uint8_t>(vgaTargets_[11] + skew));
}

uint8_t Sense::nextSkew() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<uint8_t>(static_cast<int8_t>(rng_ % 4) - 2);
}

bool Sense::hsStable(Standard current) {
    tv5725::Chip& chip = ops_.chip();
    if (current == Standard::RgbhvBypass) {
        if (chip.get(STATUS_INT_INP_NO_SYNC) == 0) {
            return true;
        }
        ops_.resetNoHsyncIrq();
        return false;
    }
    const uint32_t s16 = chip.get(STATUS_16);
    if (!(s16 & 0x02)) {
        return false;
    }
    return isSd(current) ? !(s16 & 0x01) : true; // 15 kHz sync must be active-low
}

bool Sense::syncPresent() {
    tv5725::Chip& chip = ops_.chip();
    const uint32_t busSel = chip.get(TEST_BUS_SEL);
    const uint32_t spSel = chip.get(TEST_BUS_SP_SEL);
    if (busSel != 0x0A)
        chip.write(TEST_BUS_SEL, 0x0A);
    if (spSel != 0x0F)
        chip.write(TEST_BUS_SP_SEL, 0x0F);
    const uint32_t readout = chip.get(TEST_BUS);
    if (busSel != 0x0A)
        chip.write(TEST_BUS_SEL, busSel);
    if (spSel != 0x0F)
        chip.write(TEST_BUS_SP_SEL, spSel);
    return readout > 0x0180;
}

Sense::VsyncProbe Sense::probeDiscreteVsync() {
    tv5725::Chip& chip = ops_.chip();
    const uint32_t extSync = chip.get(SP_EXT_SYNC_SEL);
    const uint32_t sogMode = chip.get(SP_SOG_MODE);
    chip.write(SP_EXT_SYNC_SEL, 0); // connect the HV input
    chip.write(SP_SOG_MODE, 0);
    sleepMs(260); // the sync processor's counters need this long to settle

    VsyncProbe result;
    result.flaggedByChip = chip.get(STATUS_SYNC_PROC_VSACT) == 1;

    constexpr uint8_t kSamples = 9;
    uint16_t samples[kSamples];
    uint8_t taken = 0;
    for (uint8_t i = 0; i < kSamples; ++i) {
        if (i != 0)
            sleepMs(20);
        samples[taken++] = static_cast<uint16_t>(chip.get(STATUS_SYNC_PROC_VTOTAL));
        // Three zeros = no H lock: nothing to measure, and a vertical source never reads that.
        if (taken == 3 && samples[0] == 0 && samples[1] == 0 && samples[2] == 0)
            break;
    }
    uint16_t sorted[kSamples];
    for (uint8_t i = 0; i < taken; ++i) { // insertion sort, at most nine values
        uint16_t v = samples[i];
        int8_t j = static_cast<int8_t>(i) - 1;
        while (j >= 0 && sorted[j] > v) {
            sorted[j + 1] = sorted[j];
            --j;
        }
        sorted[j + 1] = v;
    }
    result.lines = sorted[taken / 2];
    result.found = result.flaggedByChip;
    if (!result.found && result.lines >= 200 && result.lines <= 1300) {
        uint8_t agreeing = 0;
        for (uint8_t i = 0; i < taken; ++i) {
            if (samples[i] + 2 >= result.lines && samples[i] <= result.lines + 2)
                ++agreeing;
        }
        result.found = agreeing * 3 >= taken * 2;
    }
    chip.write(SP_SOG_MODE, sogMode);
    chip.write(SP_EXT_SYNC_SEL, extSync);
    return result;
}

uint32_t Sense::measureMilliHz() {
    if (timer_ == nullptr)
        return 0;
    uint32_t mhz = timer_->frequencyMilliHz(kPulseTimeoutMs);
    if (mhz == 0) {
        mhz = timer_->frequencyMilliHz(kPulseTimeoutMs);
    }
    if (mhz != 0 && (mhz < kMinFieldMilliHz || mhz > kMaxFieldMilliHz)) {
        mhz = timer_->frequencyMilliHz(kPulseTimeoutMs); // one retry for an out-of-band reading
    }
    return mhz;
}

uint32_t Sense::sourceFieldRateMilliHz(bool useSyncProcessorBus, bool csync) {
    tv5725::Chip& chip = ops_.chip();
    const uint32_t busSel = chip.get(TEST_BUS_SEL);
    const uint32_t spSel = chip.get(TEST_BUS_SP_SEL);
    const uint32_t ifSel = chip.get(IF_TEST_SEL);
    const uint32_t pin = chip.get(PAD_BOUT_EN);
    if (pin != 1)
        chip.write(PAD_BOUT_EN, 1);
    if (ifSel != 3)
        chip.write(IF_TEST_SEL, 3); // the input formatter's averaged frame time
    if (useSyncProcessorBus) {
        chip.write(TEST_BUS_SEL, csync ? 0x0A : 0x00);
        if (spSel != 0x0F)
            chip.write(TEST_BUS_SP_SEL, 0x0F);
    } else {
        chip.write(TEST_BUS_SEL, 0x00);
    }
    const uint32_t mhz = measureMilliHz();
    chip.write(TEST_BUS_SEL, busSel);
    chip.write(PAD_BOUT_EN, pin);
    if (spSel != 0x0F)
        chip.write(TEST_BUS_SP_SEL, spSel);
    if (ifSel != 3)
        chip.write(IF_TEST_SEL, ifSel);
    return mhz;
}

uint32_t Sense::outputFrameRateMilliHz() {
    tv5725::Chip& chip = ops_.chip();
    const uint32_t busSel = chip.get(TEST_BUS_SEL);
    const uint32_t pin = chip.get(PAD_BOUT_EN);
    if (pin != 1)
        chip.write(PAD_BOUT_EN, 1);
    if (busSel != 2)
        chip.write(TEST_BUS_SEL, 2); // VDS test bus: output vertical blank
    const uint32_t mhz = measureMilliHz();
    chip.write(TEST_BUS_SEL, busSel);
    chip.write(PAD_BOUT_EN, pin);
    return mhz;
}

uint32_t Sense::pllRate(bool csync) {
    tv5725::Chip& chip = ops_.chip();
    const uint32_t busSel = chip.get(TEST_BUS_SEL);
    const uint32_t spSel = chip.get(TEST_BUS_SP_SEL);
    const uint32_t pin = chip.get(PAD_BOUT_EN);
    if (busSel != 0x0A)
        chip.write(TEST_BUS_SEL, 0x0A);
    chip.write(TEST_BUS_SP_SEL, csync ? 0x6B : 0x09);
    chip.write(PAD_BOUT_EN, 1);
    platform::sleepUs(200);
    uint32_t rate = 0;
    if (timer_ != nullptr) {
        const uint32_t ticks = timer_->periodTicks(kPulseTimeoutMs);
        rate = ticks ? timer_->ticksPerSecond() / ticks : 0;
    }
    chip.write(PAD_BOUT_EN, pin);
    if (busSel != 0x0A)
        chip.write(TEST_BUS_SEL, busSel);
    chip.write(TEST_BUS_SP_SEL, spSel);
    return rate;
}

} // namespace gbsr::video
