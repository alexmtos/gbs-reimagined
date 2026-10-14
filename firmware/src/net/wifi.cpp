#include "net/wifi.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include "core/record.h"

namespace gbsr::net {

namespace {

constexpr char kPath[] = "/wifi.bin";
constexpr uint32_t kMagic = 0x57524247; // "GBRW"
constexpr uint8_t kVersion = 1;
constexpr uint32_t kJoinScanMs = 8'000;     // the scan before a join gives up after this
constexpr uint32_t kAttemptMs = 12'000;     // per access point of the network
constexpr uint32_t kVerdictMs = 3'000;      // a refusal counts once the attempt is this old
constexpr uint32_t kLostGraceMs = 30'000;   // a joined network may drop this long
constexpr uint32_t kRetryEveryMs = 120'000; // setup mode retries the saved network
constexpr uint32_t kApLingerMs = 60'000;    // after joining, while a phone is still on the AP

// Written by the SDK's event callback, read by the main loop for the log.
volatile uint8_t gDisconnectReason = 0;

void logIp(const char* what, uint32_t ip) {
    Serial.printf_P(PSTR("wifi: %s %u.%u.%u.%u\r\n"), what, ip & 0xFF, ip >> 8 & 0xFF,
                    ip >> 16 & 0xFF, ip >> 24);
}

} // namespace

void Wifi::begin(uint32_t nowMs) {
    // The SDK would otherwise rewrite its own flash sector on every begin().
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apSsid_, sizeof apSsid_, "GBS-%02X%02X", mac[4], mac[5]);
    WiFi.hostname(kHostname);
    onDisconnect_ = WiFi.onStationModeDisconnected(
        [](const WiFiEventStationModeDisconnected& e) { gDisconnectReason = e.reason; });
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
    WiFi.disconnect(false);
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false);
    joinScanning_ = true;
    scanStarted_ = false; // a scan the setup page asked for is superseded
    failure_ = Failure::None;
    mode_ = Mode::Joining;
    phaseStartMs_ = nowMs;
    Serial.printf_P(PSTR("wifi: joining \"%s\"\r\n"), ssid_);
}

// The access points of the saved network, strongest first.
void Wifi::pickCandidates() {
    candidateCount_ = 0;
    const int n = WiFi.scanComplete();
    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) != ssid_) {
            continue;
        }
        Candidate c{};
        memcpy(c.bssid, WiFi.BSSID(i), sizeof c.bssid);
        c.channel = WiFi.channel(i);
        c.rssi = static_cast<int8_t>(WiFi.RSSI(i));
        uint8_t at = candidateCount_ < 4 ? candidateCount_ : 4;
        while (at > 0 && candidates_[at - 1].rssi < c.rssi) {
            if (at < 4) {
                candidates_[at] = candidates_[at - 1];
            }
            --at;
        }
        if (at < 4) {
            candidates_[at] = c;
            candidateCount_ = candidateCount_ < 4 ? candidateCount_ + 1 : 4;
        }
    }
    WiFi.scanDelete();
}

void Wifi::beginAttempt(uint32_t nowMs) {
    attemptStartMs_ = nowMs;
    gDisconnectReason = 0;
    if (candidateCount_ == 0) {
        // Not heard (hidden, or out of range for the scan): let the SDK look for it.
        WiFi.begin(ssid_, password_);
        Serial.println(F("wifi: network not heard in the scan; trying by name"));
        return;
    }
    const Candidate& c = candidates_[candidate_];
    WiFi.begin(ssid_, password_, c.channel, c.bssid);
    Serial.printf_P(PSTR("wifi: trying access point %u of %u (%d dBm, channel %ld)\r\n"),
                    candidate_ + 1, candidateCount_, c.rssi, static_cast<long>(c.channel));
}

void Wifi::joinFailed(uint32_t nowMs) {
    const wl_status_t st = WiFi.status();
    Serial.printf_P(PSTR("wifi: attempt failed (status %d, reason %u)\r\n"), st, gDisconnectReason);
    // A refusal on any access point outranks "not found" or silence on another.
    if (st == WL_WRONG_PASSWORD || st == WL_CONNECT_FAILED) {
        failure_ = Failure::Refused;
    } else if (failure_ != Failure::Refused) {
        failure_ =
            st == WL_NO_SSID_AVAIL || candidateCount_ == 0 ? Failure::NotFound : Failure::NoAnswer;
    }
    WiFi.disconnect(false); // stop the SDK retrying this one on its own
    if (candidate_ + 1 < candidateCount_) {
        ++candidate_;
        beginAttempt(nowMs);
        return;
    }
    Serial.println(F("wifi: could not join; opening the setup access point"));
    openAccessPoint();
    mode_ = Mode::Setup;
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
    Serial.printf_P(PSTR("wifi: setup access point \"%s\" open\r\n"), apSsid_);
}

void Wifi::closeAccessPoint() {
    if (!apActive_) {
        return;
    }
    dns_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apActive_ = false;
    Serial.println(F("wifi: setup access point closed"));
}

void Wifi::tick(uint32_t nowMs) {
    if (apActive_) {
        dns_.processNextRequest();
    }
    MDNS.update();
    const bool connected = WiFi.status() == WL_CONNECTED;
    switch (mode_) {
    case Mode::Joining: {
        if (joinScanning_) {
            if (WiFi.scanComplete() == WIFI_SCAN_RUNNING && nowMs - phaseStartMs_ < kJoinScanMs) {
                break;
            }
            joinScanning_ = false;
            pickCandidates();
            candidate_ = 0;
            beginAttempt(nowMs);
            break;
        }
        if (connected) {
            mode_ = Mode::Joined;
            joinedMs_ = nowMs;
            lostSinceMs_ = 0;
            logIp("joined as", static_cast<uint32_t>(WiFi.localIP()));
            break;
        }
        const wl_status_t st = WiFi.status();
        const bool refused =
            st == WL_WRONG_PASSWORD || st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL;
        const uint32_t age = nowMs - attemptStartMs_;
        if ((refused && age >= kVerdictMs) || age >= kAttemptMs) {
            joinFailed(nowMs);
        }
        break;
    }
    case Mode::Joined:
        if (connected) {
            if (lostSinceMs_ != 0) {
                logIp("back as", static_cast<uint32_t>(WiFi.localIP()));
            }
            lostSinceMs_ = 0;
            // Close the access point once nobody uses it, or after a minute regardless.
            if (apActive_ && (WiFi.softAPgetStationNum() == 0 || nowMs - joinedMs_ > kApLingerMs)) {
                closeAccessPoint();
            }
        } else if (lostSinceMs_ == 0) {
            lostSinceMs_ = nowMs | 1;
            Serial.printf_P(PSTR("wifi: connection lost (status %d, reason %u)\r\n"), WiFi.status(),
                            gDisconnectReason);
        } else if (nowMs - lostSinceMs_ >= kLostGraceMs) {
            Serial.println(F("wifi: still lost after 30 s"));
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
    if (joinScanning_) {
        return -1; // the join's own scan is running; ask again shortly
    }
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
