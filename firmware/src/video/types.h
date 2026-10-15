#pragma once

#include <stdint.h>

namespace gbsr::video {

// Input classification. The numeric values are the ones the community firmware uses in logs and
// documentation, kept so bench notes stay comparable (docs/design/video-engine.md, "Input
// standards").
enum class Standard : uint8_t {
    None = 0,
    Ntsc = 1,   // 15 kHz 60 Hz: 240p / 480i
    Pal = 2,    // 15 kHz 50 Hz: 288p / 576i
    Edtv60 = 3, // 480p
    Edtv50 = 4, // 576p
    Hd720p = 5,
    Hd1080i = 6,
    Hd1080p = 7,
    MedRes = 8,     // ~25 kHz
    Unknown = 9,    // stable but unrecognised
    PcOnYpbpr = 13, // graphic mode carried over component
    RgbhvScaled = 14,
    RgbhvBypass = 15,
};

enum class InputPort : uint8_t { Component = 0, Rgb = 1, Unconnected = 2 };

enum class SyncType : uint8_t { None, Hv, Csync };

// Input choice offered to the user; values match schema/settings.yaml input.port.
enum class InputPreference : uint8_t { Auto, Rgb, Component };

// Output modes offered to the user; values match schema/settings.yaml output.resolution.
enum class OutputMode : uint8_t {
    Auto,
    R1080p,
    R1024p,
    R960p,
    R720p,
    R480p,
    R15kHz,
    Passthrough,
    // Computed from standard timings on the 1080p program (ADR 0015).
    R1024x768,
    R800x600,
    R1600x1200,
    R1920x1200,
};

enum class EngineState : uint8_t {
    NoPower,   // the scaler does not answer on I2C
    Searching, // looking for a source on the inputs
    Loading,   // writing a program and its post-load tuning
    Settling,  // waiting for sync, mode and clamp after a load
    Running,   // picture path established; the watcher keeps it there
    Bypass,    // HD or RGBHV passthrough
    Starting,  // boot-time ADC calibration, before the first search
};

// The ordered status sentences of docs/design/ux.md; the lowest value wins when several apply.
enum class Headline : uint8_t {
    ScalerMissing,
    NoSignal,
    SyncUnstable,
    Settling,
    Locking,
    Scaling,
    Paused,
};

// Values match schema/settings.yaml output.aspect.
enum class Aspect : uint8_t { FourThree, SixteenNine, Full };

// output.aspect / hPosition / vPosition / hSize / vSize.
struct GeometryAdjust {
    Aspect aspect = Aspect::FourThree;
    int8_t hPosition = 0; // + moves right; two output units per step
    int8_t vPosition = 0; // + moves down; one source line per step
    int8_t hSize = 0;     // + wider; 0.5 % per step
    int8_t vSize = 0;     // + taller; 0.5 % per step
    bool integer = false; // the source's lines at a whole multiple; vSize then has no effect
    bool operator==(const GeometryAdjust&) const = default;
};

// picture.scanlines / scanlineStrength / deinterlace.
struct LineAdjust {
    bool scanlines = false;
    uint8_t strength = 4; // 1..8
    bool bob = false;     // deinterlacer: bob instead of motion adaptive
    bool operator==(const LineAdjust&) const = default;
};

// input.syncThreshold / phase / videoLpf.
struct SamplingAdjust {
    int8_t sog = -1;   // 0..31, -1 automatic
    int8_t phase = -1; // ADC sampling phase 0..31, -1 automatic
    uint8_t lpf = 0;   // 0 automatic, 1..4 = 150, 110, 70, 40 MHz
    bool operator==(const SamplingAdjust&) const = default;
};

inline bool isSd(Standard s) {
    return s == Standard::Ntsc || s == Standard::Pal;
}

inline bool isEdtvLike(Standard s) {
    return s == Standard::Edtv60 || s == Standard::Edtv50 || s == Standard::MedRes ||
           s == Standard::Unknown;
}

inline bool isPalFamily(Standard s) {
    return s == Standard::Pal || s == Standard::Edtv50;
}

} // namespace gbsr::video
