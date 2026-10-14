#include "video/scanlines.h"

#include "chip/fields.gen.h"

namespace gbsr::video {

using namespace tv5725::fields;

namespace {

// The reference's constants for the mixer path (gbs-control scanlines.cpp).
constexpr uint8_t kWhiteLevelGain = 0x08;
constexpr uint8_t kViirCoef = 0x08;

} // namespace

uint8_t scanlineOffset(uint8_t level) {
    if (level < 1) {
        level = 1;
    } else if (level > kScanlineLevels) {
        level = kScanlineLevels;
    }
    return static_cast<uint8_t>(0x80 - 16 * level);
}

ScanlineBase readScanlineBase(tv5725::Chip& chip) {
    const uint32_t errorsBefore = chip.errorCount();
    ScanlineBase b;
    auto get = [&chip](auto field) { return static_cast<uint8_t>(chip.get(field)); };
    b.uvdlyPdSp = get(MADPT_UVDLY_PD_SP);
    b.uvdlyPdSt = get(MADPT_UVDLY_PD_ST);
    b.enUvDeint = get(MADPT_EN_UV_DEINT);
    b.uvMiDetByps = get(MADPT_UV_MI_DET_BYPS);
    b.uvMiOffset = get(MADPT_UV_MI_OFFSET);
    b.moAdpUvEn = get(MADPT_MO_ADP_UV_EN);
    b.bobPldyRamByps = get(DIAG_BOB_PLDY_RAM_BYPS);
    b.pdRamByps = get(MADPT_PD_RAM_BYPS);
    b.rffYuvDeinterlace = get(RFF_YUV_DEINTERLACE);
    b.yMiDetByps = get(MADPT_Y_MI_DET_BYPS);
    b.wlevGain = get(VDS_WLEV_GAIN);
    b.wlevByps = get(VDS_W_LEV_BYPS);
    b.viirCoef = get(MADPT_VIIR_COEF);
    b.viirByps = get(MADPT_VIIR_BYPS);
    b.yMiOffset = get(MADPT_Y_MI_OFFSET);
    b.rffLineFlip = get(RFF_LINE_FLIP);
    b.vtSelPrgv = get(MAPDT_VT_SEL_PRGV);
    b.valid = chip.errorCount() == errorsBefore;
    return b;
}

void enableScanlines(tv5725::Chip& chip, uint8_t level) {
    // The reference's order: chroma mixer, the deinterlacer RAM paths, white level, the vertical
    // IIR, then the luma mixer and the path select last.
    const uint8_t offset = scanlineOffset(level);
    chip.write(MADPT_UVDLY_PD_SP, 0);
    chip.write(MADPT_UVDLY_PD_ST, 0);
    chip.write(MADPT_EN_UV_DEINT, 1);
    chip.write(MADPT_UV_MI_DET_BYPS, 1);
    chip.write(MADPT_UV_MI_OFFSET, offset);
    chip.write(MADPT_MO_ADP_UV_EN, 1);
    chip.write(DIAG_BOB_PLDY_RAM_BYPS, 0);
    chip.write(MADPT_PD_RAM_BYPS, 0);
    chip.write(RFF_YUV_DEINTERLACE, 1);
    chip.write(MADPT_Y_MI_DET_BYPS, 1);
    chip.write(VDS_WLEV_GAIN, kWhiteLevelGain);
    chip.write(VDS_W_LEV_BYPS, 0);
    chip.write(MADPT_VIIR_COEF, kViirCoef);
    chip.write(MADPT_VIIR_BYPS, 0);
    chip.write(MADPT_Y_MI_OFFSET, offset);
    chip.write(RFF_LINE_FLIP, 1);
    chip.write(MAPDT_VT_SEL_PRGV, 0);
}

void setScanlineStrength(tv5725::Chip& chip, uint8_t level) {
    const uint8_t offset = scanlineOffset(level);
    chip.write(MADPT_UV_MI_OFFSET, offset);
    chip.write(MADPT_Y_MI_OFFSET, offset);
}

void disableScanlines(tv5725::Chip& chip, const ScanlineBase& b) {
    // The enable in reverse.
    chip.write(MAPDT_VT_SEL_PRGV, b.vtSelPrgv);
    chip.write(RFF_LINE_FLIP, b.rffLineFlip);
    chip.write(MADPT_Y_MI_OFFSET, b.yMiOffset);
    chip.write(MADPT_VIIR_BYPS, b.viirByps);
    chip.write(MADPT_VIIR_COEF, b.viirCoef);
    chip.write(VDS_W_LEV_BYPS, b.wlevByps);
    chip.write(VDS_WLEV_GAIN, b.wlevGain);
    chip.write(MADPT_Y_MI_DET_BYPS, b.yMiDetByps);
    chip.write(RFF_YUV_DEINTERLACE, b.rffYuvDeinterlace);
    chip.write(MADPT_PD_RAM_BYPS, b.pdRamByps);
    chip.write(DIAG_BOB_PLDY_RAM_BYPS, b.bobPldyRamByps);
    chip.write(MADPT_MO_ADP_UV_EN, b.moAdpUvEn);
    chip.write(MADPT_UV_MI_OFFSET, b.uvMiOffset);
    chip.write(MADPT_UV_MI_DET_BYPS, b.uvMiDetByps);
    chip.write(MADPT_EN_UV_DEINT, b.enUvDeint);
    chip.write(MADPT_UVDLY_PD_ST, b.uvdlyPdSt);
    chip.write(MADPT_UVDLY_PD_SP, b.uvdlyPdSp);
}

} // namespace gbsr::video
