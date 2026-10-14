#if defined(ARDUINO)

#include <Arduino.h>

#include "hal/pulse_timer.h"

namespace gbsr::hal {

namespace {

volatile uint32_t gStart = 0;
volatile uint32_t gStop = 0;
volatile uint8_t gEdges = 0;

void IRAM_ATTR onRisingEdge() {
    const uint32_t now = ESP.getCycleCount();
    if (gEdges == 0) {
        gStart = now;
        gEdges = 1;
    } else if (gEdges == 1) {
        gStop = now;
        gEdges = 2;
    }
}

} // namespace

void EspPulseTimer::begin() {
    pinMode(pin_, INPUT);
}

uint32_t EspPulseTimer::periodTicks(uint32_t timeoutMs) {
    gEdges = 0;
    attachInterrupt(digitalPinToInterrupt(pin_), onRisingEdge, RISING);
    const uint32_t start = millis();
    while (gEdges < 2 && millis() - start < timeoutMs) {
        yield();
    }
    // Detached on every path: an armed interrupt left behind would keep firing until the next
    // measurement (a defect the reference project recorded).
    detachInterrupt(digitalPinToInterrupt(pin_));
    if (gEdges < 2) {
        return 0;
    }
    return gStop - gStart;
}

uint32_t EspPulseTimer::ticksPerSecond() const {
    return ESP.getCpuFreqMHz() * 1000000u;
}

} // namespace gbsr::hal

#endif
