#pragma once

#include <DNSServer.h>
#include <stdint.h>

#include "platform/storage.h"

namespace gbsr::net {

// Wi-Fi provisioning (ADR 0011; user decisions of 2026-10-13): join the saved network; when there
// is none, or it stays unreachable, open the setup access point "GBS-XXXX" (WPA2, the documented
// password) with a captive DNS so any page lands on the web UI. The access point closes once the
// board has joined. The board answers as gbs.local in both cases. Credentials live in their own
// file, never in the settings or a profile.
class Wifi {
public:
    enum class Mode : uint8_t {
        Joining, // trying the saved network after boot or a new join request
        Joined,
        Setup, // access point up; the saved network (if any) is retried while nobody is connected
    };

    static constexpr char kApPassword[] = "gbsreimagined";
    static constexpr char kHostname[] = "gbs";

    explicit Wifi(platform::FileStore& fs) : fs_(fs) {}

    void begin(uint32_t nowMs);
    void tick(uint32_t nowMs);

    // Saves the credentials and tries them now; false when they cannot be valid.
    bool join(const char* ssid, const char* password, uint32_t nowMs);

    Mode mode() const { return mode_; }
    bool apActive() const { return apActive_; }
    const char* ssid() const { return ssid_; }
    const char* apSsid() const { return apSsid_; }
    uint32_t stationIp() const; // 0 unless joined
    uint32_t apIp() const;      // 0 unless the access point is up
    int8_t rssi() const;

    // Network scan for the setup page: starts one if none is running. Returns -1 while running,
    // otherwise the number of networks found.
    int scan();

private:
    void startJoin(uint32_t nowMs);
    void openAccessPoint();
    void closeAccessPoint();
    bool loadCredentials();

    platform::FileStore& fs_;
    DNSServer dns_;
    Mode mode_ = Mode::Setup;
    bool apActive_ = false;
    bool scanStarted_ = false;
    char ssid_[33] = {};
    char password_[65] = {};
    char apSsid_[12] = {};
    uint32_t phaseStartMs_ = 0;
    uint32_t lostSinceMs_ = 0;
    uint32_t joinedMs_ = 0;
};

} // namespace gbsr::net
