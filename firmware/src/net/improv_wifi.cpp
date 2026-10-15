#include "net/improv_wifi.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace gbsr::net {

void ImprovWifi::send(const uint8_t* data, size_t len) {
    Serial.write(data, len);
}

bool ImprovWifi::join(const char* ssid, const char* password) {
    return wifi_.join(ssid, password, millis());
}

ImprovWifi::Link ImprovWifi::link() const {
    switch (wifi_.mode()) {
    case Wifi::Mode::Joining:
        return Link::Joining;
    case Wifi::Mode::Joined:
        return Link::Joined;
    case Wifi::Mode::Setup:
        break;
    }
    return Link::Failed;
}

int ImprovWifi::scan() {
    // A scan in the middle of a join attempt would disturb it; Improv gives up waiting in time.
    return wifi_.mode() == Wifi::Mode::Joining ? -1 : wifi_.scan();
}

bool ImprovWifi::network(int index, Network& out) const {
    // Read in place: WiFi.SSID(i) would build a String for every comparison.
    if (index < 0 || index >= WiFi.scanComplete()) {
        return false;
    }
    const bss_info* bss = WiFi.getScanInfoByIndex(index);
    if (bss == nullptr) {
        return false;
    }
    const size_t n = bss->ssid_len < sizeof out.ssid - 1 ? bss->ssid_len : sizeof out.ssid - 1;
    memcpy(out.ssid, bss->ssid, n);
    out.ssid[n] = '\0';
    out.rssi = bss->rssi;
    out.secured = bss->authmode != AUTH_OPEN;
    return true;
}

bool ImprovWifi::address(char* url, size_t cap) const {
    const uint32_t ip = wifi_.stationIp();
    if (ip == 0) {
        return false;
    }
    snprintf_P(url, cap, PSTR("http://%u.%u.%u.%u/"), ip & 0xFF, ip >> 8 & 0xFF, ip >> 16 & 0xFF,
               ip >> 24);
    return true;
}

} // namespace gbsr::net
