#include "video/picture.h"

#include "chip/fields.gen.h"

namespace gbsr::video {

using namespace tv5725::fields;

namespace {

// Neutral output colour space (docs/research/gbs-control-mechanics.md §8.4): unity luma gain and
// the chroma gains that match the decimator's RGB→YUV matrix; component input carries a small
// offset of its own.
constexpr uint8_t kYGain = 0x80;
constexpr uint8_t kUGain = 0x1C;
constexpr uint8_t kVGain = 0x29;
constexpr int8_t kYOffsetYpbpr = -2;
constexpr int8_t kUvOffsetYpbpr = 3;

// Horizontal peaking gain per sharpness step; medium is the community firmware's default.
constexpr uint8_t kPeaking[4] = {0x00, 0x0A, 0x14, 0x20};

uint8_t clampByte(int v) {
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 0xFF ? 0xFF : v));
}

} // namespace

void applyPicture(Ops& ops, bool inputIsYpbpr, const PictureAdjust& adj) {
    tv5725::Chip& chip = ops.chip();
    const int yOffset = (inputIsYpbpr ? kYOffsetYpbpr : 0) + adj.brightness;
    const int uvOffset = inputIsYpbpr ? kUvOffsetYpbpr : 0;
    chip.write(VDS_Y_OFST, static_cast<uint8_t>(static_cast<int8_t>(yOffset))); // two's complement
    chip.write(VDS_U_OFST, static_cast<uint8_t>(uvOffset));
    chip.write(VDS_V_OFST, static_cast<uint8_t>(uvOffset));
    chip.write(VDS_Y_GAIN, clampByte(kYGain + 2 * adj.contrast));
    chip.write(VDS_UCOS_GAIN, clampByte(kUGain * (32 + adj.saturation) / 32));
    chip.write(VDS_VCOS_GAIN, clampByte(kVGain * (32 + adj.saturation) / 32));
    const uint8_t step = adj.sharpness < 4 ? adj.sharpness : 3;
    chip.write(VDS_PK_LB_GAIN, kPeaking[step]);
    chip.write(VDS_PK_LH_GAIN, kPeaking[step]);
    chip.write(VDS_PK_Y_H_BYPS, step == 0 ? 1 : 0);
}

} // namespace gbsr::video
