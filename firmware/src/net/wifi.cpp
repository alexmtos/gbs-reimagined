#include "net/wifi.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include "core/record.h"

namespace gbsr::net {

namespace {

constexpr char kPath[] = "/wifi.bin";
constexpr uint32_t kMagic = 0x57524247; // "GBRW"
constexpr uint8_t kVersion = 1;
constexpr uint32_t kJoinTimeoutMs = 30'000; // then the access point opens
constexpr uint32_t kLostGraceMs = 30'000;   // a joined network may drop this long
constexpr uint32_t kRetryEveryMs = 120'000; // setup mode retries the saved network
constexpr uint32_t kRetryWindowMs = 15'000;
constexpr uint32_t kApLingerMs = 60'000; // after joining, while a phone is still on the AP

} // namespace

void Wifi::begin(uint32_t nowMs) {
    // The SDK would otherwise rewrite its own flash sector on every begin().
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apSsid_, sizeof apSsid_, "GBS-%02X%02X", mac[4], mac[5]);
    WiFi.hostname(kHostname);
    if (loadCredentials()) {
        startJoin(nowMs);
    } else {
        WiFi.mode(WIFI_AP);
        openAccessPoint();
        mode_ = Mode::Setup;
        phaseStartMs_ = nowMs;
    }
    MDNS.begin(kHostname);
    MDNS.addService("http", "tcp", 80);
}

bool Wifi::loadCredentials() {
    uint8_t buf[sizeof ssid_ + sizeof password_];
    if (core::loadRecord(fs_, kPath, kMagic, kVersion, buf, sizeof buf) !=
        static_cast<int>(sizeof buf)) {
        return false;
    }
    memcpy(ssid_, buf, sizeof ssid_);
    memcpy(password_, buf + sizeof ssid_, sizeof password_);
    ssid_[sizeof ssid_ - 1] = '\0';
    password_[sizeof password_ - 1] = '\0';
    return ssid_[0] != '\0';
}

bool Wifi::join(const char* ssid, const char* password, uint32_t nowMs) {
    if (ssid == nullptr || *ssid == '\0' || strlen(ssid) >= sizeof ssid_ ||
        strlen(password) >= sizeof password_) {
        return false;
    }
    memset(ssid_, 0, sizeof ssid_);
    memset(password_, 0, sizeof password_);
    strncpy(ssid_, ssid, sizeof ssid_ - 1);
    strncpy(password_, password, sizeof password_ - 1);
    uint8_t buf[sizeof ssid_ + sizeof password_];
    memcpy(buf, ssid_, sizeof ssid_);
    memcpy(buf + sizeof ssid_, password_, sizeof password_);
    core::saveRecord(fs_, kPath, kMagic, kVersion, buf, sizeof buf); // kept for the next boot
    startJoin(nowMs);
    return true;
}

void Wifi::startJoin(uint32_t nowMs) {
    // Keep the access point while trying, so a phone configuring through it is not cut off.
    WiFi.mode(apActive_ ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(ssid_, password_);
    mode_ = Mode::Joining;
    phaseStartMs_ = nowMs;
}

void Wifi::openAccessPoint() {
    if (apActive_) {
        return;
    }
    WiFi.mode((WiFi.getMode() & WIFI_STA) != 0 ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP(apSsid_, kApPassword);
    dns_.setErrorReplyCode(DNSReplyCode::NoError);
    dns_.start(53, "*", WiFi.softAPIP()); // every name answers with the board: a captive portal
    apActive_ = true;
}

void Wifi::closeAccessPoint() {
    if (!apActive_) {
        return;
    }
    dns_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apActive_ = false;
}

void Wifi::tick(uint32_t nowMs) {
    if (apActive_) {
        dns_.processNextRequest();
    }
    MDNS.update();
    const bool connected = WiFi.status() == WL_CONNECTED;
    switch (mode_) {
    case Mode::Joining:
        if (connected) {
            mode_ = Mode::Joined;
            joinedMs_ = nowMs;
            lostSinceMs_ = 0;
        } else if (nowMs - phaseStartMs_ >= (apActive_ ? kRetryWindowMs : kJoinTimeoutMs)) {
            WiFi.disconnect(false); // stop scanning for it: that would disturb the access point
            openAccessPoint();
            mode_ = Mode::Setup;
            phaseStartMs_ = nowMs;
        }
        break;
    case Mode::Joined:
        if (connected) {
            lostSinceMs_ = 0;
            // Close the access point once nobody uses it, or after a minute regardless.
            if (apActive_ && (WiFi.softAPgetStationNum() == 0 || nowMs - joinedMs_ > kApLingerMs)) {
                closeAccessPoint();
            }
        } else if (lostSinceMs_ == 0) {
            lostSinceMs_ = nowMs | 1;
        } else if (nowMs - lostSinceMs_ >= kLostGraceMs) {
            WiFi.disconnect(false);
            openAccessPoint();
            mode_ = Mode::Setup;
            phaseStartMs_ = nowMs;
        }
        break;
    case Mode::Setup:
        if (ssid_[0] != '\0' && nowMs - phaseStartMs_ >= kRetryEveryMs &&
            WiFi.softAPgetStationNum() == 0) {
            startJoin(nowMs);
        }
        break;
    }
}

uint32_t Wifi::stationIp() const {
    return mode_ == Mode::Joined ? static_cast<uint32_t>(WiFi.localIP()) : 0;
}

uint32_t Wifi::apIp() const {
    return apActive_ ? static_cast<uint32_t>(WiFi.softAPIP()) : 0;
}

int8_t Wifi::rssi() const {
    return mode_ == Mode::Joined ? static_cast<int8_t>(WiFi.RSSI()) : 0;
}

int Wifi::scan() {
    const int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
        return -1;
    }
    if (n >= 0 && scanStarted_) {
        scanStarted_ = false; // the caller reads these results; the next call scans again
        return n;
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false);
    scanStarted_ = true;
    return -1;
}

} // namespace gbsr::net
