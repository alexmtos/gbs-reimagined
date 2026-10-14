#pragma once

#include <stdint.h>

namespace gbsr::ui {

// Turns raw knob readings (button level, detents turned) into the gestures of docs/design/ux.md:
// press (released before 0.6 s, without turning), long press (held 0.6 s without turning; fires
// once while still held) and turns, which count ×10 ("coarse") while the button is held. Turning
// while held cancels the press. Pure logic, host-tested.
class Knob {
public:
    static constexpr uint32_t kLongPressMs = 600;
    static constexpr uint32_t kDebounceMs = 30;

    struct Events {
        int turn = 0;
        bool coarse = false;
        bool press = false;
        bool longPress = false;
    };

    Events update(bool buttonDown, int detents, uint32_t nowMs);

private:
    bool down_ = false; // debounced level
    bool rawDown_ = false;
    uint32_t rawSinceMs_ = 0;
    uint32_t downSinceMs_ = 0;
    bool turnedWhileDown_ = false;
    bool longFired_ = false;
};

} // namespace gbsr::ui
