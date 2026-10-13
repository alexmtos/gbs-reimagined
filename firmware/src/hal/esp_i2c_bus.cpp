#include "hal/esp_i2c_bus.h"

#include <twi.h>

#include <Arduino.h>

namespace gbsr::hal {

namespace {

constexpr int kRecoveryClocks = 16;   // a stuck slave releases SDA within nine clocks
constexpr uint32_t kHalfPeriodUs = 5; // ~100 kHz while recovering

} // namespace

bool EspI2cBus::recover() {
    pinMode(sda_, INPUT_PULLUP);
    pinMode(scl_, INPUT_PULLUP);
    delayMicroseconds(kHalfPeriodUs);
    for (int i = 0; i < kRecoveryClocks && digitalRead(sda_) == LOW; ++i) {
        pinMode(scl_, OUTPUT);
        digitalWrite(scl_, LOW);
        delayMicroseconds(kHalfPeriodUs);
        pinMode(scl_, INPUT_PULLUP);
        delayMicroseconds(kHalfPeriodUs);
    }
    // Generate a STOP: SDA low -> high while SCL is high.
    pinMode(sda_, OUTPUT);
    digitalWrite(sda_, LOW);
    delayMicroseconds(kHalfPeriodUs);
    pinMode(sda_, INPUT_PULLUP);
    delayMicroseconds(kHalfPeriodUs);
    return digitalRead(sda_) == HIGH && digitalRead(scl_) == HIGH;
}

bool EspI2cBus::begin(uint8_t sdaPin, uint8_t sclPin, uint32_t clockHz) {
    sda_ = sdaPin;
    scl_ = sclPin;
    const bool free = recover();
    twi_init(sda_, scl_);
    twi_setClock(clockHz);
    twi_setClockStretchLimit(1500); // microseconds; only the OLED could ever stretch
    return free && twi_status() == I2C_OK;
}

bool EspI2cBus::write(uint8_t addr, const uint8_t* data, size_t len) {
    // twi_writeTo takes a non-const buffer but does not modify it.
    if (twi_writeTo(addr, const_cast<uint8_t*>(data), len, true) != 0) {
        ++errors_;
        return false;
    }
    return true;
}

bool EspI2cBus::writeRead(uint8_t addr, const uint8_t* tx, size_t txLen, uint8_t* rx,
                          size_t rxLen) {
    if (twi_writeTo(addr, const_cast<uint8_t*>(tx), txLen, true) != 0 ||
        twi_readFrom(addr, rx, rxLen, true) != 0) {
        ++errors_;
        return false;
    }
    return true;
}

bool EspI2cBus::probe(uint8_t addr) {
    return twi_writeTo(addr, nullptr, 0, true) == 0;
}

} // namespace gbsr::hal
