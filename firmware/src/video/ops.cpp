#include "video/ops.h"

#include "chip/fields.gen.h"
#include "platform/progmem.h"
#include "platform/time.h"

namespace gbsr::video {

using namespace tv5725::fields;
using platform::sleepMs;
using platform::sleepUs;

namespace {

constexpr uint8_t kIdleSogLevel = 5;

} // namespace

// ------------------------------------------------------------------------------------------------
// Whole-chip state

void Ops::zeroAll() {
    chip_.write(RESET_CONTROL_0x46, 0x00);
    chip_.write(RESET_CONTROL_0x47, 0x00);
    const uint8_t zeros[16] = {};
    for (uint8_t seg = 0; seg < tv5725::Chip::kSegments; ++seg) {
        // 0xF0..0xFF is skipped: 0xF0 is the segment select itself.
        for (uint16_t addr = 0; addr < 0xF0; addr += 16) {
            chip_.writeBytes(seg, static_cast<uint8_t>(addr), zeros, sizeof zeros);
        }
    }
}

void Ops::idleBaseline() {
    // Minimum input-formatter timing so the mode detector can see a source.
    chip_.write(IF_VS_SEL, 1);
    chip_.write(IF_VS_FLIP, 1);
    chip_.write(IF_HSYNC_RST, 0x3FF);
    chip_.write(IF_VB_ST, 0);
    chip_.write(IF_VB_SP, 2);

    chip_.write(OUT_SYNC_CNTRL, 0);
    chip_.write(DAC_RGBS_PWDNZ, 0);
    // The ADC stays on: sync-on-green separation needs it to "see" a source at all.
    chip_.write(ADC_TA_05_CTRL, 0x02);
    chip_.write(ADC_TEST_04, 0x02);
    chip_.write(ADC_TEST_0C, 0x12);
    chip_.write(ADC_CLK_PA, 0);
    chip_.write(ADC_SOGEN, 1);
    chip_.write(SP_SOG_MODE, 1);
    chip_.write(ADC_INPUT_SEL, 1);
    chip_.write(ADC_POWDZ, 1);
    setSogLevel(kIdleSogLevel);

    chip_.write(RESET_CONTROL_0x46, 0x00);
    chip_.write(RESET_CONTROL_0x47, 0x00);
    chip_.write(GPIO_CONTROL_00, 0x67);
    chip_.write(GPIO_CONTROL_01, 0x00);
    chip_.write(PLL648_CONTROL_01, 0x00);
    chip_.write(PAD_CKIN_ENZ, 0);
    chip_.write(PAD_CKOUT_ENZ, 1);
    chip_.write(IF_SEL_ADC_SYNC, 1);
    chip_.write(PLLAD_VCORST, 1);
    chip_.write(PLL_ADS, 1);
    chip_.write(PLL_CKIS, 0);
    chip_.write(PLL_MS, 2);
    chip_.write(PAD_CONTROL_00_0x48, 0x2B);
    chip_.write(PAD_CONTROL_01_0x49, 0x1F);
    loadHdBypassSection();
    loadModeDetectSection(LoadContext{});
    applyAdcLevels();

    chip_.write(SP_PRE_COAST, 9);
    chip_.write(SP_POST_COAST, 18);
    chip_.write(SP_NO_COAST_REG, 0);
    chip_.write(SP_CS_CLP_ST, 32);
    chip_.write(SP_CS_CLP_SP, 48);
    chip_.write(SP_SOG_SRC_SEL, 0);
    chip_.write(SP_EXT_SYNC_SEL, 0);
    chip_.write(SP_NO_CLAMP_REG, 1);
    chip_.write(PLLAD_ICP, 0);
    chip_.write(PLLAD_FS, 0);
    chip_.write(PLLAD_5_16, 0x1F);
    chip_.write(PLLAD_MD, 0x700);
    resetPll();
    sleepMs(2);
    resetPllad();
    chip_.write(PLL_VCORST, 1);
    chip_.write(PLLAD_CONTROL_00_5x11, 0x01);
    resetDebugPort();

    // Release only what detection needs: IF and VDS, then DEC, MODE, SYNC and INT.
    chip_.write(RESET_CONTROL_0x46, 0x41);
    chip_.write(RESET_CONTROL_0x47, 0x17);
    chip_.write(INTERRUPT_CONTROL_01, 0xFF);
    clearInterrupts();
    chip_.write(PAD_SYNC_OUT_ENZ, 0);
}

void Ops::prepareSyncProcessor(Standard standard, bool rgbhvBypass) {
    chip_.write(SP_SOG_P_ATO, 0);
    chip_.write(SP_JITTER_SYNC, 0);

    // H and V activity detection thresholds and timers (segment 5, no named fields).
    static const uint8_t kSpBaseline[][2] PROGMEM = {
        {0x21, 0x18}, {0x22, 0x0F}, {0x23, 0x00}, {0x24, 0x40}, {0x25, 0x00},
        {0x26, 0x04}, {0x27, 0x00}, {0x2A, 0x0F}, {0x2D, 0x03}, {0x2E, 0x00},
        {0x2F, 0x02}, {0x31, 0x2F}, {0x33, 0x3A}, {0x34, 0x06},
    };
    for (const auto& entry : kSpBaseline) {
        chip_.writeByte(5, pgm_read_byte(&entry[0]), pgm_read_byte(&entry[1]));
    }

    uint16_t dlt = 0x70;
    if (standard != Standard::None && static_cast<uint8_t>(standard) <= 4) {
        dlt = 0xC0;
    } else if (standard == Standard::Hd720p || standard == Standard::Hd1080i) {
        dlt = 0xA0;
    }
    chip_.write(SP_DLT_REG, dlt);
    chip_.write(SP_H_PULSE_IGNOR, isSd(standard) ? 0x6B : 0x02);
    chip_.write(SP_H_TOTAL_EQ_THD, 3);
    setCsVsStart(4);
    setCsVsStop(1);
    chip_.write(SP_CS_HS_ST, 0x10);
    chip_.write(SP_CS_HS_SP, 0x00);

    static const uint8_t kSpRetime[][2] PROGMEM = {
        {0x49, 0x00}, {0x4A, 0x00}, {0x4B, 0x44}, {0x4C, 0x00},
        {0x51, 0x02}, {0x52, 0x00}, {0x53, 0x00}, {0x54, 0x00},
    };
    for (const auto& entry : kSpRetime) {
        chip_.writeByte(5, pgm_read_byte(&entry[0]), pgm_read_byte(&entry[1]));
    }

    if (!rgbhvBypass) {
        chip_.write(SP_CLAMP_MANUAL, 0);
        chip_.write(SP_CLP_SRC_SEL, 0);
        chip_.write(SP_NO_CLAMP_REG, 1);
        chip_.write(SP_SOG_MODE, 1);
        chip_.write(SP_H_CST_ST, 0x10);
        chip_.write(SP_H_CST_SP, 0x100);
        chip_.write(SP_DIS_SUB_COAST, 0);
        chip_.write(SP_H_PROTECT, 1);
        chip_.write(SP_HCST_AUTO_EN, 0);
        chip_.write(SP_NO_COAST_REG, 0);
    }
    chip_.write(SP_HS_REG, 1);
    chip_.write(SP_HS_PROC_INV_REG, 0);
    chip_.write(SP_VS_PROC_INV_REG, 0);

    static const uint8_t kSpTail[][2] PROGMEM = {
        {0x58, 0x05}, {0x59, 0x00}, {0x5A, 0x01}, {0x5B, 0x00}, {0x5C, 0x03}, {0x5D, 0x02},
    };
    for (const auto& entry : kSpTail) {
        chip_.writeByte(5, pgm_read_byte(&entry[0]), pgm_read_byte(&entry[1]));
    }
}

bool Ops::checkPower() {
    chip_.clearError();
    chip_.write(ADC_UNUSED_69, 0x6A);
    const bool ok = chip_.get(ADC_UNUSED_69) == 0x6A && chip_.ok();
    chip_.write(ADC_UNUSED_69, 0);
    return ok;
}

// ------------------------------------------------------------------------------------------------
// Programs and sections

namespace {

void writeSection(tv5725::Chip& chip, tv5725::ProgramId id) {
    chip.writeProgram(id);
}

} // namespace

void Ops::loadHdBypassSection() {
    writeSection(chip_, tv5725::ProgramId::SectionHdBypass);
}

void Ops::loadModeDetectSection(const LoadContext& ctx) {
    writeSection(chip_, tv5725::ProgramId::SectionModeDetect);
    chip_.write(MD_SEL_VGA60, ctx.syncCsync ? 0 : 1); // csync: EDTV likelier than VGA 640x480
    chip_.write(MD_HD1250P_CNTRL, ctx.medResLineCount);
}

void Ops::loadDeinterlacerSection() {
    writeSection(chip_, tv5725::ProgramId::SectionDeinterlacer);
}

void Ops::writeProgram(tv5725::ProgramId id, const LoadContext& ctx) {
    const tv5725::ProgramInfo info = tv5725::programInfo(id);
    const uint8_t reset46 = static_cast<uint8_t>(chip_.get(RESET_CONTROL_0x46));
    const uint8_t reset47 = static_cast<uint8_t>(chip_.get(RESET_CONTROL_0x47));

    const uint8_t* p = info.records;
    const uint8_t* end = info.records + info.recordsSize;
    uint8_t values[tv5725::Chip::kMaxBurst];
    int16_t lastSeg = -1;

    // Sections and clears that belong between program segments, in the order the chip is used to.
    auto finishSegment = [&](int16_t seg) {
        if (seg == 1) {
            loadModeDetectSection(ctx);
            loadDeinterlacerSection();
        } else if (seg == 3) {
            const uint8_t zeros[16] = {};
            chip_.writeBytes(3, 0x80, zeros, sizeof zeros); // PIP registers, else uninitialised
        }
    };

    while (p < end) {
        const uint8_t seg = pgm_read_byte(p++);
        const uint8_t start = pgm_read_byte(p++);
        const uint8_t count = pgm_read_byte(p++);
        memcpy_P(values, p, count);
        p += count;
        if (seg != lastSeg) {
            if (lastSeg >= 0) {
                finishSegment(lastSeg);
            }
            lastSeg = seg;
        }
        for (uint8_t i = 0; i < count; ++i) {
            const uint8_t addr = static_cast<uint8_t>(start + i);
            uint8_t& v = values[i];
            if (seg == 0) {
                if (addr == 0x44 && ctx.hdmiSyncFix) {
                    v &= static_cast<uint8_t>(~0x01); // keep the DAC off
                } else if (addr == 0x46) {
                    v = reset46; // block resets stay as they are now
                } else if (addr == 0x47) {
                    v = reset47;
                } else if (addr == 0x49 && ctx.hdmiSyncFix) {
                    v |= 0x04; // keep sync out off
                }
            } else if (seg == 1) {
                if (addr == 0x00) {
                    v &= static_cast<uint8_t>(~0x20); // IF_VS_SEL = 0
                } else if (addr == 0x01) {
                    v |= 0x01; // IF_VS_FLIP = 1
                } else if (addr == 0x0C) {
                    v &= 0x0F; // IF_INI_ST low bits
                } else if (addr == 0x0D) {
                    v = 0; // IF_INI_ST high bits
                }
            } else if (seg == 5) {
                if (addr == 0x02) {
                    // input select bit 6: 1 = RGB set, 0 = component set
                    v = ctx.inputIsYpbpr ? static_cast<uint8_t>(v & ~0x40) : (v | 0x40);
                } else if (addr == 0x03) {
                    v &= static_cast<uint8_t>(~0x04); // G clamps to ground
                    if (ctx.inputIsYpbpr) {
                        v |= 0x0A; // R and B (Pr, Pb) clamp to mid level
                    } else {
                        v &= static_cast<uint8_t>(~0x0A);
                    }
                } else if (addr == 0x20) {
                    v = 0x02; // only SOG auto polarity
                } else if (addr == 0x37) {
                    v = isSd(ctx.standard) ? 0x6B : 0x02; // H pulse ignore
                } else if (addr == 0x3E) {
                    v |= 0x20; // SP_DIS_SUB_COAST
                } else if (addr == 0x57) {
                    v |= 0x01; // SP_NO_CLAMP_REG: clamp off until its position is measured
                }
            }
        }
        chip_.writeBytes(seg, start, values, count);
    }
    if (lastSeg >= 0) {
        finishSegment(lastSeg);
    }
}

// ------------------------------------------------------------------------------------------------
// Resets and latches

void Ops::resetDigital(bool hdBypass) {
    const bool keepBypassActive = chip_.get(SFTRST_HDBYPS_RSTZ) == 1;
    chip_.write(RESET_CONTROL_0x47, 0x17);
    if (hdBypass) {
        chip_.write(RESET_CONTROL_0x46, 0x00);
        chip_.write(RESET_CONTROL_0x47, 0x1F);
        return;
    }
    // IF and VDS keep running while the memory side is reset: the producer and the consumer must
    // be live when the capture and playback pointers are released (video-path lessons §1.5).
    chip_.write(RESET_CONTROL_0x46, 0x41);
    if (keepBypassActive) {
        chip_.write(RESET_CONTROL_0x47, 0x1F);
    }
    chip_.write(RESET_CONTROL_0x46, 0x7F);
}

void Ops::resetPllad() {
    chip_.write(PLLAD_VCORST, 1);
    chip_.write(PLLAD_PDZ, 1);
    latchPllad();
    chip_.write(PLLAD_VCORST, 0);
    sleepMs(1);
    latchPllad();
}

void Ops::latchPllad() {
    chip_.write(PLLAD_LAT, 0);
    sleepUs(128);
    chip_.write(PLLAD_LAT, 1);
}

void Ops::resetPll() {
    chip_.write(PLL_VCORST, 1);
    sleepMs(1);
    chip_.write(PLL_VCORST, 0);
    sleepMs(1);
}

void Ops::resetSdram() {
    chip_.write(SDRAM_RESET_CONTROL, 0x02);
    chip_.write(SDRAM_RESET_SIGNAL, 1);
    chip_.write(SDRAM_RESET_SIGNAL, 0);
    chip_.write(SDRAM_RESET_CONTROL, 0x82);
}

void Ops::resetSyncProcessor() {
    chip_.write(SFTRST_SYNC_RSTZ, 0);
    sleepUs(10);
    chip_.write(SFTRST_SYNC_RSTZ, 1);
}

void Ops::resetModeDetect() {
    chip_.write(SFTRST_MODE_RSTZ, 0);
    sleepMs(1);
    chip_.write(SFTRST_MODE_RSTZ, 1);
}

void Ops::nudgeModeDetect() {
    // Toggling the VS flip twice can make the mode detector notice a line-count change.
    const uint32_t flip = chip_.get(MD_VS_FLIP);
    chip_.write(MD_VS_FLIP, flip ^ 1u);
    chip_.write(MD_VS_FLIP, flip);
}

void Ops::resetDebugPort() {
    chip_.write(PAD_BOUT_EN, 1);
    chip_.write(IF_TEST_EN, 1);
    chip_.write(IF_TEST_SEL, 3);
    chip_.write(TEST_BUS_SEL, 0x0A);
    chip_.write(TEST_BUS_EN, 1);
    chip_.write(TEST_BUS_SP_SEL, 0x0F);
    chip_.write(MEM_FF_TOP_FF_SEL, 1);
    chip_.write(VDS_TEST_EN, 1);
}

void Ops::clearInterrupts() {
    chip_.write(INTERRUPT_CONTROL_00, 0xFF);
    chip_.write(INTERRUPT_CONTROL_00, 0x00);
}

void Ops::clearInterruptsExceptSogBad() {
    chip_.write(INTERRUPT_CONTROL_00, 0xFE);
    chip_.write(INTERRUPT_CONTROL_00, 0x00);
}

void Ops::resetSogBadIrq() {
    chip_.write(INT_CONTROL_RST_SOGBAD, 1);
    chip_.write(INT_CONTROL_RST_SOGBAD, 0);
}

void Ops::resetSogSwitchIrq() {
    chip_.write(INT_CONTROL_RST_SOGSWITCH, 1);
    chip_.write(INT_CONTROL_RST_SOGSWITCH, 0);
}

void Ops::resetNoHsyncIrq() {
    chip_.write(INT_CONTROL_RST_NOHSYNC, 1);
    chip_.write(INT_CONTROL_RST_NOHSYNC, 0);
}

// ------------------------------------------------------------------------------------------------
// Sampling

void Ops::setSogLevel(uint8_t level) {
    lock_.sogLevel = level & 0x1F;
    chip_.write(ADC_SOGCTRL, lock_.sogLevel);
    latchPhaseSp();
    latchPhaseAdc();
    latchPllad();
    clearInterrupts();
}

void Ops::latchPhaseSp() {
    chip_.write(PA_SP_LAT, 0);
    chip_.write(PA_SP_S, lock_.phaseSp & 0x1F);
    chip_.write(PA_SP_LAT, 1);
}

void Ops::latchPhaseAdc() {
    chip_.write(PA_ADC_LAT, 0);
    chip_.write(PA_ADC_S, lock_.phaseAdc & 0x1F);
    chip_.write(PA_ADC_LAT, 1);
}

void Ops::togglePhaseUnits() {
    chip_.write(PA_SP_BYPSZ, 0);
    chip_.write(PA_SP_BYPSZ, 1);
    sleepMs(2);
    chip_.write(PA_ADC_BYPSZ, 0);
    chip_.write(PA_ADC_BYPSZ, 1);
    sleepMs(2);
}

void Ops::setOverSampleRatio(uint8_t ratio, bool prepareOnly, Standard standard, bool hdBypass) {
    const uint32_t ks = chip_.get(PLLAD_KS);
    const bool hiRes = standard == Standard::MedRes || standard == Standard::Edtv50 ||
                       standard == Standard::Edtv60;
    // PLLAD_KS is the post-divider; CKOS gives the ADC clock relative to it. A ratio the divider
    // cannot support falls back to 1x.
    if ((ratio == 2 && ks == 0) || (ratio == 4 && ks <= 1)) {
        ratio = 1;
    }
    switch (ratio) {
    case 1:
        chip_.write(PLLAD_CKOS, ks);
        chip_.write(ADC_CLK_ICLK2X, 0);
        chip_.write(ADC_CLK_ICLK1X, 0);
        chip_.write(DEC1_BYPS, 1);
        chip_.write(DEC2_BYPS, 1);
        if (hiRes && !hdBypass) {
            chip_.write(ADC_CLK_ICLK1X, 1);
        }
        break;
    case 2:
        chip_.write(PLLAD_CKOS, ks - 1);
        chip_.write(ADC_CLK_ICLK2X, 0);
        chip_.write(ADC_CLK_ICLK1X, 1);
        chip_.write(DEC2_BYPS, 0);
        chip_.write(DEC1_BYPS, 1);
        if (hiRes && !hdBypass) {
            chip_.write(PLLAD_CKOS, ks); // one more: avoids a 2x-scaled picture for EDTV
        }
        break;
    case 4:
        chip_.write(PLLAD_CKOS, ks - 2);
        chip_.write(ADC_CLK_ICLK2X, 1);
        chip_.write(ADC_CLK_ICLK1X, 1);
        chip_.write(DEC1_BYPS, 0);
        chip_.write(DEC2_BYPS, 0);
        break;
    default:
        return;
    }
    lock_.osr = ratio;
    if (!prepareOnly) {
        latchPllad();
    }
}

void Ops::setCsVsStart(uint16_t start) {
    chip_.write(SP_SDCS_VSST_REG_H, start >> 8);
    chip_.write(SP_SDCS_VSST_REG_L, start & 0xFF);
}

void Ops::setCsVsStop(uint16_t stop) {
    chip_.write(SP_SDCS_VSSP_REG_H, stop >> 8);
    chip_.write(SP_SDCS_VSSP_REG_L, stop & 0xFF);
}

uint16_t Ops::csVsStop() {
    return static_cast<uint16_t>((chip_.get(SP_SDCS_VSSP_REG_H) << 8) |
                                 chip_.get(SP_SDCS_VSSP_REG_L));
}

void Ops::applyAdcLevels() {
    chip_.write(ADC_ROFCTRL, lock_.adcOffset[0]);
    chip_.write(ADC_GOFCTRL, lock_.adcOffset[1]);
    chip_.write(ADC_BOFCTRL, lock_.adcOffset[2]);
    chip_.write(ADC_RGCTRL, lock_.adcGain[0]);
    chip_.write(ADC_GGCTRL, lock_.adcGain[1]);
    chip_.write(ADC_BGCTRL, lock_.adcGain[2]);
}

void Ops::applyRgbPatches() {
    chip_.write(ADC_RYSEL_R, 0); // all three clamp to ground
    chip_.write(ADC_RYSEL_B, 0);
    chip_.write(ADC_RYSEL_G, 0);
    chip_.write(DEC_MATRIX_BYPS, 0); // the decimator converts RGB to YUV
    chip_.write(IF_MATRIX_BYPS, 1);
    // The output colour space follows in applyPicture (video/picture.cpp), which owns it.
}

void Ops::applyYuvPatches() {
    chip_.write(ADC_RYSEL_R, 1); // Pr and Pb clamp to mid level, Y to ground
    chip_.write(ADC_RYSEL_B, 1);
    chip_.write(ADC_RYSEL_G, 0);
    chip_.write(DEC_MATRIX_BYPS, 1);
    chip_.write(IF_MATRIX_BYPS, 1);
}

// ------------------------------------------------------------------------------------------------
// Picture path

void Ops::freeze() {
    chip_.write(CAPTURE_ENABLE, 0);
}

void Ops::unfreeze() {
    chip_.write(CAPTURE_ENABLE, 1);
}

void Ops::outputVga(bool inputIsYpbpr) {
    chip_.write(VDS_SYNC_LEV, 0);
    chip_.write(VDS_CONVT_BYPS, 0);
    chip_.write(OUT_SYNC_CNTRL, 1);
    if (inputIsYpbpr) {
        applyYuvPatches();
    } else {
        applyRgbPatches();
    }
}

void Ops::setDac(bool on) {
    chip_.write(DAC_RGBS_PWDNZ, on ? 1 : 0);
}

void Ops::setSyncOut(bool on) {
    chip_.write(PAD_SYNC_OUT_ENZ, on ? 0 : 1);
}

void Ops::enableMotionAdaptive(Standard standard) {
    freeze();
    chip_.write(DEINT_00, 0x19);
    chip_.write(MADPT_Y_MI_OFFSET, 0x00);
    chip_.write(MADPT_Y_MI_DET_BYPS, 0);
    if (standard == Standard::Ntsc) {
        chip_.write(MADPT_VTAP2_COEFF, 6);
    } else if (standard == Standard::Pal) {
        chip_.write(MADPT_VTAP2_COEFF, 4);
    }
    chip_.write(RFF_ADR_ADD_2, 1);
    chip_.write(RFF_REQ_SEL, 3);
    chip_.write(RFF_FETCH_NUM, 0x80);
    chip_.write(RFF_WFF_OFFSET, 0x100);
    chip_.write(RFF_YUV_DEINTERLACE, 0);
    chip_.write(WFF_FF_STA_INV, 0);
    chip_.write(WFF_ENABLE, 1); // write FIFO before read FIFO
    chip_.write(RFF_ENABLE, 1);
    unfreeze();
    // The caller selects the motion-adaptive path (MAPDT_VT_SEL_PRGV = 0) one tick later, once the
    // FIFOs run (the reference waits 60 ms).
}

void Ops::disableMotionAdaptive() {
    chip_.write(MAPDT_VT_SEL_PRGV, 1);
    chip_.write(DEINT_00, 0xFF);
    chip_.write(RFF_FETCH_NUM, 0x1); // the "RFF disable fix": park the FIFO before stopping it
    chip_.write(RFF_WFF_OFFSET, 0x1);
    sleepMs(2);
    chip_.write(WFF_ENABLE, 0);
    chip_.write(RFF_ENABLE, 0);
    chip_.write(WFF_FF_STA_INV, 1);
    chip_.write(MADPT_Y_MI_OFFSET, 0x7F);
    chip_.write(MADPT_Y_MI_DET_BYPS, 1);
}

} // namespace gbsr::video
