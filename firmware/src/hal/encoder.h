#pragma once

#include <stdint.h>

namespace gbsr::hal {

// The board's rotary encoder with push button (docs/hardware/board-gbsc.md). The two quadrature
// pins raise interrupts that only update a transition counter (IRAM, ADR 0003); the main loop
// reads whole detents. The button sits on GPIO0, the boot strap, and is read as a plain level.
class EspEncoder {
public:
    EspEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinButton, bool clockwiseNegative)
        : pinA_(pinA), pinB_(pinB), pinButton_(pinButton), sign_(clockwiseNegative ? -1 : 1) {}

    void begin();
    // Detents turned since the last call, clockwise positive.
    int takeDetents();
    bool buttonDown() const;

private:
    uint8_t pinA_, pinB_, pinButton_;
    int sign_;
    int remainder_ = 0; // transitions not yet making a whole detent
};

} // namespace gbsr::hal
