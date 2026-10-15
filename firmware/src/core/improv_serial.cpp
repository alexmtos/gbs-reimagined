#include "core/improv_serial.h"

#include <improv.h>
#include <stdio.h>
#include <string.h>

#include "platform/progmem.h"

namespace gbsr::core {

namespace {

constexpr uint8_t kHeaderLen = 6; // "IMPROV", then the version byte
constexpr char kHeader[] PROGMEM = "IMPROV";
constexpr uint8_t kVersion = improv::IMPROV_SERIAL_VERSION;
constexpr uint8_t kDataAt = 9; // header, version, type, length

// A packet arrives in one burst (about 1 ms per 10 bytes at 115200 baud): a pause this long means
// it was cut short. Shorter than the client's 1 s retry, so the retry is read as a new packet.
constexpr uint32_t kGapMs = 500;
// The installer gives a join 45 s; answer before it gives up, even while the board still tries.
constexpr uint32_t kJoinLimitMs = 35'000;
// The state asked while the saved network is being joined waits this long for the outcome: the
// bench board joined 5.3 s after its loop started (docs/bench/M3.md). After flashing, the installer
// waits as long as its manifest's new_install_improv_wait_time; otherwise only 1.5 s (ESP Web
// Tools 10.4.0 source).
constexpr uint32_t kStateHoldMs = 8'000;
// A scan takes a few seconds. One that has not ended by then (the board is joining a network, or
// the scan failed) ends with an empty list rather than leaving the installer waiting.
constexpr uint32_t kScanLimitMs = 10'000;

uint8_t headerByte(uint8_t at) {
    return at < kHeaderLen ? pgm_read_byte(&kHeader[at]) : kVersion;
}

bool elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t ms) {
    return static_cast<int32_t>(nowMs - sinceMs) >= static_cast<int32_t>(ms);
}

} // namespace

ImprovSerial::Spill ImprovSerial::feed(uint8_t byte, uint32_t nowMs) {
    uint8_t n = 0;
    if (pos_ > 0 && nowMs - lastByteMs_ > kGapMs) {
        // Cut short: the start of a header was console input after all; a deeper part is dropped.
        for (uint8_t i = 0; pos_ <= kHeaderLen + 1 && i < pos_; ++i) {
            spill_[n++] = headerByte(i);
        }
        pos_ = 0;
    }
    lastByteMs_ = nowMs;
    if (pos_ <= kHeaderLen) {
        if (byte == headerByte(static_cast<uint8_t>(pos_))) {
            sum_ = static_cast<uint8_t>((pos_ == 0 ? 0 : sum_) + byte);
            ++pos_;
            return {spill_, n};
        }
        // Not a packet: the console gets what was held, then this byte unless it starts one.
        for (uint8_t i = 0; i < pos_; ++i) {
            spill_[n++] = headerByte(i);
        }
        pos_ = 0;
        if (byte == headerByte(0)) {
            sum_ = byte;
            pos_ = 1;
        } else {
            spill_[n++] = byte;
        }
        return {spill_, n};
    }
    if (pos_ == kHeaderLen + 1) {
        type_ = byte;
    } else if (pos_ == kHeaderLen + 2) {
        len_ = byte;
    } else if (pos_ - kDataAt < len_) {
        if (pos_ - kDataAt < kMaxData) {
            data_[pos_ - kDataAt] = byte;
        }
    } else {
        // The checksum: the sum of every byte before it.
        pos_ = 0;
        if (byte != sum_) {
            sendError(improv::ERROR_INVALID_RPC);
        } else if (type_ == improv::TYPE_RPC) {
            handleRpc(nowMs);
        }
        return {spill_, 0};
    }
    sum_ = static_cast<uint8_t>(sum_ + byte);
    ++pos_;
    return {spill_, 0};
}

void ImprovSerial::handleRpc(uint32_t nowMs) {
    sendError(improv::ERROR_NONE); // as the protocol asks, before any answer
    scan_ = Scan::Idle;            // a list still being sent was abandoned by the client
    if (len_ < 2 || len_ > kMaxData) {
        sendError(improv::ERROR_INVALID_RPC);
        return;
    }
    const improv::ImprovCommand cmd = improv::parse_improv_data(data_, len_, false);
    switch (cmd.command) {
    case improv::GET_CURRENT_STATE:
        if (!joining_ && board_.link() == Board::Link::Joining) {
            // The board has just started and is joining its saved network: answer once that is
            // settled, so a board that joins is not offered the Wi-Fi form.
            if (!stateAsked_) {
                stateAsked_ = true;
                stateAskMs_ = nowMs;
            }
            return;
        }
        answerState();
        return;
    case improv::GET_DEVICE_INFO: {
        const char* info[] = {id_.firmware, id_.version, id_.chip, id_.device};
        sendResult(improv::GET_DEVICE_INFO, info, 4);
        return;
    }
    case improv::GET_WIFI_NETWORKS:
        scan_ = Scan::Waiting;
        scanAskMs_ = nowMs;
        return;
    case improv::WIFI_SETTINGS:
        // The password goes to the Wi-Fi module only; nothing here prints it.
        if (!board_.join(cmd.ssid.c_str(), cmd.password.c_str())) {
            sendError(improv::ERROR_UNABLE_TO_CONNECT);
            sendState(improv::STATE_AUTHORIZED);
            return;
        }
        joining_ = true;
        joinStartMs_ = nowMs;
        stateAsked_ = false;
        sendState(improv::STATE_PROVISIONING);
        return;
    case improv::UNKNOWN: // the lengths inside disagree
        sendError(improv::ERROR_INVALID_RPC);
        return;
    default:
        sendError(improv::ERROR_UNKNOWN_RPC);
        return;
    }
}

void ImprovSerial::tick(uint32_t nowMs) {
    if (joining_) {
        const Board::Link link = board_.link();
        if (link == Board::Link::Joined) {
            joining_ = false;
            char url[40];
            if (!board_.address(url, sizeof url)) {
                url[0] = '\0';
            }
            sendState(improv::STATE_PROVISIONED);
            const char* result[] = {url};
            sendResult(improv::WIFI_SETTINGS, result, 1);
        } else if (link == Board::Link::Failed || elapsed(nowMs, joinStartMs_, kJoinLimitMs)) {
            joining_ = false;
            sendError(improv::ERROR_UNABLE_TO_CONNECT);
            sendState(improv::STATE_AUTHORIZED);
        }
    }
    if (stateAsked_ &&
        (board_.link() != Board::Link::Joining || elapsed(nowMs, stateAskMs_, kStateHoldMs))) {
        answerState();
    }
    if (scan_ == Scan::Waiting) {
        const int n = board_.scan();
        if (n >= 0) {
            scanCount_ = static_cast<int16_t>(n);
            scanNext_ = 0;
            scan_ = Scan::Sending;
        } else if (elapsed(nowMs, scanAskMs_, kScanLimitMs)) {
            scan_ = Scan::Idle;
            sendResult(improv::GET_WIFI_NETWORKS, nullptr, 0);
        }
    } else if (scan_ == Scan::Sending) {
        sendNextNetwork();
    }
}

void ImprovSerial::answerState() {
    stateAsked_ = false;
    if (joining_) {
        sendState(improv::STATE_PROVISIONING);
        return;
    }
    char url[40];
    if (board_.link() == Board::Link::Joined && board_.address(url, sizeof url)) {
        // Provisioned: the same answer as a join that just succeeded.
        sendState(improv::STATE_PROVISIONED);
        const char* result[] = {url};
        sendResult(improv::GET_CURRENT_STATE, result, 1);
        return;
    }
    sendState(improv::STATE_AUTHORIZED); // ready for a network; no button to press first
}

// One network per pass, so a long list does not hold the main loop on the serial port. Hidden
// networks are left out, and a network heard through several access points is listed once, with
// its strongest signal (the client lists every entry it gets).
void ImprovSerial::sendNextNetwork() {
    Board::Network net;
    Board::Network other;
    while (scanNext_ < scanCount_) {
        const int i = scanNext_++;
        if (!board_.network(i, net)) {
            break; // the results were dropped (another scan, a join)
        }
        bool listed = net.ssid[0] == '\0';
        for (int j = 0; j < scanCount_ && !listed; ++j) {
            if (j != i && board_.network(j, other) && strcmp(other.ssid, net.ssid) == 0 &&
                (other.rssi > net.rssi || (other.rssi == net.rssi && j < i))) {
                listed = true;
            }
        }
        if (listed) {
            continue;
        }
        char rssi[8];
        snprintf(rssi, sizeof rssi, "%d", net.rssi);
        const char* result[] = {net.ssid, rssi, net.secured ? PSTR("YES") : PSTR("NO")};
        sendResult(improv::GET_WIFI_NETWORKS, result, 3);
        return;
    }
    scan_ = Scan::Idle;
    sendResult(improv::GET_WIFI_NETWORKS, nullptr, 0); // an empty result ends the list
}

void ImprovSerial::sendState(uint8_t state) {
    sendPacket(improv::TYPE_CURRENT_STATE, &state, 1);
}

void ImprovSerial::sendError(uint8_t error) {
    sendPacket(improv::TYPE_ERROR_STATE, &error, 1);
}

void ImprovSerial::sendResult(uint8_t command, const char* const* strings, uint8_t count) {
    uint8_t buf[improv::RPC_RESPONSE_MAX_SIZE];
    improv::RpcResponseBuilder rpc(buf, static_cast<improv::Command>(command));
    for (uint8_t i = 0; i < count; ++i) {
        char text[65];
        strncpy_P(text, strings[i], sizeof text - 1); // may be flash text
        text[sizeof text - 1] = '\0';
        rpc.add_string(text);
    }
    const auto out = rpc.finish(false);
    // Without the builder's trailing byte: on serial the packet's own checksum covers the result.
    sendPacket(improv::TYPE_RPC_RESPONSE, out.data(), out.size() - 1);
}

void ImprovSerial::sendPacket(uint8_t type, const uint8_t* data, size_t len) {
    uint8_t head[kDataAt];
    for (uint8_t i = 0; i <= kHeaderLen; ++i) {
        head[i] = headerByte(i);
    }
    head[kHeaderLen + 1] = type;
    head[kHeaderLen + 2] = static_cast<uint8_t>(len);
    uint8_t sum = 0;
    for (uint8_t b : head) {
        sum = static_cast<uint8_t>(sum + b);
    }
    for (size_t i = 0; i < len; ++i) {
        sum = static_cast<uint8_t>(sum + data[i]);
    }
    // The newline is not part of the packet; it keeps whatever the board prints next on its own
    // line, where the client looks for packets.
    const uint8_t tail[] = {sum, '\n'};
    board_.send(head, sizeof head);
    board_.send(data, len);
    board_.send(tail, sizeof tail);
}

} // namespace gbsr::core
