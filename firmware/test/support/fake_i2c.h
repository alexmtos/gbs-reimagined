#pragma once

#include <map>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "hal/i2c_bus.h"

namespace gbsr::test {

// A device on the fake bus: a register file with an auto-incrementing pointer.
class FakeDevice {
public:
    virtual ~FakeDevice() = default;
    virtual bool write(const uint8_t* data, size_t len) = 0;
    virtual bool read(uint8_t reg, uint8_t* out, size_t len) = 0;
};

// Model of the TV5725's I2C slave: six 256-byte segments, segment select at 0xF0, pointer
// auto-increment, and refusal of reads longer than 16 bytes (the real chip answers those with
// nothing at all).
class FakeTv5725 : public FakeDevice {
public:
    uint8_t regs[6][256] = {};
    uint8_t segment = 0;
    int segmentSelects = 0;
    std::vector<std::pair<uint16_t, uint8_t>> writes; // (seg << 8 | addr, value)

    uint8_t& at(uint8_t seg, uint8_t addr) { return regs[seg][addr]; }

    bool write(const uint8_t* data, size_t len) override {
        if (len == 0) {
            return true;
        }
        uint8_t ptr = data[0];
        for (size_t i = 1; i < len; ++i, ++ptr) {
            if (ptr == 0xF0) {
                if (data[i] > 5) {
                    return false;
                }
                segment = data[i];
                ++segmentSelects;
                continue;
            }
            regs[segment][ptr] = data[i];
            writes.emplace_back(static_cast<uint16_t>(segment << 8 | ptr), data[i]);
        }
        return true;
    }

    bool read(uint8_t reg, uint8_t* out, size_t len) override {
        if (len > 16) {
            return false;
        }
        for (size_t i = 0; i < len; ++i) {
            out[i] = regs[segment][static_cast<uint8_t>(reg + i)];
        }
        return true;
    }
};

// Flat 256-register device (Si5351, OLED controller).
class FakeFlatDevice : public FakeDevice {
public:
    uint8_t regs[256] = {};
    std::vector<std::pair<uint8_t, uint8_t>> writes;

    bool write(const uint8_t* data, size_t len) override {
        if (len == 0) {
            return true;
        }
        uint8_t ptr = data[0];
        for (size_t i = 1; i < len; ++i, ++ptr) {
            regs[ptr] = data[i];
            writes.emplace_back(ptr, data[i]);
        }
        return true;
    }

    bool read(uint8_t reg, uint8_t* out, size_t len) override {
        for (size_t i = 0; i < len; ++i) {
            out[i] = regs[static_cast<uint8_t>(reg + i)];
        }
        return true;
    }
};

class FakeI2cBus : public hal::I2cBus {
public:
    std::map<uint8_t, FakeDevice*> devices;
    int transactions = 0;
    bool failNext = false;

    void attach(uint8_t addr, FakeDevice& dev) { devices[addr] = &dev; }

    bool write(uint8_t addr, const uint8_t* data, size_t len) override {
        ++transactions;
        if (consumeFailure()) {
            return false;
        }
        auto it = devices.find(addr);
        return it != devices.end() && it->second->write(data, len);
    }

    bool writeRead(uint8_t addr, const uint8_t* tx, size_t txLen, uint8_t* rx,
                   size_t rxLen) override {
        ++transactions;
        if (consumeFailure() || txLen != 1) {
            return false;
        }
        auto it = devices.find(addr);
        return it != devices.end() && it->second->read(tx[0], rx, rxLen);
    }

    bool probe(uint8_t addr) override { return devices.count(addr) != 0; }

private:
    bool consumeFailure() {
        const bool f = failNext;
        failNext = false;
        return f;
    }
};

} // namespace gbsr::test
