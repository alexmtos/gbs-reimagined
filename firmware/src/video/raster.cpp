#include "video/raster.h"

#include <string.h>

#include "platform/progmem.h"

namespace gbsr::video {

namespace {

struct NamedTiming {
    char name[12];
    Timing t;
};

// VESA DMT, CVT-RB and CEA-861 timings. The first four are the baseline programs' own frames, to
// check the computation against a converter that already shows them.
const NamedTiming kTimings[] PROGMEM = {
    {"1080p", {1920, 88, 44, 148, 1080, 4, 5, 36, false, false, 16, 9}},     // CEA-861
    {"720p", {1280, 110, 40, 220, 720, 5, 5, 20, false, false, 16, 9}},      // CEA-861
    {"1280x960", {1280, 96, 112, 312, 960, 1, 3, 36, false, false, 4, 3}},   // DMT
    {"1280x1024", {1280, 48, 112, 248, 1024, 1, 3, 38, false, false, 5, 4}}, // DMT
    {"1024x768", {1024, 24, 136, 160, 768, 3, 6, 29, true, true, 4, 3}},     // DMT
    {"800x600", {800, 40, 128, 88, 600, 1, 4, 23, false, false, 4, 3}},      // DMT
    {"1600x1200", {1600, 48, 32, 80, 1200, 3, 4, 28, false, true, 4, 3}},    // CVT-RB
    {"1920x1200", {1920, 48, 32, 80, 1200, 3, 6, 26, false, true, 16, 10}},  // CVT-RB
};

// The internal display-clock codes, slowest first (RD-5725 p.81).
struct ClockCode {
    uint8_t code;
    uint32_t hz;
};
const ClockCode kClockCodes[] PROGMEM = {
    {0x25, 40'500'000},  {0x45, 54'000'000},  {0x55, 64'800'000},  {0x65, 81'000'000},
    {0x85, 108'000'000}, {0x95, 129'600'000}, {0xA5, 162'000'000},
};
constexpr uint8_t kClockCount = sizeof kClockCodes / sizeof kClockCodes[0];

ClockCode clockAt(uint8_t i) {
    ClockCode c{};
    memcpy_P(&c, &kClockCodes[i], sizeof c);
    return c;
}

// A 4:3 picture's divisor on an extra mode leaves room for 16:9 (three quarters of it) above the
// 512 below which columns were displaced (docs/bench/M2.md).
constexpr int32_t kMinRasterHScale = 683;
// A longer line only when the picture would otherwise be more than 2 % too wide: NTSC 4:3 at 1080p
// asks for 1025.5 (0.15 %), PAL 4:3 for about 1.1x and integer scale for 1.125x.
constexpr int32_t kStretchAbove = 1044;
constexpr int32_t kStretchTarget = 1014;

// Where the HSync pulse starts, and the first line of the VSync (the reference's choice:
// mechanics §6.3).
constexpr uint16_t kHsStart = 8;
constexpr uint16_t kVsStart = 1;

uint16_t scaled(uint32_t px, uint32_t units, uint32_t total) {
    return static_cast<uint16_t>((px * units + total / 2) / total);
}

} // namespace

bool timingByName(const char* name, Timing& out) {
    for (const NamedTiming& n : kTimings) {
        char buf[sizeof n.name];
        strncpy_P(buf, n.name, sizeof buf);
        if (strcmp(buf, name) == 0) {
            memcpy_P(&out, &n.t, sizeof out);
            return true;
        }
    }
    return false;
}

Raster makeRaster(const GeometryBase& donor, const Timing& t, uint16_t hUnits) {
    Raster r;
    const uint32_t ht = t.hTotal();
    const uint16_t vt = t.vTotal();
    RasterRegs& g = r.regs;
    g.hsyncRst = static_cast<uint16_t>(hUnits - 1);
    g.vsyncRst = static_cast<uint16_t>(vt - 1);
    const uint16_t hsEnd = static_cast<uint16_t>(kHsStart + scaled(t.hSync, hUnits, ht));
    g.hsSt = t.hNegative ? hsEnd : kHsStart;
    g.hsSp = t.hNegative ? kHsStart : hsEnd;
    const uint16_t vsEnd = static_cast<uint16_t>(kVsStart + t.vSync);
    g.vsSt = t.vNegative ? vsEnd : kVsStart;
    g.vsSp = t.vNegative ? kVsStart : vsEnd;
    g.vsynSize = static_cast<uint16_t>(g.vsyncRst + 2);

    GeometryBase& b = r.base;
    b = donor;
    b.valid = false;
    if (!donor.valid || hUnits < 256 || hUnits > 4096 || vt < 64 || vt > 2048) {
        return r;
    }
    b.presetId = 0xFF; // not a program: the frame below says what the converter shows
    b.frameActive = t.hActive;
    b.frameTotal = static_cast<uint16_t>(ht);
    b.frameLines = t.vActive;
    b.shapeW = t.shapeW;
    b.shapeH = t.shapeH;

    // Horizontal: the window is the active area after the sync and back porch, showing the
    // donor's capture across it (the scaler only enlarges), with the donor's fetch distances.
    const int32_t activeSp = (kHsStart + scaled(t.hSync + t.hBack, hUnits, ht)) & ~1;
    const int32_t activeW = scaled(t.hActive, hUnits, ht) & ~1;
    const int32_t capture = (donor.disHbSt - donor.disHbSp) * donor.hscale / 1024;
    if (capture > activeW) {
        return r; // more units per pixel are needed
    }
    const int32_t fetchLead = donor.disHbSp - donor.hbSp;
    const int32_t fetchTail = donor.disHbSt - donor.hbSt;
    b.htotal = g.hsyncRst;
    b.disHbSp = static_cast<uint16_t>(activeSp);
    b.disHbSt = static_cast<uint16_t>(activeSp + activeW);
    b.hbSp = static_cast<uint16_t>(activeSp - fetchLead);
    b.hbSt = static_cast<uint16_t>(activeSp + activeW - fetchTail);
    b.syncEnd = hsEnd;
    b.hscale = static_cast<uint16_t>((1024 * capture + activeW / 2) / activeW);

    // Vertical, the same way in lines.
    const int32_t activeTop = kVsStart + t.vSync + t.vBack;
    const int32_t lines = (donor.disVbSt - donor.disVbSp) * donor.vscale / 1024;
    // Reduced-blanking timings leave less back porch than the donor's fetch lead: the fetch then
    // starts during the sync (the reference used 4 pixels of lead: mechanics §6.3). Bench check.
    if (lines > t.vActive || activeSp < fetchLead + 2) {
        return r;
    }
    const int32_t fetchLeadV = donor.disVbSp - donor.vbSp;
    const int32_t fetchTailV = donor.disVbSp - donor.vbSt;
    if (activeTop <= fetchLeadV || activeTop <= fetchTailV) {
        return r; // the fetch would have to start before the frame
    }
    b.vtotal = g.vsyncRst;
    b.disVbSp = static_cast<uint16_t>(activeTop);
    b.disVbSt = static_cast<uint16_t>(activeTop + t.vActive);
    b.vbSp = static_cast<uint16_t>(activeTop - fetchLeadV);
    b.vbSt = static_cast<uint16_t>(activeTop - fetchTailV);
    b.vsyncEnd = vsEnd;
    b.vscale = static_cast<uint16_t>((1024 * lines + t.vActive / 2) / t.vActive);
    // The fetch may start during the VSync, as the PAL 1080p program's own does (VB_ST 3, VSync
    // to line 6); the guard above keeps it inside the frame.
    b.valid = true;
    return r;
}

uint32_t displayClockHz(uint8_t code) {
    for (uint8_t i = 0; i < kClockCount; ++i) {
        if (clockAt(i).code == code) {
            return clockAt(i).hz;
        }
    }
    return 0;
}

LoadRaster rasterForMode(const GeometryBase& donor, const Timing& t, uint32_t nominalMilliHz) {
    LoadRaster chosen;
    if (nominalMilliHz == 0 || t.hTotal() == 0) {
        return chosen;
    }
    for (uint8_t i = kClockCount; i-- > 0;) {
        const ClockCode c = clockAt(i);
        const uint64_t units = (static_cast<uint64_t>(c.hz) * 1000 +
                                static_cast<uint64_t>(t.vTotal()) * nominalMilliHz / 2) /
                               (static_cast<uint64_t>(t.vTotal()) * nominalMilliHz);
        if (units > 4096) {
            continue;
        }
        const Raster r = makeRaster(donor, t, static_cast<uint16_t>(units));
        if (!r.base.valid) {
            break; // slower clocks give still fewer units
        }
        chosen = {true, c.code, r};
        if (computeGeometry(r.base, {}, r.base.htotal).hscale >= kMinRasterHScale) {
            break;
        }
    }
    return chosen;
}

LoadStretch stretchFor(const GeometryBase& program, const GeometryAdjust& adj,
                       uint8_t programCode) {
    LoadStretch s;
    GeometryAdjust shape = adj;
    shape.hSize = 0;
    const int32_t need = neededHScale(program, shape, program.htotal);
    const uint32_t from = displayClockHz(programCode);
    if (!program.valid || need <= kStretchAbove || from == 0) {
        return s;
    }
    for (uint8_t i = 0; i < kClockCount; ++i) {
        const ClockCode c = clockAt(i);
        if (c.hz <= from) {
            continue;
        }
        const uint64_t units = ((program.htotal + 1ull) * c.hz + from / 2) / from;
        if (units > 4096) {
            break;
        }
        if (static_cast<uint64_t>(need) * (program.htotal + 1u) <= kStretchTarget * units) {
            s.valid = true;
            s.code = c.code;
            s.htotal = static_cast<uint16_t>(units - 1);
            s.base = stretchBase(program, s.htotal);
            return s;
        }
    }
    return s;
}

uint16_t stretchPosition(uint16_t pos, uint16_t fromHTotal, uint16_t toHTotal) {
    return scaled(pos, toHTotal + 1u, fromHTotal + 1u);
}

GeometryBase stretchBase(const GeometryBase& b, uint16_t newHTotal) {
    GeometryBase s = b;
    if (!b.valid) {
        return s;
    }
    const uint16_t from = b.htotal;
    auto at = [&](uint16_t p) { return stretchPosition(p, from, newHTotal); };
    s.htotal = newHTotal;
    // The window keeps its even edges; its width and the scale change together, so the capture
    // it shows stays the same.
    s.disHbSp = static_cast<uint16_t>(at(b.disHbSp) & ~1);
    const uint16_t width = static_cast<uint16_t>(at(b.disHbSt - b.disHbSp) & ~1);
    s.disHbSt = static_cast<uint16_t>(s.disHbSp + width);
    s.hbSp = static_cast<uint16_t>(s.disHbSp - at(b.disHbSp - b.hbSp));
    s.hbSt = static_cast<uint16_t>(s.disHbSt - at(b.disHbSt - b.hbSt));
    s.syncEnd = at(b.syncEnd);
    s.hscale = static_cast<uint16_t>(
        (static_cast<uint32_t>(b.disHbSt - b.disHbSp) * b.hscale + width / 2) / width);
    if (s.hscale > 1024) {
        s.hscale = 1024;
    }
    return s;
}

} // namespace gbsr::video
