#include "video/geometry.h"

#include "chip/fields.gen.h"

namespace gbsr::video {

using namespace tv5725::fields;

namespace {

// What the HDMI converter shows of each output program: the active share of the standard timing
// it recognises, and that frame's lines and shape. On the M1 output-matrix captures the same
// Mega Drive picture (NTSC programs) and the same PS2 picture (PAL programs) measure one width per
// source within 0.3 % once the visible width is taken as this share of the line; the 50 Hz
// programs follow the 60 Hz shares. The PAL 576p share was inferred from one capture and is not a
// standard timing (docs/bench/M2.md, "What the converter shows").
struct Frame {
    uint16_t active, total;
    uint16_t lines;
    uint8_t shapeW, shapeH;
};

bool frameFor(uint8_t presetId, Frame& f) {
    switch (presetId) {
    case 0x01:
    case 0x11:
        f = {1280, 1800, 960, 4, 3}; // VESA 1280x960
        return true;
    case 0x02:
    case 0x12:
        f = {1280, 1688, 1024, 5, 4}; // VESA 1280x1024
        return true;
    case 0x03:
    case 0x13:
        f = {1280, 1650, 720, 16, 9}; // CEA-861 720p
        return true;
    case 0x04:
        f = {640, 800, 480, 4, 3}; // VESA 640x480
        return true;
    case 0x14:
        f = {781, 1000, 576, 4, 3};
        return true;
    case 0x05:
    case 0x15:
        f = {1920, 2200, 1080, 16, 9}; // CEA-861 1080p
        return true;
    default:
        return false; // the 15 kHz programs bypass the converter: position only
    }
}

// Horizontally the user's size stops at 2x (divisor 512), or at the shape's own divisor where that
// is lower (480p: 478, clean): at 1280x960, 489 displaced a line now and then, 467 several and 447
// garbled the picture (docs/bench/M2.md). Vertically the reference's limit holds (445 was clean).
constexpr int32_t kMinHScale = 512;
constexpr uint32_t kMinVScale = 128;
// Samples the playback may fetch per line: clean at 1015 and 1021, a stray edge column at 1019,
// 1023 and 1027 (1080p at divisor 769, 720p at 705); kept under the lowest failure.
constexpr int32_t kMaxFetchSamples = 1008;
constexpr int32_t kMinWindow = 64;

// BT.601: a 4:3 picture is 704 samples at 13.5 MHz (52.148 µs) wide; as a share of the line
// (1/10000) that is 8205 on 525-line systems (63.556 µs) and 8148 on 625-line ones (64 µs).
constexpr uint32_t kWidth43Of525 = 8205;
constexpr uint32_t kWidth43Of625 = 8148;

int32_t divRound(int64_t n, int64_t d) {
    return static_cast<int32_t>(n >= 0 ? (n + d / 2) / d : -((-n + d / 2) / d));
}

int32_t clampTo(int32_t v, int32_t lo, int32_t hi) {
    if (hi < lo) {
        hi = lo;
    }
    return v < lo ? lo : (v > hi ? hi : v);
}

int32_t wrapTo(int32_t v, int32_t period) {
    if (period <= 0) {
        return v < 0 ? 0 : v;
    }
    v %= period;
    return v < 0 ? v + period : v;
}

} // namespace

GeometryBase readGeometryBase(tv5725::Chip& chip, const PathContext& ctx) {
    GeometryBase b;
    if (ctx.hdBypass) {
        return b;
    }
    // A failed read returns 0, which would pass for a value: any bus error voids the baseline.
    const uint32_t errorsBefore = chip.errorCount();
    b.presetId = ctx.presetId;
    b.htotal = static_cast<uint16_t>(chip.get(VDS_HSYNC_RST));
    b.disHbSt = static_cast<uint16_t>(chip.get(VDS_DIS_HB_ST));
    b.disHbSp = static_cast<uint16_t>(chip.get(VDS_DIS_HB_SP));
    b.hbSt = static_cast<uint16_t>(chip.get(VDS_HB_ST));
    b.hbSp = static_cast<uint16_t>(chip.get(VDS_HB_SP));
    // HSync sits at the start of the line in every program; the window must stay clear of it.
    const uint16_t hsSt = static_cast<uint16_t>(chip.get(VDS_HS_ST));
    const uint16_t hsSp = static_cast<uint16_t>(chip.get(VDS_HS_SP));
    const uint16_t hsEnd = hsSt > hsSp ? hsSt : hsSp;
    b.syncEnd = hsEnd < b.htotal / 2 ? hsEnd : 0;
    b.hscale = chip.get(VDS_HSCALE_BYPS) ? 1024 : static_cast<uint16_t>(chip.get(VDS_HSCALE));
    b.disVbSt = static_cast<uint16_t>(chip.get(VDS_DIS_VB_ST));
    b.disVbSp = static_cast<uint16_t>(chip.get(VDS_DIS_VB_SP));
    b.vbSt = static_cast<uint16_t>(chip.get(VDS_VB_ST));
    b.vbSp = static_cast<uint16_t>(chip.get(VDS_VB_SP));
    b.vtotal = static_cast<uint16_t>(chip.get(VDS_VSYNC_RST));
    const uint16_t vsSt = static_cast<uint16_t>(chip.get(VDS_VS_ST));
    const uint16_t vsSp = static_cast<uint16_t>(chip.get(VDS_VS_SP));
    const uint16_t vsEnd = vsSt > vsSp ? vsSt : vsSp;
    b.vsyncEnd = vsEnd < b.vtotal / 2 ? vsEnd : 0;
    b.vscale = chip.get(VDS_VSCALE_BYPS) ? 1024 : static_cast<uint16_t>(chip.get(VDS_VSCALE));
    b.ifLine = static_cast<uint16_t>(chip.get(IF_HSYNC_RST) + 1);
    b.ifHbSp2 = static_cast<uint16_t>(chip.get(IF_HB_SP2));
    b.ifVbSp = static_cast<uint16_t>(chip.get(IF_VB_SP));
    const uint16_t ifVbSt = static_cast<uint16_t>(chip.get(IF_VB_ST));
    b.ifVbGap = b.ifVbSp > ifVbSt ? static_cast<uint16_t>(b.ifVbSp - ifVbSt) : 2;
    const bool lines525 = ctx.standard == Standard::Ntsc || ctx.standard == Standard::Edtv60;
    const bool lines625 = ctx.standard == Standard::Pal || ctx.standard == Standard::Edtv50;
    // The reference wraps the capture start this far before the counted frame end
    // (geometry.cpp shiftVerticalUpIF / DownIF). A count far from the standard's own (a csync
    // source reads it less reliably) is replaced by the nominal one.
    uint16_t vperiod = static_cast<uint16_t>(chip.get(VPERIOD_IF));
    const uint16_t nominal = lines525 ? 525 : (lines625 ? 625 : 0);
    if (nominal != 0 && (vperiod + 8 < nominal || vperiod > nominal + 8)) {
        vperiod = nominal;
    } else if (nominal == 0 && (vperiod < 64 || vperiod > 1400)) {
        vperiod = 0; // no wrap: the capture start stops at the frame start
    }
    const uint16_t tail = ctx.standard == Standard::Pal ? 4 : 1;
    b.ifFrame = vperiod > tail ? static_cast<uint16_t>(vperiod - tail) : 0;
    // 15 kHz sources pass the line doubler: one source line is two input-formatter lines.
    b.lineStep = isSd(ctx.standard) ? 2 : 1;

    if (chip.errorCount() != errorsBefore || b.htotal < 256 || b.disHbSt <= b.disHbSp ||
        b.disHbSt > b.htotal || b.disVbSt <= b.disVbSp || b.disVbSt > b.vtotal || b.ifLine < 256 ||
        b.hscale == 0 || b.vscale == 0) {
        return b; // not a window this code understands; leave the program alone
    }
    if (lines525 || lines625) {
        b.width43 = static_cast<uint16_t>(divRound(
            static_cast<int64_t>(b.ifLine) * (lines525 ? kWidth43Of525 : kWidth43Of625), 10000));
        b.fullHeight = lines525 ? 480 : 576;
    } else {
        // Unknown shape: the program's own capture is taken as the 4:3 picture.
        b.width43 = static_cast<uint16_t>(
            divRound(static_cast<int64_t>(b.disHbSt - b.disHbSp) * b.hscale, 1024));
        b.fullHeight = static_cast<uint16_t>(
            divRound(static_cast<int64_t>(b.disVbSt - b.disVbSp) * b.vscale, 1024));
    }
    b.valid = true;
    return b;
}

GeometryRegs computeGeometry(const GeometryBase& b, const GeometryAdjust& adj, uint16_t htotal) {
    GeometryRegs r;
    if (!b.valid) {
        return r;
    }
    if (htotal < 256) {
        htotal = b.htotal; // a failed read; the line has not changed by that much
    }
    // A frame-lock line change moves the window by half the difference (mechanics §6.2).
    int32_t shift = (static_cast<int32_t>(htotal) - b.htotal) / 2;
    shift -= shift % 2;

    // ---- scales: the picture's shape on screen, then the user's size
    int32_t hs = b.hscale;
    int32_t vs = b.vscale;
    int32_t visible = 0; // the converter's width in output units
    Frame f{};
    if (frameFor(b.presetId, f) && b.width43 != 0 && b.fullHeight != 0) {
        uint32_t rw = 4, rh = 3;
        if (adj.aspect == Aspect::SixteenNine) {
            rw = 16;
            rh = 9;
        } else if (adj.aspect == Aspect::Full) {
            rw = f.shapeW;
            rh = f.shapeH;
        }
        // Share of the frame (1/10000) the picture's full height and 4:3 width fill. A picture
        // wider than the frame is letterboxed (user decision 2026-10-14).
        int64_t hb = 10000;
        if (rw * f.shapeH > f.shapeW * rh) {
            hb = 10000LL * f.shapeW * rh / (static_cast<int64_t>(f.shapeH) * rw);
        }
        int64_t wb = hb * rw * f.shapeH / (static_cast<int64_t>(rh) * f.shapeW);
        visible = static_cast<int32_t>((static_cast<uint32_t>(htotal) + 1) * f.active / f.total);
        const int32_t hsShape = divRound(1024LL * b.width43 * 10000, wb * visible);
        wb = wb * (200 + adj.hSize) / 200;
        hb = hb * (200 + adj.vSize) / 200;
        hs = clampTo(divRound(1024LL * b.width43 * 10000, wb * visible),
                     hsShape < kMinHScale ? hsShape : kMinHScale, 1024);
        vs = clampTo(divRound(1024LL * b.fullHeight * 10000, hb * f.lines), kMinVScale, 1024);
    }
    r.hscale = static_cast<uint16_t>(hs);
    r.vscale = static_cast<uint16_t>(vs);

    // ---- horizontal: the window shows what the program captured, at the new scale, centred where
    // the program put it; the fetch blanking keeps the program's own distances from it
    // (videopath lessons §3.5: offsets that are not the array's own wrap or tear the picture).
    const int32_t w0 = b.disHbSt - b.disHbSp;
    const int32_t c0 = divRound(static_cast<int64_t>(w0) * b.hscale, 1024);
    const int32_t centre = (b.disHbSt + b.disHbSp) / 2 + shift;
    const int32_t fetchLead = b.disHbSp - b.hbSp;
    const int32_t fetchTail = b.disHbSt - b.hbSt;
    int32_t wMax = w0 > visible ? w0 : visible;
    const int32_t room =
        2 * (centre - fetchLead < htotal - centre ? centre - fetchLead : htotal - centre);
    if (wMax > room) {
        wMax = room;
    }
    int32_t w = divRound(static_cast<int64_t>(c0) * 1024, hs);
    w = clampTo(w, kMinWindow, wMax) & ~1;
    const int32_t c = divRound(static_cast<int64_t>(w) * hs, 1024);
    // The position moves the whole window (display, fetch and mask) against the sync, two output
    // units per step: the picture moves as it is. Moving the capture start instead showed the
    // decoder's green from before the source's active line at the window edge (docs/bench/M2.md).
    const int32_t placed = (centre - w / 2) & ~1;
    const int32_t lo = b.syncEnd + 4 > fetchLead ? b.syncEnd + 4 : fetchLead;
    const int32_t hi = htotal - 2 - w;
    const int32_t disSp =
        clampTo(placed + 2 * adj.hPosition, placed < lo ? placed : lo, placed > hi ? placed : hi) &
        ~1;
    const int32_t disSt = disSp + w;
    int32_t hbSp = disSp - fetchLead;
    int32_t hbSt = disSt - fetchTail;
    if (hs != b.hscale) {
        // At a scale the program was not made for, a fetch of more than about 1016 samples per
        // line showed a thin copy of the picture at the window's far edge (PB_FETCH_NUM is 256,
        // i.e. 1024): end the fetch earlier, which leaves the content where it is
        // (docs/bench/M2.md).
        const int32_t maxFetch = divRound(static_cast<int64_t>(kMaxFetchSamples) * 1024, hs) & ~1;
        if (hbSt - hbSp > maxFetch) {
            hbSt = hbSp + maxFetch;
        }
    }
    if (hs < 512) {
        // Below 2x the reference keeps HB_ST even and HB_SP at the parity of VDS_HSYNC_RST
        // (geometry.cpp scaleHorizontal).
        hbSt += hbSt & 1;
        if ((hbSp & 1) != (htotal & 1)) {
            hbSp -= 1;
        }
    }
    r.disHbSp = static_cast<uint16_t>(disSp);
    r.disHbSt = static_cast<uint16_t>(disSt);
    r.hbSp = static_cast<uint16_t>(clampTo(hbSp, 0, htotal));
    r.hbSt = static_cast<uint16_t>(clampTo(hbSt, 0, htotal));

    // The capture start keeps the centre of the program's capture at the window's centre
    // (IF_HB_SP2 is a pure translation: videopath lessons §2.8).
    const int32_t sp2 = b.ifHbSp2 + (c0 - c) / 2;
    const int32_t sp2Max =
        b.ifLine - c > b.ifHbSp2 + (c0 - c) / 2 ? b.ifLine - c : b.ifHbSp2 + (c0 - c) / 2;
    r.ifHbSp2 = static_cast<uint16_t>(clampTo(sp2, 0, sp2Max));

    // ---- vertical, the same way: the window shows what the program captured at the new scale,
    // and the fetch blanking (VDS_VB) moves with it, keeping the program's distances. Moving the
    // capture start instead lost the bottom of the picture once it wrapped into the previous frame
    // (the frame memory holds one frame from the capture start); moving the fetch blanking with
    // the window letterboxed cleanly (docs/bench/M2.md).
    const int32_t h0 = b.disVbSt - b.disVbSp;
    const int32_t k0 = divRound(static_cast<int64_t>(h0) * b.vscale, 1024);
    int32_t h = divRound(static_cast<int64_t>(k0) * 1024, vs);
    h = clampTo(h, kMinWindow, h0);
    const int32_t k = divRound(static_cast<int64_t>(h) * vs, 1024);
    const int32_t vPlaced = (b.disVbSt + b.disVbSp) / 2 - h / 2;
    // One source line per step, in output lines at the program's own scale.
    const int32_t vMove =
        divRound(static_cast<int64_t>(adj.vPosition) * b.lineStep * 1024, b.vscale);
    // The window may not start before the sync plus the fetch lead, nor end after the frame (the
    // programs already end at the last line): past either limit the picture is cut there.
    const int32_t fetchLeadV = b.disVbSp - (b.vbSt < b.vbSp ? b.vbSt : b.vbSp);
    const int32_t syncLimit = b.vsyncEnd + 1 + fetchLeadV;
    const int32_t vTopLimit = syncLimit < b.disVbSp ? syncLimit : b.disVbSp;
    const int32_t vBottomLimit = b.vtotal > b.disVbSt ? b.vtotal : b.disVbSt;
    const int32_t want = vPlaced + vMove;
    const int32_t top = clampTo(want, vTopLimit, vBottomLimit - kMinWindow);
    const int32_t bottom = clampTo(want + h, top + kMinWindow, vBottomLimit);
    const int32_t dv = top - b.disVbSp;
    r.disVbSp = static_cast<uint16_t>(top);
    r.disVbSt = static_cast<uint16_t>(bottom);
    r.vbSt = static_cast<uint16_t>(clampTo(b.vbSt + dv, 0, b.vtotal));
    r.vbSp = static_cast<uint16_t>(clampTo(b.vbSp + dv, 0, b.vtotal));
    // The window's top shows the capture start. A picture taller than the window is cropped
    // around its centre, and one moved above the top limit is cut there: either way the capture
    // starts later, in whole source lines, and never earlier than the program's.
    int32_t crop = (k0 - k) / 2 + divRound(static_cast<int64_t>(top - want) * vs, 1024);
    crop -= crop % b.lineStep;
    const int32_t vsp = b.ifVbSp + crop;
    r.ifVbSp = static_cast<uint16_t>(vsp);
    r.ifVbSt = static_cast<uint16_t>(wrapTo(vsp - b.ifVbGap, b.ifFrame));
    return r;
}

namespace {

// The fields that depend on the line length; the frame lock rewrites only these.
void writeHorizontal(tv5725::Chip& chip, const GeometryRegs& r) {
    chip.write(VDS_HSCALE, r.hscale >= 1024 ? 1023 : r.hscale);
    chip.write(VDS_HSCALE_BYPS, r.hscale >= 1024 ? 1 : 0);
    chip.write(VDS_DIS_HB_ST, r.disHbSt);
    chip.write(VDS_DIS_HB_SP, r.disHbSp);
    chip.write(VDS_HB_ST, r.hbSt);
    chip.write(VDS_HB_SP, r.hbSp);
    chip.write(VDS_EXT_HB_ST, r.disHbSt);
    chip.write(VDS_EXT_HB_SP, r.disHbSp);
    chip.write(IF_HB_SP2, r.ifHbSp2);
}

} // namespace

void writeGeometry(tv5725::Chip& chip, const GeometryRegs& r) {
    writeHorizontal(chip, r);
    chip.write(VDS_VSCALE, r.vscale >= 1024 ? 1023 : r.vscale);
    chip.write(VDS_VSCALE_BYPS, r.vscale >= 1024 ? 1 : 0);
    chip.write(VDS_DIS_VB_ST, r.disVbSt);
    chip.write(VDS_DIS_VB_SP, r.disVbSp);
    chip.write(VDS_EXT_VB_ST, r.disVbSt);
    chip.write(VDS_EXT_VB_SP, r.disVbSp);
    chip.write(VDS_VB_ST, r.vbSt);
    chip.write(VDS_VB_SP, r.vbSp);
    chip.write(IF_VB_ST, r.ifVbSt);
    chip.write(IF_VB_SP, r.ifVbSp);
}

bool applyBestHTotal(Ops& ops, Sense& sense, PathContext& ctx, uint16_t bestHTotal,
                     const GeometryBase& base, const GeometryAdjust& adj) {
    tv5725::Chip& chip = ops.chip();
    if (ctx.hdBypass || bestHTotal < 256 || bestHTotal > 4095) {
        return ctx.hdBypass;
    }
    const uint16_t orig = static_cast<uint16_t>(chip.get(VDS_HSYNC_RST));
    const int diff = static_cast<int>(bestHTotal) - static_cast<int>(orig);
    const uint16_t absDiff = static_cast<uint16_t>(diff < 0 ? -diff : diff);
    if (absDiff == 0) {
        return true;
    }
    uint32_t limitPermille = 60; // typical differences are a handful of units out of ~2000
    if (sense.videoMode(ctx.standard, ctx.inputIsYpbpr) == Standard::MedRes ||
        ctx.standard == Standard::RgbhvScaled) {
        limitPermille = 160; // arcade sources run anywhere from 52 to 60 Hz
    }
    if (absDiff * 1000u > orig * limitPermille || chip.get(STATUS_INT_SOG_BAD) == 1) {
        return false;
    }
    // Without a baseline only the line changes; the windows stay where the program put them.
    const GeometryRegs regs = computeGeometry(base, adj, bestHTotal);
    const uint16_t disSp =
        base.valid ? regs.disHbSp : static_cast<uint16_t>(chip.get(VDS_DIS_HB_SP));

    // HSync must stay inside the blanking.
    uint16_t hsSt = static_cast<uint16_t>(chip.get(VDS_HS_ST));
    uint16_t hsSp = static_cast<uint16_t>(chip.get(VDS_HS_SP));
    const bool negative = hsSt > hsSp && hsSt < bestHTotal / 2;
    if (negative) {
        if (hsSt >= disSp) {
            hsSt = static_cast<uint16_t>(disSp * 8u / 10u);
            hsSp = 4;
        }
    } else if (hsSp >= disSp) {
        hsSp = static_cast<uint16_t>(disSp * 8u / 10u);
        hsSt = 4;
    }

    // Apply at a field start: wait for the field flag to go 1 -> 0 -> 1 (bounded).
    uint16_t guard = 0;
    while (chip.get(STATUS_VDS_FIELD) == 1 && ++guard < 400) {
    }
    while (chip.get(STATUS_VDS_FIELD) == 0 && ++guard < 800) {
    }
    chip.write(VDS_HSYNC_RST, bestHTotal);
    chip.write(VDS_HS_ST, hsSt);
    chip.write(VDS_HS_SP, hsSp);
    if (base.valid) {
        writeHorizontal(chip, regs); // the vertical does not depend on the line length
    }
    return true;
}

} // namespace gbsr::video
