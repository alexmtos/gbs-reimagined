#pragma once

#include "hal/i2c_bus.h"

namespace gbsr::hal {

// I2C on the ESP8266 through the core's bit-banged `twi_*` driver, bypassing `Wire`:
// `TwoWire::available()` yields when its buffer is empty, which would let the network stack run
// in the middle of a register access. The twi functions do not yield (the TV5725 never stretches
// the clock, so the stretch wait never runs).
//
// Reads write the register pointer, send STOP, then read: the TV5725 has been driven this way on
// the bench for years, so no repeated start is used.
class EspI2cBus final : public I2cBus {
public:
    // Frees a bus left mid-transaction by a reset (SDA held low), then starts the driver.
    // Returns false if SDA or SCL is still held low afterwards.
    bool begin(uint8_t sdaPin, uint8_t sclPin, uint32_t clockHz);

    bool write(uint8_t addr, const uint8_t* data, size_t len) override;
    bool writeRead(uint8_t addr, const uint8_t* tx, size_t txLen, uint8_t* rx,
                   size_t rxLen) override;
    bool probe(uint8_t addr) override;

    uint32_t errorCount() const { return errors_; }

private:
    bool recover();

    uint8_t sda_ = 0;
    uint8_t scl_ = 0;
    uint32_t errors_ = 0;
};

} // namespace gbsr::hal
