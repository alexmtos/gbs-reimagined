#pragma once

#include <stdint.h>

#include "hal/i2c_bus.h"
#include "ui/menu.h"

namespace gbsr::ui {

// The SSD1306 128×64 OLED through U8g2 (ADR 0011): full-frame buffer, Latin-1 6×10 font, written
// over our own I2C bus with a custom byte callback so U8g2 never reconfigures the shared bus. Draws
// a Frame of six text lines; a highlighted line is drawn inverted.
class Oled {
public:
    explicit Oled(hal::I2cBus& bus, uint8_t address) : bus_(bus), address_(address) {}

    // False when no display answers; every other call is then a no-op.
    bool begin();
    // Sends the frame if it differs from the one on screen (about 26 ms of bus time at 400 kHz).
    void show(const Frame& f);
    void sleep(bool asleep);
    bool present() const { return present_; }

private:
    hal::I2cBus& bus_;
    uint8_t address_;
    bool present_ = false;
    bool asleep_ = false;
    Frame shown_;
    bool shownValid_ = false;
};

} // namespace gbsr::ui
