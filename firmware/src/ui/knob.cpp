#include "ui/knob.h"

namespace gbsr::ui {

Knob::Events Knob::update(bool buttonDown, int detents, uint32_t nowMs) {
    Events ev;
    if (buttonDown != rawDown_) {
        rawDown_ = buttonDown;
        rawSinceMs_ = nowMs;
    }
    const bool settled = nowMs - rawSinceMs_ >= kDebounceMs;
    if (settled && rawDown_ != down_) {
        down_ = rawDown_;
        if (down_) {
            downSinceMs_ = nowMs;
            turnedWhileDown_ = false;
            longFired_ = false;
        } else if (!turnedWhileDown_ && !longFired_) {
            ev.press = true;
        }
    }
    if (detents != 0) {
        ev.turn = detents;
        ev.coarse = down_;
        if (down_) {
            turnedWhileDown_ = true;
        }
    }
    if (down_ && !turnedWhileDown_ && !longFired_ && nowMs - downSinceMs_ >= kLongPressMs) {
        longFired_ = true;
        ev.longPress = true;
    }
    return ev;
}

} // namespace gbsr::ui
