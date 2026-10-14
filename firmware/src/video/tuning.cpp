#include "video/tuning.h"

#include "chip/fields.gen.h"
#include "platform/time.h"

namespace gbsr::video {

using namespace tv5725::fields;
using platform::nowMs;
using platform::sleepMs;
using platform::sleepUs;

namespace {

bool isDownscale(uint8_t presetId) {
    return presetId == 0x06 || presetId == 0x16;
}

// Horizontal capture window per output program for EDTV sources (mechanics §4.1 step 13):
// {preset id, IF_HB_ST2, IF_HB_SP2}. These were tuned on hardware by the community.
struct CaptureWindow {
    uint8_t presetId;
    uint16_t hbSt2;
    uint16_t hbSp2;
};
constexpr CaptureWindow kEdtv60Windows[] = {
    {0x05, 0x4BC, 0xB0}, {0x03, 0x478, 0x84}, {0x02, 0x478, 0x84},
    {0x01, 0x478, 0x84}, {0x04, 0x478, 0x90},
};
constexpr CaptureWindow kEdtv50Windows[] = {
    {0x15, 0x4C0, 0xC8}, {0x13, 0x478, 0x88}, {0x12, 0x454, 0x88},
    {0x11, 0x454, 0x88}, {0x14, 0x478, 0x90},
};

template <size_t N>
void applyCaptureWindow(tv5725::Chip& chip, const CaptureWindow (&table)[N], uint8_t presetId) {
    for (const CaptureWindow& w : table) {
        if (w.presetId == presetId) {
            chip.write(IF_HB_ST2, w.hbSt2);
            chip.write(IF_HB_SP2, w.hbSp2);
            return;
        }
    }
}

void tuneSd(Ops& ops, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    chip.write(ADC_FLTR, 3); // 40 MHz anti-alias filter
    chip.write(PLLAD_KS, 2);
    ops.setOverSampleRatio(4, true, ctx.standard, ctx.hdBypass);
    chip.write(IF_SEL_WEN, 0);
    if (ctx.inputIsYpbpr) {
        chip.write(IF_HS_TAP11_BYPS, 0);
        chip.write(IF_HS_Y_PDELAY, 2);
        chip.write(VDS_V_DELAY, 0);
        chip.write(VDS_Y_DELAY, 3);
    }
    if (isDownscale(ctx.presetId)) {
        ops.setCsVsStart(2);
        ops.setCsVsStop(0);
        chip.write(IF_VS_SEL, 1);
        chip.write(IF_VS_FLIP, 0);
        chip.write(IF_LD_RAM_BYPS, 0);
        chip.write(IF_HS_DEC_FACTOR, 1);
        chip.write(IF_LD_SEL_PROV, 0);
        chip.write(IF_HB_ST, 2);
        chip.write(MADPT_Y_VSCALE_BYPS, 0);
        chip.write(MADPT_UV_VSCALE_BYPS, 0);
        chip.write(MADPT_PD_RAM_BYPS, 0);
        chip.write(MADPT_VSCALE_DEC_FACTOR, 1);
        chip.write(MADPT_SEL_PHASE_INI, 0);
        if (ctx.standard == Standard::Ntsc) {
            chip.write(IF_HB_ST2, 0x490);
            chip.write(IF_HB_SP2, 0x80);
            chip.write(IF_HB_SP, 0x4A);
            chip.write(IF_HBIN_SP, 0xD0);
        } else {
            chip.write(IF_HB_SP2, 0x74);
            chip.write(IF_HB_SP, 0x50);
            chip.write(IF_HBIN_SP, 0xD0);
        }
    }
}

void tuneEdtvLike(Ops& ops, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    chip.write(ADC_FLTR, 3);
    chip.write(PLLAD_KS, 1);
    if (!isDownscale(ctx.presetId)) {
        ops.setCsVsStart(14);
        ops.setCsVsStop(11);
        chip.write(IF_HB_SP, 0);
    }
    ops.setOverSampleRatio(2, true, ctx.standard, ctx.hdBypass);
    chip.write(IF_HS_DEC_FACTOR, 0);
    chip.write(IF_LD_SEL_PROV, 1);
    chip.write(IF_PRGRSV_CNTRL, 1);
    chip.write(IF_SEL_WEN, 1);
    chip.write(IF_HS_SEL_LPF, 0);
    chip.write(IF_HS_TAP11_BYPS, 0);
    chip.write(IF_HS_Y_PDELAY, 3);
    chip.write(VDS_V_DELAY, 1);
    chip.write(MADPT_Y_DELAY_UV_DELAY, 1);
    chip.write(VDS_Y_DELAY, 3);
    if (ctx.standard == Standard::Unknown && chip.get(STATUS_SYNC_PROC_VTOTAL) > 650) {
        sleepMs(20);
        if (chip.get(STATUS_SYNC_PROC_VTOTAL) > 650) {
            chip.write(PLLAD_KS, 0);
            chip.write(VDS_VSCALE_BYPS, 1);
        }
    }
    if (isDownscale(ctx.presetId)) {
        chip.write(MADPT_Y_VSCALE_BYPS, 0);
        chip.write(MADPT_UV_VSCALE_BYPS, 0);
        chip.write(MADPT_PD_RAM_BYPS, 0);
        chip.write(MADPT_VSCALE_DEC_FACTOR, 1);
        chip.write(MADPT_SEL_PHASE_INI, 1);
        chip.write(MADPT_SEL_PHASE_INI, 0);
    }
}

void tuneByStandard(Ops& ops, Sense& sense, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    if (ctx.standard == Standard::Edtv60 && ctx.presetId != 0x06) {
        ops.setCsVsStart(16);
        ops.setCsVsStop(13);
        chip.write(IF_HB_ST, 30);
        chip.write(IF_HBIN_ST, 0x20);
        chip.write(IF_HBIN_SP, 0x60);
        if (ctx.presetId == 0x03) {
            chip.write(VDS_VSCALE, 683);
        }
        applyCaptureWindow(chip, kEdtv60Windows, ctx.presetId);
    } else if (ctx.standard == Standard::Edtv50 && ctx.presetId != 0x16) {
        chip.write(IF_HBIN_SP, 0x40);
        chip.write(IF_HBIN_ST, 0x20);
        chip.write(IF_HB_ST, 0x30);
        applyCaptureWindow(chip, kEdtv50Windows, ctx.presetId);
    } else if (ctx.standard == Standard::Hd720p) {
        chip.write(ADC_FLTR, 1);
        chip.write(IF_PRGRSV_CNTRL, 1);
        chip.write(IF_HS_DEC_FACTOR, 0);
        chip.write(INPUT_FORMATTER_02, 0x74);
        chip.write(VDS_Y_DELAY, 3);
    } else if (ctx.standard == Standard::Hd1080i || ctx.standard == Standard::Hd1080p) {
        chip.write(ADC_FLTR, 1);
        chip.write(PLLAD_KS, 0);
        chip.write(IF_PRGRSV_CNTRL, 1);
        chip.write(IF_HS_DEC_FACTOR, 0);
        chip.write(INPUT_FORMATTER_02, 0x74);
        chip.write(VDS_Y_DELAY, 3);
    } else if (ctx.standard == Standard::MedRes) {
        uint32_t rate = 0;
        for (uint8_t i = 0; i < 8; ++i) {
            rate += sense.pllRate(ctx.syncCsync);
        }
        rate /= 8;
        if (rate > 200 && rate < 1800) {
            chip.write(PLLAD_FS, 0); // a slow H-PLL wants low gain
        }
        chip.write(PLLAD_ICP, 6);
        chip.write(ADC_FLTR, 1);
        chip.write(IF_HB_ST, 30);
        chip.write(IF_HBIN_SP, 0x60);
        switch (ctx.presetId) {
        case 0x01:
            chip.write(VDS_VSCALE, 410);
            break;
        case 0x02:
            chip.write(VDS_VSCALE, 402);
            break;
        case 0x03:
            chip.write(VDS_VSCALE, 546);
            break;
        case 0x05:
            chip.write(VDS_VSCALE, 400);
            break;
        default:
            break;
        }
    }
}

} // namespace

void fitPalVertical(Ops& ops, const PathContext& ctx) {
    if (ctx.standard != Standard::Pal || ctx.hdBypass) {
        return;
    }
    // Program id → scale for 576 lines (0 = the program's own scale already fits).
    struct Fit {
        uint8_t presetId;
        uint16_t vscale;
    };
    static constexpr Fit kFits[] = {
        {0x15, 546}, // 1920x1080: 1024 × 576 / 1080; was 512 (2×, 540 lines)
        {0x12, 576}, // 1280x1024: 1024 × 576 / 1024; was 512
        {0x11, 614}, // 1280x960: 1024 × 576 / 960; was 585
        {0x13, 0},   // 1280x720: 819 already shows 576 lines; only the capture start moves
        {0x14, 0},   // 576p: 1:1 already; the capture started at 42, cutting the top (M2)
    };
    constexpr uint16_t kFirstActiveLine = 16; // IF_VB_ST; the stop is two lines later
    for (const Fit& f : kFits) {
        if (f.presetId != ctx.presetId) {
            continue;
        }
        tv5725::Chip& chip = ops.chip();
        if (f.vscale != 0) {
            chip.write(VDS_VSCALE, f.vscale);
        }
        chip.write(IF_VB_ST, kFirstActiveLine);
        chip.write(IF_VB_SP, kFirstActiveLine + 2);
        return;
    }
}

bool postLoadTuning(Ops& ops, Sense& sense, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    LockState& lock = ops.lock();

    // The output stays dark until the clamp is locked again: without it, black drifts upward.
    ops.setDac(false);
    chip.write(ADC_UNUSED_64, 0);
    chip.write(ADC_UNUSED_65, 0);
    chip.write(ADC_UNUSED_66, 0);
    chip.write(ADC_UNUSED_67, 0);
    chip.write(PAD_CKIN_ENZ, 0); // clock input (pin 40) enabled for the external generator

    ops.prepareSyncProcessor(ctx.standard, false);
    chip.write(SP_H_PROTECT, 0);
    chip.write(SP_COAST_INV_REG, 0);
    if (!ctx.hdBypass) {
        updateSpDynamic(ops, sense, ctx, false);
    }
    chip.write(SP_NO_CLAMP_REG, 1);
    chip.write(OUT_SYNC_CNTRL, 1);

    chip.write(ADC_AUTO_OFST_PRD, 1); // ADC auto offset prepared (by line), not enabled
    chip.write(ADC_AUTO_OFST_DELAY, 0);
    chip.write(ADC_AUTO_OFST_STEP, 0);
    chip.write(ADC_AUTO_OFST_TEST, 1);
    chip.write(ADC_AUTO_OFST_RANGE_REG, 0x00);

    if (ctx.inputIsYpbpr) {
        ops.applyYuvPatches();
    } else {
        ops.applyRgbPatches();
    }
    if (ctx.hdBypass) {
        chip.write(OUT_SYNC_SEL, 1);
    }

    lock.phaseAdc = static_cast<uint8_t>(chip.get(PA_ADC_S)); // the program's choice
    lock.phaseSp = 8;
    ctx.maxSogLevel = ctx.inputIsYpbpr ? 14 : 13;
    ops.setSogLevel(ctx.maxSogLevel);
    ops.applyAdcLevels();
    chip.write(GPIO_CONTROL_00, 0x67);
    chip.write(GPIO_CONTROL_01, 0x00);
    ctx.clampSet = false;
    ctx.coastSet = false;
    ctx.phaseSet = false;
    ctx.noSyncCounter = 0;

    if (isEdtvLike(ctx.standard)) {
        chip.write(IF_LD_RAM_BYPS, 1); // no line doubler for progressive 31 kHz sources
    }
    chip.write(IF_INI_ST, 0);
    chip.write(IF_HS_INT_LPF_BYPS, 0); // interpolator: smoother scrolling, fewer jailbars
    chip.write(IF_HS_SEL_LPF, 0);
    chip.write(IF_HS_PSHIFT_BYPS, 1);
    chip.write(IF_LD_WRST_SEL, 1); // fixes the output position regardless of IF_HBIN_ST
    chip.write(SP_RT_HS_ST, 0);
    chip.write(SP_RT_HS_SP, chip.get(PLLAD_MD) * 93 / 100);
    chip.write(VDS_PK_LB_CORE, 0);
    chip.write(VDS_PK_LH_CORE, 0);
    chip.write(VDS_PK_LB_CMP, 31); // 0 would cancel peaking entirely
    chip.write(VDS_PK_LH_CMP, 31);
    chip.write(VDS_PK_VL_HL_SEL, 0);
    chip.write(VDS_PK_VL_HH_SEL, 0);
    chip.write(VDS_STEP_GAIN, 1);
    ops.setOverSampleRatio(2, true, ctx.standard, ctx.hdBypass);

    if (isSd(ctx.standard)) {
        tuneSd(ops, ctx);
    }
    if (isEdtvLike(ctx.standard)) {
        tuneEdtvLike(ops, ctx);
    }
    tuneByStandard(ops, sense, ctx);

    ops.resetDebugPort();
    bool wantSogOptimise = false;
    if (ctx.syncCsync && chip.get(TEST_BUS_2F) == 0) {
        sleepMs(4);
        wantSogOptimise = chip.get(TEST_BUS_2F) == 0;
    }
    ops.latchPllad();

    chip.write(ADC_TEST_04, 0x02);
    chip.write(ADC_TEST_0C, 0x12);
    chip.write(ADC_TA_05_CTRL, 0x02);
    chip.write(DEC_TEST_ENABLE, 0);
    chip.write(IF_AUTO_OFST_U_RANGE, 0);
    chip.write(IF_AUTO_OFST_V_RANGE, 0);
    chip.write(IF_AUTO_OFST_PRD, 0);
    chip.write(IF_AUTO_OFST_EN, 0); // not reliable on this chip
    chip.write(VDS_D_RAM_BYPS, 1);  // line filter off (measured as softening only)
    chip.write(VDS_TAP6_BYPS, 0);
    // Step response helps everything but the 1080p programs.
    chip.write(VDS_UV_STEP_BYPS, (ctx.presetId == 0x05 || ctx.presetId == 0x15) ? 1 : 0);
    chip.write(VDS_SYNC_EN, 0); // the chip's own frame lock is not used
    chip.write(VDS_FLOCK_EN, 0);

    updateCoastPosition(ops, sense, ctx);
    ops.resetNoHsyncIrq();
    ops.resetSogBadIrq();

    if (isSd(ctx.standard) && !ctx.hdBypass && chip.get(VPERIOD_IF) == 523) {
        // SNES: fewer total lines and a slight offset.
        chip.write(IF_VB_SP, chip.get(IF_VB_SP) + 4);
        chip.write(IF_VB_ST, chip.get(IF_VB_ST) + 4);
    }
    fitPalVertical(ops, ctx);

    chip.write(VDS_EXT_HB_ST, chip.get(VDS_DIS_HB_ST));
    chip.write(VDS_EXT_HB_SP, chip.get(VDS_DIS_HB_SP));
    chip.write(VDS_EXT_VB_ST, chip.get(VDS_DIS_VB_ST));
    chip.write(VDS_EXT_VB_SP, chip.get(VDS_DIS_VB_SP));
    const uint32_t vtotal = chip.get(VDS_VSYNC_RST);
    chip.write(VDS_VSYN_SIZE1, vtotal + 2);
    chip.write(VDS_VSYN_SIZE2, vtotal + 2);
    chip.write(VDS_FRAME_RST, 4);
    chip.write(VDS_FRAME_NO, 1);
    chip.write(VDS_FR_SELECT, 1);

    ops.resetDigital(ctx.hdBypass);
    ops.resetPllad();
    chip.write(PLLAD_LEN, 1);

    chip.write(VDS_IN_DREG_BYPS, 0);
    chip.write(PLLAD_R, 3);
    chip.write(PLLAD_S, 3);
    chip.write(PLL_R, 1);
    chip.write(PLL_S, 2);
    chip.write(DEC_IDREG_EN, 1);
    chip.write(DEC_WEN_MODE, 1); // keeps the ADC phase consistent (about 4 lock positions)
    chip.write(CAP_SAFE_GUARD_EN, 0);
    chip.write(PB_CUT_REFRESH, 1);
    chip.write(RFF_LREQ_CUT, 0);
    chip.write(CAP_REQ_OVER, 0);
    chip.write(CAP_STATUS_SEL, 1);
    chip.write(PB_REQ_SEL, 3);
    chip.write(RFF_WFF_OFFSET, 0);
    if (ctx.standard == Standard::Edtv60 || ctx.standard == Standard::Edtv50) {
        chip.write(PB_CAP_OFFSET, chip.get(PB_FETCH_NUM) + 4);
    }
    if (!ctx.hdBypass) {
        ops.resetSdram();
    }
    ops.setSogLevel(lock.sogLevel); // cycles the SP and ADPLL latches
    if (!isDownscale(ctx.presetId)) {
        chip.write(IF_VS_SEL, 0); // "VCR" IF sync, needs VS_FLIP
        chip.write(IF_VS_FLIP, 1);
    }
    chip.write(SP_CLP_SRC_SEL, 0);
    chip.write(SP_CS_CLP_ST, 32);
    chip.write(SP_CS_CLP_SP, 48);
    ops.setSyncOut(true);
    chip.write(DAC_RGBS_SPD, 0); // these three: fewer jailbars
    chip.write(DAC_RGBS_S0ENZ, 0);
    chip.write(DAC_RGBS_S1EN, 1);
    chip.write(SP_H_PROTECT, 0);
    if (static_cast<uint8_t>(ctx.standard) >= 5) {
        chip.write(SP_DIS_SUB_COAST, 1);
    }
    if (ctx.syncCsync) {
        chip.write(SP_EXT_SYNC_SEL, 1); // HV input disconnected until the next reset
    }
    ctx.coastSet = false;
    ctx.clampSet = false;
    return wantSogOptimise;
}

void updateSpDynamic(Ops& ops, Sense& sense, PathContext& ctx, bool modeCheck) {
    tv5725::Chip& chip = ops.chip();
    Standard mode = sense.videoMode(ctx.standard, ctx.inputIsYpbpr);
    if (mode == Standard::None) {
        mode = sense.videoMode(ctx.standard, ctx.inputIsYpbpr);
    }
    if (ctx.standard == Standard::None && mode == Standard::None) {
        chip.write(SP_DLT_REG, chip.get(SP_DLT_REG) > 0x30 ? 0x30 : 0xC0);
        return;
    }
    if (mode == Standard::None && modeCheck) {
        // Unknown mode during loss: alternate the DLT so both SD and HDTV can be detected.
        const uint16_t phase = ctx.noSyncCounter % 16;
        if (ctx.noSyncCounter != 0 && phase > 8) {
            chip.write(SP_DLT_REG, 0xC0);
        } else {
            chip.write(SP_DLT_REG, 0x30);
        }
        if (!(ctx.syncCsync && isSd(ctx.standard))) {
            chip.write(SP_H_PULSE_IGNOR, 0x02);
        }
        chip.write(SP_H_CST_ST, 0x10);
        chip.write(SP_H_CST_SP, 0x100);
        chip.write(SP_H_COAST, 0);
        chip.write(SP_H_TIMER_VAL, 0x3A);
        if (ctx.syncCsync) {
            chip.write(SP_COAST_INV_REG, 1);
        }
        ctx.coastSet = false;
        return;
    }
    if (ctx.syncCsync) {
        chip.write(SP_COAST_INV_REG, 0);
    }
    if (ctx.standard == Standard::None) {
        return;
    }
    const uint8_t s = static_cast<uint8_t>(ctx.standard);
    if (s <= 2) {
        chip.write(SP_PRE_COAST, 7);
        chip.write(SP_POST_COAST, 3);
        chip.write(SP_DLT_REG, 0xC0);
        chip.write(SP_H_TIMER_VAL, 0x28);
        if (ctx.syncCsync && !modeCheck) {
            // H pulse ignore from the line period and the sync-width ratio (crtemudriver tests).
            uint32_t hPeriod = chip.get(HPERIOD_IF);
            for (uint8_t i = 0; i < 16 && (hPeriod == 511 || hPeriod < 200); ++i) {
                sleepUs(100);
                hPeriod = chip.get(HPERIOD_IF);
                if (i == 15)
                    hPeriod = 300;
            }
            uint32_t ignore = hPeriod <= 200 ? 0x18 : hPeriod * 81 / 1000;
            uint32_t ratioSum = 0; // ratio in 1/10000
            uint8_t ok = 0;
            for (uint8_t i = 0; i < 30 && ok < 12; ++i) {
                const uint32_t low = chip.get(STATUS_SYNC_PROC_HLOW_LEN);
                const uint32_t total = chip.get(STATUS_SYNC_PROC_HTOTAL) + 1;
                const uint32_t ratio = low * 10000 / total;
                if (ratio > 410 && ratio < 1520) {
                    ratioSum += ratio;
                    ++ok;
                    sleepUs(30);
                }
            }
            const uint32_t ratio = ok == 12 ? ratioSum / 12 : 320;
            ignore += chip.get(PLLAD_MD) * ratio * 38 / 1000000;
            const uint32_t current = chip.get(SP_H_PULSE_IGNOR);
            if (ignore > current || current >= 0x90) {
                if (ignore > 0x90)
                    ignore = 0x90;
                if (ignore >= 0x1A && ignore <= 0x42)
                    ignore = 0x1A;
                if (ignore != current) {
                    chip.write(SP_H_PULSE_IGNOR, ignore);
                    ctx.coastSet = false; // must follow a dot-clock or rate change
                }
            }
        }
    } else if (s <= 4) {
        chip.write(SP_PRE_COAST, 7);
        chip.write(SP_POST_COAST, 6);
        chip.write(SP_DLT_REG, 0xA0);
        chip.write(SP_H_PULSE_IGNOR, 0x0E);
    } else if (s == 5) {
        chip.write(SP_PRE_COAST, 7);
        chip.write(SP_POST_COAST, 7);
        chip.write(SP_DLT_REG, 0x30);
        chip.write(SP_H_PULSE_IGNOR, 0x08);
    } else if (s <= 7) {
        chip.write(SP_PRE_COAST, 9);
        chip.write(SP_POST_COAST, 18);
        chip.write(SP_DLT_REG, 0x70);
        chip.write(SP_H_PULSE_IGNOR, 0x06);
    } else if (s >= 13) {
        if (!ctx.syncCsync) {
            chip.write(SP_PRE_COAST, 0);
            chip.write(SP_POST_COAST, 0);
            chip.write(SP_H_PULSE_IGNOR, 0xFF); // cancel SOG decoding
            chip.write(SP_DLT_REG, 0x00);
        } else {
            chip.write(SP_PRE_COAST, 4);
            chip.write(SP_POST_COAST, 7);
            chip.write(SP_DLT_REG, 0x70);
            chip.write(SP_H_PULSE_IGNOR, 0x02);
        }
    }
}

void updateCoastPosition(Ops& ops, Sense& sense, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    const uint8_t s = static_cast<uint8_t>(ctx.standard);
    if (s == 0 || s > 14) {
        return;
    }
    uint32_t acc = 0;
    uint32_t prev = chip.get(HPERIOD_IF);
    for (uint8_t i = 0; i < 8; ++i) {
        const uint32_t now = chip.get(HPERIOD_IF);
        if (now + 3 <= prev || now >= prev + 3 || !sense.hsStable(ctx.standard)) {
            return; // not steady yet; the watcher retries
        }
        acc += now;
        prev = now;
    }
    acc = acc * 4 / 8; // HPERIOD_IF is a quarter of the line length
    if (acc >= 2040) {
        acc = 1716; // clipped readings in low-res VGA modes: use the likely real value
    }
    if (acc <= 240 && chip.get(STATUS_SYNC_PROC_VTOTAL) <= 322) {
        sleepMs(4);
        if (chip.get(STATUS_SYNC_PROC_VTOTAL) <= 322) {
            acc = 2000; // low resolution at low rate overflows HPERIOD_IF
            if (ctx.syncCsync && s >= 1 && s <= 4 && chip.get(PLLAD_ICP) >= 5 &&
                chip.get(PLLAD_FS) == 1) {
                chip.write(PLLAD_ICP, 5);
                chip.write(PLLAD_FS, 0);
                ops.latchPllad();
                ctx.phaseSet = false;
            }
        }
    }
    if (acc > 32) {
        chip.write(SP_H_CST_ST, 0x10);
        chip.write(SP_H_CST_SP, acc * 968 / 1000);
        chip.write(SP_HCST_AUTO_EN, 0);
        ctx.coastSet = true;
    }
}

void updateClampPosition(Ops& ops, Sense& sense, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    if (ctx.standard == Standard::None) {
        return;
    }
    if (sense.videoMode(ctx.standard, ctx.inputIsYpbpr) == Standard::None) {
        // Some csync SD sources never set the mode bits; their clamp must still be positioned,
        // or the AC-coupled input fades to grey (mechanics §4.4).
        const bool csyncSdStable = ctx.syncCsync && isSd(ctx.standard) &&
                                   sense.hsStable(ctx.standard) && ctx.noSyncCounter == 0;
        if (!csyncSdStable) {
            return;
        }
    }
    chip.write(SP_CLAMP_MANUAL, ctx.inputIsYpbpr ? 0 : 1); // no auto clamp for RGB

    auto lineLength = [&]() {
        return ctx.syncCsync ? chip.get(HPERIOD_IF) : chip.get(STATUS_SYNC_PROC_HTOTAL);
    };
    uint32_t acc = 0;
    uint32_t prev = lineLength();
    for (uint8_t i = 0; i < 16; ++i) {
        const uint32_t now = lineLength();
        if (now + 3 <= prev || now >= prev + 3 || !sense.hsStable(ctx.standard)) {
            return;
        }
        acc += now;
        prev = now;
        sleepUs(100);
    }
    acc /= 16;
    if (acc > 4095) {
        return;
    }
    uint32_t stMul = ctx.syncCsync ? 32 : 10; // per mille of the line
    const uint32_t spMul = ctx.syncCsync ? 174 : 58;
    if (ctx.inputIsYpbpr) {
        stMul = ctx.syncCsync ? 89 : 32; // start after a blacker-than-black sync tip
    }
    uint32_t start = 1 + acc * stMul / 1000;
    uint32_t stop = 2 + acc * spMul / 1000;
    if (ctx.inputIsYpbpr && ctx.hdBypass) {
        if (isSd(ctx.standard)) {
            start += 0x60;
            stop += 0x60;
        }
        chip.write(HD_BLK_GY_DATA, 0x05);
        chip.write(HD_BLK_BU_DATA, 0x00);
        chip.write(HD_BLK_RV_DATA, 0x00);
    }
    const uint32_t oldStart = chip.get(SP_CS_CLP_ST);
    const uint32_t oldStop = chip.get(SP_CS_CLP_SP);
    if (start + 1 < oldStart || start > oldStart + 1 || stop + 1 < oldStop || stop > oldStop + 1) {
        chip.write(SP_CS_CLP_ST, start);
        chip.write(SP_CS_CLP_SP, stop);
    }
    ctx.clampSet = true;
}

// ------------------------------------------------------------------------------------------------

void SogOptimizer::start(Ops& ops, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    running_ = false;
    if (ctx.standard == Standard::RgbhvBypass || chip.get(SP_SOG_MODE) != 1 || !ctx.syncCsync) {
        ctx.maxSogLevel = 13;
        ops.setSogLevel(13);
        return;
    }
    ctx.maxSogLevel = ctx.inputIsYpbpr ? 14 : 13;
    ops.setSogLevel(ctx.maxSogLevel);
    busBackup_ = static_cast<uint8_t>(chip.get(TEST_BUS_SEL));
    spBackup_ = static_cast<uint8_t>(chip.get(TEST_BUS_SP_SEL));
    chip.write(TEST_BUS_SEL, 0x0A);
    chip.write(TEST_BUS_SP_SEL, 0x0F);
    chip.write(TEST_BUS_EN, 1);
    running_ = true;
}

bool SogOptimizer::step(Ops& ops, PathContext& ctx) {
    if (!running_) {
        return false;
    }
    tv5725::Chip& chip = ops.chip();
    LockState& lock = ops.lock();

    // Does HS stay active at this slicer level? Then is vsync decoded and steady?
    uint16_t good = 0;
    const uint32_t t0 = nowMs();
    while (nowMs() - t0 < 60 && good < 60) {
        if (chip.get(STATUS_SYNC_PROC_HSACT) == 1) {
            ++good;
        } else if (good >= 4) {
            good -= 3;
        }
    }
    bool found = false;
    if (good >= 60 && chip.get(TEST_BUS_2F) > 0) {
        sleepMs(20);
        found = true;
        for (uint8_t a = 0; a < 50; ++a) {
            if (chip.get(STATUS_SYNC_PROC_HSACT) == 0 || chip.get(TEST_BUS_2F) == 0) {
                found = false;
                break;
            }
        }
    }
    bool done = found;
    if (!found) {
        if (lock.sogLevel >= 2) {
            ops.setSogLevel(lock.sogLevel - 1);
            sleepMs(8);
        } else {
            ops.setSogLevel(13); // nothing better: back to the default
            sleepMs(8);
            done = true;
        }
    }
    if (done) {
        ctx.maxSogLevel = lock.sogLevel == 0 ? 1 : lock.sogLevel;
        chip.write(TEST_BUS_SEL, busBackup_);
        chip.write(TEST_BUS_SP_SEL, spBackup_);
        running_ = false;
    }
    return running_;
}

// ------------------------------------------------------------------------------------------------

void PhaseOptimizer::start(Ops& ops, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    running_ = false;
    pixelClock_ = static_cast<uint16_t>(chip.get(PLLAD_MD));
    const uint32_t ht = chip.get(STATUS_SYNC_PROC_HTOTAL);
    if (ht + 8 < pixelClock_ || ht > pixelClock_ + 8u) {
        ctx.phaseSet = false; // H-PLL not locked: nothing to measure
        return;
    }
    if (ops.lock().sogLevel <= 2) {
        finish(ops, ctx, false); // too unstable to measure: fixed phases
        return;
    }
    steps_ = bad_ = prevBad_ = prevPrevBad_ = worstBad_ = worstPhase_ = goodSteps_ = 0;
    running_ = true;
}

bool PhaseOptimizer::step(Ops& ops, PathContext& ctx) {
    if (!running_) {
        return false;
    }
    tv5725::Chip& chip = ops.chip();
    LockState& lock = ops.lock();
    for (uint8_t chunk = 0; chunk < 9 && steps_ < 34; ++chunk, ++steps_) {
        lock.phaseSp = (lock.phaseSp + 1) & 0x1F;
        ops.latchPhaseSp();
        bad_ = 0;
        sleepUs(256);
        for (uint8_t i = 0; i < 20; ++i) {
            if (chip.get(STATUS_SYNC_PROC_HTOTAL) != pixelClock_) {
                ++bad_;
                sleepUs(384);
            }
        }
        const uint8_t window = bad_ + prevBad_ + prevPrevBad_; // three-step average
        if (window > worstBad_) {
            worstBad_ = window;
            worstPhase_ = (lock.phaseSp - 1) & 0x1F;
        }
        if (bad_ == 0) {
            ++goodSteps_;
        }
        prevPrevBad_ = prevBad_;
        prevBad_ = bad_;
    }
    if (steps_ < 34) {
        return true;
    }
    if (goodSteps_ < 17) {
        running_ = false;
        ctx.phaseSet = false; // pixel clock unstable: the run is not valid
        return false;
    }
    finish(ops, ctx, worstBad_ != 0);
    return false;
}

void PhaseOptimizer::finish(Ops& ops, PathContext& ctx, bool measured) {
    LockState& lock = ops.lock();
    lock.phaseSp = measured ? static_cast<uint8_t>((worstPhase_ + 16) & 0x1F) : 16;
    lock.phaseAdc = 16;
    const uint8_t s = static_cast<uint8_t>(ctx.standard);
    // Some oversampling ratios need the ADC phase shifted 180 degrees.
    if ((s >= 5 && s <= 7 && lock.osr == 2 && measured) || (s >= 1 && s <= 4 && lock.osr == 4)) {
        lock.phaseAdc = (lock.phaseAdc + 16) & 0x1F;
    }
    ops.latchPhaseSp();
    sleepMs(1);
    ops.latchPhaseAdc();
    ctx.phaseSet = true;
    running_ = false;
}

} // namespace gbsr::video
