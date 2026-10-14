#if defined(ARDUINO)

#include <Arduino.h>

#include "hal/encoder.h"

namespace gbsr::hal {

namespace {

constexpr int kTransitionsPerDetent = 4;

volatile int32_t gTransitions = 0;
volatile uint8_t gState = 0;
uint8_t gPinA = 0;
uint8_t gPinB = 0;

// Quadrature table: previous (2 bits) and current (2 bits) A/B state to -1, 0 or +1. Invalid jumps
// (both pins changed: a missed edge) count nothing.
constexpr int8_t kStep[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void IRAM_ATTR onEdge() {
    const uint32_t in = GPI; // both pins in one read
    const uint8_t now = static_cast<uint8_t>(((in >> gPinA) & 1) << 1 | ((in >> gPinB) & 1));
    gTransitions += kStep[(gState << 2) | now];
    gState = now;
}

} // namespace

void EspEncoder::begin() {
    gPinA = pinA_;
    gPinB = pinB_;
    pinMode(pinA_, INPUT_PULLUP);
    pinMode(pinB_, INPUT_PULLUP);
    pinMode(pinButton_, INPUT_PULLUP);
    gState = static_cast<uint8_t>(digitalRead(pinA_) << 1 | digitalRead(pinB_));
    attachInterrupt(digitalPinToInterrupt(pinA_), onEdge, CHANGE);
    attachInterrupt(digitalPinToInterrupt(pinB_), onEdge, CHANGE);
}

int EspEncoder::takeDetents() {
    noInterrupts();
    const int32_t t = gTransitions;
    gTransitions = 0;
    interrupts();
    remainder_ += t;
    const int detents = remainder_ / kTransitionsPerDetent;
    remainder_ -= detents * kTransitionsPerDetent;
    return sign_ * detents;
}

bool EspEncoder::buttonDown() const {
    return digitalRead(pinButton_) == LOW;
}

} // namespace gbsr::hal

#endif
