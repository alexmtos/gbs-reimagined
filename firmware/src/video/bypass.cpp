#include "video/bypass.h"

#include "chip/fields.gen.h"
#include "platform/time.h"

namespace gbsr::video {

using namespace tv5725::fields;
using platform::sleepMs;

namespace {

constexpr uint8_t kPresetHdBypass = 0x21;
constexpr uint8_t kPresetRgbhvBypass = 0x22;
constexpr uint8_t kClockUnused = 0x35; // PLL648_CONTROL_01: display clock not used

// Per-standard blanking, sync and sampling of the HD block (values from the community firmware's
// setOutModeHdBypass; mechanics §6.5).
void hdTimingSd(Ops& ops, Standard s) {
    tv5725::Chip& chip = ops.chip();
    // Sync from the sync processor, locked to the falling HS edge.
    chip.write(SP_HS2PLL_INV_REG, 1);
    chip.write(SP_CS_P_SWAP, 1);
    chip.write(SP_HS_PROC_INV_REG, 1);
    chip.write(MD_HS_FLIP, 1);
    chip.write(MD_VS_FLIP, 1);
    chip.write(OUT_SYNC_SEL, 2);
    chip.write(SP_HS_LOOP_SEL, 0);
    chip.write(ADC_FLTR, 3);
    const uint32_t md = chip.get(PLLAD_MD);
    chip.write(HD_HSYNC_RST, md / 2 + 8);
    chip.write(HD_HB_ST, md * 945 / 1000);
    chip.write(HD_HB_SP, 0x90);
    chip.write(HD_HS_ST, 0x80);
    chip.write(HD_HS_SP, 0x00);
    chip.write(SP_CS_HS_ST, 0xA0);
    chip.write(SP_CS_HS_SP, 0x00);
    if (s == Standard::Ntsc) {
        ops.setCsVsStart(250);
        ops.setCsVsStop(1);
        chip.write(HD_VB_ST, 500);
        chip.write(HD_VS_ST, 3);
        chip.write(HD_VS_SP, 522);
        chip.write(HD_VB_SP, 16);
    } else {
        ops.setCsVsStart(301);
        ops.setCsVsStop(5);
        chip.write(HD_VB_ST, 605);
        chip.write(HD_VS_ST, 1);
        chip.write(HD_VS_SP, 621);
        chip.write(HD_VB_SP, 16);
    }
}

void hdTimingEdtv(Ops& ops, Standard s) {
    tv5725::Chip& chip = ops.chip();
    chip.write(ADC_FLTR, 2);
    chip.write(PLLAD_KS, 1);
    chip.write(PLLAD_CKOS, 0);
    chip.write(HD_HB_ST, 0x864);
    chip.write(HD_HB_SP, 0xA0);
    chip.write(HD_VB_ST, 0x00);
    chip.write(HD_VB_SP, 0x40);
    chip.write(HD_VS_ST, 0x06);
    chip.write(HD_VS_SP, 0x00);
    if (s == Standard::Edtv60) {
        chip.write(HD_HS_ST, 0x54);
        chip.write(HD_HS_SP, 0x864);
        ops.setCsVsStart(525 - 5);
        ops.setCsVsStop(525 - 3);
    } else {
        chip.write(HD_HS_ST, 0x10);
        chip.write(HD_HS_SP, 0x880);
        ops.setCsVsStart(48);
        ops.setCsVsStop(46);
    }
}

// 720p and 1080p sample at 1x with the second decimator bypassed.
void hdHighRate(tv5725::Chip& chip) {
    chip.write(PLLAD_KS, 0);
    chip.write(PLLAD_CKOS, 0);
    chip.write(ADC_FLTR, 0);
    chip.write(ADC_CLK_ICLK1X, 0);
    chip.write(DEC2_BYPS, 1);
    chip.write(PLLAD_ICP, 6);
    chip.write(PLLAD_FS, 1);
}

void hdTimingHd(Ops& ops, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    switch (ctx.standard) {
    case Standard::Hd720p:
        chip.write(PLLAD_MD, 2474);
        chip.write(HD_HSYNC_RST, 550);
        hdHighRate(chip);
        chip.write(HD_HB_ST, 0);
        chip.write(HD_HB_SP, 0x140);
        chip.write(HD_HS_ST, 0x20);
        chip.write(HD_HS_SP, 0x80);
        chip.write(HD_VB_ST, 0x00);
        chip.write(HD_VB_SP, 0x6C);
        chip.write(HD_VS_ST, 0x00);
        chip.write(HD_VS_SP, 0x05);
        ops.setCsVsStart(2);
        ops.setCsVsStop(0);
        break;
    case Standard::Hd1080i:
        chip.write(HD_HSYNC_RST, 0x710);
        chip.write(PLLAD_KS, 1);
        chip.write(PLLAD_CKOS, 0);
        chip.write(ADC_FLTR, 1);
        chip.write(HD_HB_ST, 0);
        chip.write(HD_HB_SP, 0xB8);
        chip.write(HD_HS_ST, 0x04);
        chip.write(HD_HS_SP, 0x50);
        chip.write(HD_VB_ST, 0x00);
        chip.write(HD_VB_SP, 0x1E);
        chip.write(HD_VS_ST, 0x04);
        chip.write(HD_VS_SP, 0x09);
        ops.setCsVsStart(8);
        ops.setCsVsStop(6);
        break;
    case Standard::Hd1080p:
        chip.write(PLLAD_MD, 2749);
        chip.write(HD_HSYNC_RST, 0x710);
        hdHighRate(chip);
        chip.write(HD_HB_ST, 0x00);
        chip.write(HD_HB_SP, 0xB0);
        chip.write(HD_HS_ST, 0x20);
        chip.write(HD_HS_SP, 0x70);
        chip.write(HD_VB_ST, 0x00);
        chip.write(HD_VB_SP, 0x2F);
        chip.write(HD_VS_ST, 0x04);
        chip.write(HD_VS_SP, 0x0A);
        break;
    case Standard::PcOnYpbpr: {
        // A graphic mode carried over component: RGB colour handling, composite sync.
        ops.applyRgbPatches();
        ctx.syncCsync = true;
        chip.write(DEC_MATRIX_BYPS, 1);
        chip.write(SP_PRE_COAST, 4);
        chip.write(SP_POST_COAST, 4);
        chip.write(SP_DLT_REG, 0x70);
        chip.write(HD_MATRIX_BYPS, 1);
        chip.write(HD_DYN_BYPS, 1);
        chip.write(SP_VS_PROC_INV_REG, 0);
        chip.write(PLLAD_KS, 0);
        chip.write(PLLAD_CKOS, 0);
        chip.write(ADC_CLK_ICLK1X, 0);
        chip.write(ADC_CLK_ICLK2X, 0);
        chip.write(DEC1_BYPS, 1);
        chip.write(DEC2_BYPS, 1);
        chip.write(PLLAD_MD, 512);
        const uint32_t lines = chip.get(STATUS_SYNC_PROC_VTOTAL);
        chip.write(PLLAD_KS, lines < 532 ? 3 : 2);
        chip.write(PLLAD_FS, lines >= 532 && lines < 810 ? 0 : 1);
        break;
    }
    default:
        break;
    }
}

} // namespace

void enterHdBypass(Ops& ops, Sense& sense, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    LockState& lock = ops.lock();
    ctx.hdBypass = true;
    ctx.presetId = kPresetHdBypass;

    updateSpDynamic(ops, sense, ctx, false);
    ops.loadHdBypassSection();
    chip.write(SP_NO_COAST_REG, 0);
    chip.write(SP_COAST_INV_REG, 0);
    chip.write(ADC_UNUSED_62, 0);
    chip.write(RESET_CONTROL_0x46, 0);
    chip.write(RESET_CONTROL_0x47, 0);
    chip.write(PA_ADC_BYPSZ, 1);
    chip.write(PA_SP_BYPSZ, 1);
    postLoadTuning(ops, sense, ctx); // the common post-load honours ctx.hdBypass
    ops.resetDebugPort();

    chip.write(OUT_SYNC_SEL, 1); // sync from the HD bypass
    chip.write(PLL_CKIS, 0);
    chip.write(PLL_DIVBY2Z, 0);
    chip.write(PAD_OSC_CNTRL, 1); // community note: "wave pattern in 720p source, this fixed it"
    chip.write(PLL648_CONTROL_01, kClockUnused);
    chip.write(PLL648_CONTROL_03, 0);
    chip.write(PLL_LEN, 1);
    chip.write(DAC_RGBS_R0ENZ, 1);
    chip.write(DAC_RGBS_G0ENZ, 1);
    chip.write(DAC_RGBS_B0ENZ, 1);
    chip.write(DAC_RGBS_S1EN, 1);
    chip.write(PAD_TRI_ENZ, 1); // memory bus tri-stated: it is idle, and quieter so
    chip.write(PLL_MS, 2);
    chip.write(MEM_PAD_CLK_INVERT, 0);
    chip.write(RESET_CONTROL_0x47, 0x1F);
    chip.write(DAC_RGBS_BYPS2DAC, 1); // the HD block feeds the DAC
    chip.write(SP_HS_LOOP_SEL, 1);
    chip.write(SP_HS_PROC_INV_REG, 0);
    chip.write(SP_CS_P_SWAP, 0);
    chip.write(SP_HS2PLL_INV_REG, 0);
    chip.write(PB_BYPASS, 1);
    chip.write(PLLAD_MD, 2345);
    chip.write(PLLAD_KS, 2);
    ops.setOverSampleRatio(2, true, ctx.standard, true);
    chip.write(PLLAD_ICP, 5);
    chip.write(PLLAD_FS, 1);

    // YPbPr is converted to RGB in the HD block; RGB passes through.
    chip.write(DEC_MATRIX_BYPS, 1);
    chip.write(HD_MATRIX_BYPS, ctx.inputIsYpbpr ? 0 : 1);
    chip.write(HD_DYN_BYPS, ctx.inputIsYpbpr ? 0 : 1);
    chip.write(HD_SEL_BLK_IN, 0);

    chip.write(SP_SDCS_VSST_REG_H, 0);
    chip.write(SP_SDCS_VSSP_REG_H, 0);
    chip.write(SP_SDCS_VSST_REG_L, 0);
    chip.write(SP_SDCS_VSSP_REG_L, 2);
    chip.write(HD_HSYNC_RST, 0x3FF);
    chip.write(HD_INI_ST, 0);

    if (isSd(ctx.standard)) {
        hdTimingSd(ops, ctx.standard);
    } else if (ctx.standard == Standard::Edtv60 || ctx.standard == Standard::Edtv50) {
        hdTimingEdtv(ops, ctx.standard);
    } else {
        hdTimingHd(ops, ctx);
    }

    chip.write(DEC_IDREG_EN, 1);
    chip.write(DEC_WEN_MODE, 1);
    lock.phaseSp = 8;
    lock.phaseAdc = 24;
    ops.setSogLevel(lock.sogLevel); // latches both phases and the PLLAD
}

void enterRgbhvBypass(Ops& ops, PathContext& ctx) {
    tv5725::Chip& chip = ops.chip();
    LockState& lock = ops.lock();
    ops.setDac(false);
    ops.setSyncOut(false);
    ops.loadHdBypassSection();
    chip.write(ADC_UNUSED_62, 0);
    chip.write(PA_ADC_BYPSZ, 1);
    chip.write(PA_SP_BYPSZ, 1);
    ops.applyRgbPatches();
    ops.resetDebugPort();
    ctx.standard = Standard::RgbhvBypass;
    ctx.hdBypass = false;
    ctx.presetId = kPresetRgbhvBypass;
    ctx.clampSet = false;

    chip.write(PLL_CKIS, 0);
    chip.write(PLL_DIVBY2Z, 0);
    chip.write(PLL_ADS, 0);
    chip.write(PLL_MS, 2);
    chip.write(PAD_TRI_ENZ, 1);
    chip.write(MEM_PAD_CLK_INVERT, 0);
    chip.write(PLL648_CONTROL_01, kClockUnused);
    chip.write(PLL648_CONTROL_03, 0);
    chip.write(PLL_LEN, 1);

    chip.write(DAC_RGBS_ADC2DAC, 1); // the ADC feeds the DAC directly
    chip.write(OUT_SYNC_SEL, 1);
    chip.write(SFTRST_HDBYPS_RSTZ, 1);
    chip.write(HD_INI_ST, 0);
    chip.write(HD_MATRIX_BYPS, 1);
    chip.write(HD_DYN_BYPS, 1);
    chip.write(PAD_SYNC1_IN_ENZ, 0); // sync input filters on
    chip.write(PAD_SYNC2_IN_ENZ, 0);
    chip.write(SP_SOG_P_ATO, 1);
    chip.write(SP_SOG_SRC_SEL, 0);
    chip.write(ADC_SOGEN, 1);
    chip.write(SP_SYNC_BYPS, 0);
    chip.write(SP_HS_LOOP_SEL, 1);
    if (!ctx.syncCsync) {
        chip.write(SP_EXT_SYNC_SEL, 0); // separate H and V
        chip.write(SP_SOG_MODE, 0);
        chip.write(SP_NO_COAST_REG, 1);
        chip.write(SP_PRE_COAST, 0);
        chip.write(SP_POST_COAST, 0);
        chip.write(SP_H_PULSE_IGNOR, 0xFF);
        chip.write(SP_HS_POL_ATO, 1);
        chip.write(SP_VS_POL_ATO, 1);
        chip.write(SP_H_PROTECT, 0);
    } else {
        chip.write(SP_EXT_SYNC_SEL, 1);
        chip.write(SP_SOG_MODE, 1);
        chip.write(SP_NO_COAST_REG, 0);
        chip.write(SP_PRE_COAST, 4);
        chip.write(SP_POST_COAST, 7);
        chip.write(SP_H_PROTECT, 1);
        lock.sogLevel = 24;
    }
    lock.phaseAdc = 16;
    lock.phaseSp = 8;
    chip.write(SP_CLAMP_MANUAL, 1);
    chip.write(SP_COAST_INV_REG, 0);
    chip.write(SP_DIS_SUB_COAST, 1);
    chip.write(SP_HS_PROC_INV_REG, 0);
    chip.write(SP_VS_PROC_INV_REG, 0);
    chip.write(PLLAD_KS, 1);
    ops.setOverSampleRatio(2, true, Standard::RgbhvBypass, false);
    chip.write(DEC_MATRIX_BYPS, 1);
    chip.write(ADC_FLTR, 0); // 150 MHz
    chip.write(PLLAD_ICP, 4);
    chip.write(PLLAD_FS, 0);
    chip.write(PLLAD_MD, 1856); // community note: "allows lower res to detect"
    chip.write(ADC_TA_05_CTRL, 0x02);
    chip.write(ADC_TEST_04, 0x02);
    chip.write(ADC_TEST_0C, 0x12);
    chip.write(DAC_RGBS_R0ENZ, 1);
    chip.write(DAC_RGBS_G0ENZ, 1);
    chip.write(DAC_RGBS_B0ENZ, 1);
    chip.write(OUT_SYNC_CNTRL, 1);

    ops.resetDigital(false);
    ops.resetSyncProcessor();
    sleepMs(2);
    ops.resetSdram();
    sleepMs(2);
    ops.resetPllad();
    ops.togglePhaseUnits();
    sleepMs(20);
    chip.write(PLLAD_LEN, 1);
    ops.setDac(true);
    ops.setSyncOut(true);
    ops.setSogLevel(lock.sogLevel); // latches both phases and the PLLAD
    ops.latchPllad();
    ops.applyAdcLevels();
}

uint8_t tuneRgbhvBand(Ops& ops, uint32_t pllRate, uint8_t currentBand) {
    if (pllRate <= 100 || pllRate >= 7500) {
        return 0;
    }
    const uint8_t band = pllRate < 1030   ? 1
                         : pllRate < 2300 ? 2
                         : pllRate < 3200 ? 3
                         : pllRate < 3800 ? 4
                                          : 5;
    if (band != currentBand) {
        static constexpr uint8_t kKs[6] = {0, 2, 1, 1, 0, 0};
        static constexpr uint8_t kFs[6] = {0, 0, 0, 1, 0, 1};
        tv5725::Chip& chip = ops.chip();
        chip.write(PLLAD_KS, kKs[band]);
        chip.write(PLLAD_FS, kFs[band]);
        chip.write(PLLAD_ICP, 6);
        ops.latchPllad();
    }
    return band;
}

} // namespace gbsr::video
