#pragma once

#include <DNSServer.h>
#include <stdint.h>

#include <ESP8266WiFi.h>

#include "platform/storage.h"

namespace gbsr::net {

// Wi-Fi provisioning (ADR 0011; user decisions of 2026-10-13): join the saved network; when there
// is none, or it stays unreachable, open the setup access point "GBS-XXXX" (WPA2, the documented
// password) with a captive DNS so any page lands on the web UI. The access point closes once the
// board has joined. The board answers as gbs.local in both cases. Credentials live in their own
// file, never in the settings or a profile.
//
// A join first scans, then tries each access point of the network from the strongest down, so a
// mesh node that misbehaves cannot keep the board out while another node of the same network would
// let it in. Each failure is classified (refused, not found, no answer) for the setup page: a
// mistyped password otherwise looks like a board that "connects" (docs/bench/M1.md, "Wi-Fi").
class Wifi {
public:
    enum class Mode : uint8_t {
        Joining, // trying the saved network after boot or a new join request
        Joined,
        Setup, // access point up; the saved network (if any) is retried while nobody is connected
    };

    // Why the last join did not succeed, for the setup page.
    enum class Failure : uint8_t { None, Refused, NotFound, NoAnswer };

    static constexpr char kApPassword[] =
        "qqqqqqqq"; // as the community firmware: familiar to its users
    static constexpr char kHostname[] = "gbs";

    explicit Wifi(platform::FileStore& fs) : fs_(fs) {}

    void begin(uint32_t nowMs);
    void tick(uint32_t nowMs);

    // Saves the credentials and tries them now; false when they cannot be valid.
    bool join(const char* ssid, const char* password, uint32_t nowMs);

    Mode mode() const { return mode_; }
    Failure lastFailure() const { return failure_; }
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
    struct Candidate {
        uint8_t bssid[6];
        int32_t channel;
        int8_t rssi;
    };

    void startJoin(uint32_t nowMs);
    void pickCandidates();
    void beginAttempt(uint32_t nowMs);
    void joinFailed(uint32_t nowMs);
    void openAccessPoint();
    void closeAccessPoint();
    bool loadCredentials();

    platform::FileStore& fs_;
    DNSServer dns_;
    WiFiEventHandler onDisconnect_; // records the SDK's reason only (ADR 0003)
    Mode mode_ = Mode::Setup;
    Failure failure_ = Failure::None;
    bool apActive_ = false;
    bool scanStarted_ = false;
    char ssid_[33] = {};
    char password_[65] = {};
    char apSsid_[12] = {};
    Candidate candidates_[4] = {};
    uint8_t candidateCount_ = 0;
    uint8_t candidate_ = 0;
    bool joinScanning_ = false;
    uint32_t attemptStartMs_ = 0;
    uint32_t phaseStartMs_ = 0;
    uint32_t lostSinceMs_ = 0;
    uint32_t joinedMs_ = 0;
};

} // namespace gbsr::net
