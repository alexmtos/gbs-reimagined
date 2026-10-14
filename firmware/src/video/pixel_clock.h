#pragma once

#include <stdint.h>

#include "hal/si5351.h"
#include "video/ops.h"
#include "video/sense.h"

namespace gbsr::video {

// The external pixel clock (Si5351 CLK0 → TV5725 pin 40). Programs are tuned for the chip's
// internal display PLL; once a picture is settled the display is switched ("adopted") onto the
// Si5351 and the clock is set to `clock × sourceRate / outputRate`, so the output rate equals the
// source rate (ADR 0006).
class PixelClock {
public:
    static constexpr uint32_t kMinHz = 20'000'000;
    static constexpr uint32_t kMaxHz = 162'000'000; // the TV5725 DAC's rating comes first

    PixelClock(Ops& ops, Sense& sense, hal::Si5351* si) : ops_(ops), sense_(sense), si_(si) {}

    bool present() const { return si_ != nullptr && present_; }
    void setPresent(bool present) { present_ = present; }

    // Frequency equivalent of an internal display-clock code (0x25 40.5 MHz … 0xA5 162 MHz).
    static uint32_t frequencyForCode(uint8_t code);

    // After a program load: park the generator at the program's internal-clock frequency, output
    // off, and forget any rate relationship.
    void resetForProgram(uint8_t displayClockCode);

    // Switch the display onto the generator (enable CLK0, wait 800 µs, PLL648_CONTROL_01 = 0x75).
    void adopt();

    // Whether the display currently runs from the generator.
    bool adopted();

    // Back onto the chip's internal display clock at the program's own rate (frame lock off).
    void release(uint8_t displayClockCode);

    enum class SyncResult : uint8_t {
        Locked,           // retuned: output rate = source rate
        NotReady,         // not adopted, no generator, or a rate could not be measured
        NeedsShorterLine, // the source is faster than the program allows within 162 MHz
        OutOfRange,       // the required clock is below 20 MHz or far above the DAC rating
    };

    // Measure source and output rates and retune so they match (ADR 0006).
    SyncResult syncToSource(bool csync);
    // Measure both rates for the status only (frame lock off, or no generator).
    void measure(bool csync);

    // The clock the last measurement asked for, even when refused.
    uint32_t requiredHz() const { return requiredHz_; }

    uint32_t frequency() const { return frequencyHz_; }
    uint32_t lastSourceMilliHz() const { return sourceMilliHz_; }
    uint32_t lastOutputMilliHz() const { return outputMilliHz_; }
    bool locked() const { return locked_; }
    void unlock() { locked_ = false; }

private:
    bool setFrequency(uint32_t hz);
    void rampTo(uint32_t hz);

    Ops& ops_;
    Sense& sense_;
    hal::Si5351* si_;
    bool present_ = false;
    bool locked_ = false;
    uint32_t frequencyHz_ = 0;
    uint32_t sourceMilliHz_ = 0;
    uint32_t outputMilliHz_ = 0;
    uint32_t requiredHz_ = 0;
};

} // namespace gbsr::video
