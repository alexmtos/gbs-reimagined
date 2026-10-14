#pragma once

#include <stdint.h>

namespace gbsr::hal {

// Measures the period of the signal the TV5725 drives onto its debug pin (selected through the
// test bus: input vsync, output vsync, or the sync processor's H-PLL), by timestamping two
// consecutive rising edges in an interrupt. Used for source/output rate measurement and frame lock.
class PulseTimer {
public:
    virtual ~PulseTimer() = default;

    // Period of one cycle in timer ticks, or 0 when two edges did not arrive within `timeoutMs`.
    virtual uint32_t periodTicks(uint32_t timeoutMs) = 0;

    // Ticks per second of `periodTicks` (the CPU clock on the ESP8266).
    virtual uint32_t ticksPerSecond() const = 0;

    // Frequency in millihertz of the measured signal, or 0 when nothing was measured.
    uint32_t frequencyMilliHz(uint32_t timeoutMs) {
        const uint32_t ticks = periodTicks(timeoutMs);
        if (ticks == 0) {
            return 0;
        }
        return static_cast<uint32_t>((static_cast<uint64_t>(ticksPerSecond()) * 1000u) / ticks);
    }
};

// ESP8266 implementation on a GPIO with an IRAM interrupt and the CPU cycle counter.
class EspPulseTimer final : public PulseTimer {
public:
    explicit EspPulseTimer(uint8_t pin) : pin_(pin) {}
    void begin();
    uint32_t periodTicks(uint32_t timeoutMs) override;
    uint32_t ticksPerSecond() const override;

private:
    uint8_t pin_;
};

} // namespace gbsr::hal
