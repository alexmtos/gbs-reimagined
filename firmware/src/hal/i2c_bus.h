#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gbsr::hal {

// Byte-level I2C transactions. Implementations must complete a transaction without yielding
// to the network stack: a yield between the pieces of one access lets another context reuse the
// bus mid-transaction (see docs/research/reference-lessons.md, "bus layer").
class I2cBus {
public:
    virtual ~I2cBus() = default;

    // Writes `len` bytes to the device. Returns false on NACK or bus error.
    virtual bool write(uint8_t addr, const uint8_t* data, size_t len) = 0;

    // Writes `txLen` bytes (usually a register pointer), then reads exactly `rxLen` bytes with a
    // repeated start. Returns false unless every requested byte arrived.
    virtual bool writeRead(uint8_t addr, const uint8_t* tx, size_t txLen, uint8_t* rx,
                           size_t rxLen) = 0;

    // True when a device acknowledges its address.
    virtual bool probe(uint8_t addr) = 0;

    bool writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
        const uint8_t buf[2] = {reg, value};
        return write(addr, buf, sizeof buf);
    }

    bool readReg(uint8_t addr, uint8_t reg, uint8_t& value) {
        return writeRead(addr, &reg, 1, &value, 1);
    }
};

} // namespace gbsr::hal
