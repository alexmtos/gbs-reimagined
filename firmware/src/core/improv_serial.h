#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gbsr::core {

// Improv Wi-Fi Serial (https://www.improv-wifi.com/serial/, ADR 0011): the browser installer hands
// the board a Wi-Fi network over the USB serial port right after flashing it. Improv packets share
// UART0 with the console: feed() takes every received byte and gives back, in order, the ones that
// turn out not to be Improv, for the console to read.
//
// What the installer's client (improv-wifi-serial-sdk 2.8.0, inside ESP Web Tools 10.4.0) expects,
// read from its source: a packet only counts when it starts a line; it asks for the state once a
// second until the board answers; it gives a join 45 s before giving up.
class ImprovSerial {
public:
    // The board side: the serial port and the Wi-Fi (net::ImprovWifi; a fake in tests).
    class Board {
    public:
        enum class Link : uint8_t {
            Joining, // trying a network
            Joined,
            Failed, // no network to try, or the last try failed
        };
        struct Network {
            char ssid[33];
            int8_t rssi;
            bool secured;
        };

        virtual ~Board() = default;
        virtual void send(const uint8_t* data, size_t len) = 0;
        // Saves the network and tries it; false when it cannot be valid.
        virtual bool join(const char* ssid, const char* password) = 0;
        virtual Link link() const = 0;
        // Starts a scan if none is running: -1 while it runs, then the number of networks heard.
        virtual int scan() = 0;
        virtual bool network(int index, Network& out) const = 0;
        // The page to open once joined ("http://<address>/"); false while not joined.
        virtual bool address(char* url, size_t cap) const = 0;
    };

    // What the installer shows; it compares `firmware` and `version` with its manifest's `name` and
    // `version` to tell an update from another firmware. Each may be flash text.
    struct Identity {
        const char* firmware;
        const char* version;
        const char* chip;
        const char* device;
    };

    // Bytes for the console, in order; empty while a packet may be forming.
    struct Spill {
        const uint8_t* data;
        uint8_t len;
    };

    ImprovSerial(Board& board, const Identity& id) : board_(board), id_(id) {}

    Spill feed(uint8_t byte, uint32_t nowMs);
    // Answers that wait: a join, a scan, the state asked while the saved network is being joined.
    void tick(uint32_t nowMs);

    bool provisioning() const { return joining_; }

private:
    void handleRpc(uint32_t nowMs);
    void answerState();
    void sendState(uint8_t state);
    void sendError(uint8_t error);
    void sendResult(uint8_t command, const char* const* strings, uint8_t count);
    void sendPacket(uint8_t type, const uint8_t* data, size_t len);
    void sendNextNetwork();

    Board& board_;
    Identity id_;

    // The packet being read: type, data length, data (what this board accepts fits), checksum.
    static constexpr uint8_t kMaxData = 100; // the longest Wi-Fi settings: 2 + 1 + 32 + 1 + 64
    uint16_t pos_ = 0;                       // up to 9 + 255 + 1
    uint8_t type_ = 0;
    uint8_t len_ = 0;
    uint8_t sum_ = 0;
    uint8_t data_[kMaxData + 1] = {}; // + 1: the library's parser may look one byte past the end
    uint32_t lastByteMs_ = 0;
    uint8_t spill_[8] = {};

    bool joining_ = false; // a join asked over Improv is running
    uint32_t joinStartMs_ = 0;
    bool stateAsked_ = false; // asked while the saved network was being joined
    uint32_t stateAskMs_ = 0;
    enum class Scan : uint8_t { Idle, Waiting, Sending };
    Scan scan_ = Scan::Idle;
    uint32_t scanAskMs_ = 0;
    int16_t scanNext_ = 0; // the next network to send
    int16_t scanCount_ = 0;
};

} // namespace gbsr::core
