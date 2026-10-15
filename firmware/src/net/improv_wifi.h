#pragma once

#include "core/improv_serial.h"
#include "net/wifi.h"

namespace gbsr::net {

// Improv's view of the board: UART0 and the Wi-Fi module.
class ImprovWifi final : public core::ImprovSerial::Board {
public:
    explicit ImprovWifi(Wifi& wifi) : wifi_(wifi) {}

    void send(const uint8_t* data, size_t len) override;
    bool join(const char* ssid, const char* password) override;
    Link link() const override;
    int scan() override;
    bool network(int index, Network& out) const override;
    bool address(char* url, size_t cap) const override;

private:
    Wifi& wifi_;
};

} // namespace gbsr::net
