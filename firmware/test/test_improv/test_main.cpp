#include <string.h>
#include <string>
#include <vector>

#include "../support/doctest_main.h"
#include "core/improv_serial.h"

using namespace gbsr;
using core::ImprovSerial;
using Link = ImprovSerial::Board::Link;

namespace {

constexpr uint8_t kState = 0x01;
constexpr uint8_t kError = 0x02;
constexpr uint8_t kRpc = 0x03;
constexpr uint8_t kResult = 0x04;

constexpr uint8_t kSendWifi = 0x01;
constexpr uint8_t kGetState = 0x02;
constexpr uint8_t kGetInfo = 0x03;
constexpr uint8_t kGetNetworks = 0x04;

constexpr uint8_t kReady = 0x02;
constexpr uint8_t kProvisioning = 0x03;
constexpr uint8_t kProvisioned = 0x04;

class FakeBoard final : public ImprovSerial::Board {
public:
    void send(const uint8_t* data, size_t len) override { out.insert(out.end(), data, data + len); }
    bool join(const char* s, const char* p) override {
        ssid = s;
        password = p;
        ++joins;
        if (*s == '\0') {
            return false;
        }
        state = Link::Joining;
        return true;
    }
    Link link() const override { return state; }
    int scan() override { return scanDone ? static_cast<int>(networks.size()) : -1; }
    bool network(int i, Network& n) const override {
        if (i < 0 || i >= static_cast<int>(networks.size())) {
            return false;
        }
        n = networks[static_cast<size_t>(i)];
        return true;
    }
    bool address(char* url, size_t cap) const override {
        if (state != Link::Joined) {
            return false;
        }
        snprintf(url, cap, "http://10.0.0.119/");
        return true;
    }

    std::vector<uint8_t> out;
    Link state = Link::Failed;
    std::string ssid;
    std::string password;
    int joins = 0;
    bool scanDone = false;
    std::vector<Network> networks;
};

struct Packet {
    uint8_t type;
    std::vector<uint8_t> data;
};

std::vector<uint8_t> frame(uint8_t type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> p = {
        'I', 'M', 'P', 'R', 'O', 'V', 1, type, static_cast<uint8_t>(data.size())};
    p.insert(p.end(), data.begin(), data.end());
    uint8_t sum = 0;
    for (uint8_t b : p) {
        sum = static_cast<uint8_t>(sum + b);
    }
    p.push_back(sum);
    p.push_back('\n'); // as the installer's client sends it
    return p;
}

std::vector<uint8_t> rpc(uint8_t command, const std::vector<uint8_t>& args = {}) {
    std::vector<uint8_t> d = {command, static_cast<uint8_t>(args.size())};
    d.insert(d.end(), args.begin(), args.end());
    return frame(kRpc, d);
}

std::vector<uint8_t> wifiSettings(const std::string& ssid, const std::string& password) {
    std::vector<uint8_t> a = {static_cast<uint8_t>(ssid.size())};
    a.insert(a.end(), ssid.begin(), ssid.end());
    a.push_back(static_cast<uint8_t>(password.size()));
    a.insert(a.end(), password.begin(), password.end());
    return rpc(kSendWifi, a);
}

// Feeds bytes; returns what reached the console.
std::string feed(ImprovSerial& improv, const std::vector<uint8_t>& bytes, uint32_t nowMs = 1000) {
    std::string console;
    for (uint8_t b : bytes) {
        const auto spill = improv.feed(b, nowMs);
        console.append(reinterpret_cast<const char*>(spill.data), spill.len);
    }
    return console;
}

std::string feed(ImprovSerial& improv, const std::string& text, uint32_t nowMs = 1000) {
    return feed(improv, std::vector<uint8_t>(text.begin(), text.end()), nowMs);
}

// Splits what the board sent into packets, checking each one's framing as the client does: the
// header at the start of a line, the checksum over every byte before it, a newline after it.
std::vector<Packet> packets(FakeBoard& board) {
    std::vector<Packet> result;
    const auto& o = board.out;
    size_t at = 0;
    while (at < o.size()) {
        REQUIRE(o.size() - at >= 11);
        REQUIRE(memcmp(&o[at], "IMPROV\x01", 7) == 0);
        const uint8_t len = o[at + 8];
        REQUIRE(at + 9 + len + 2 <= o.size());
        uint8_t sum = 0;
        for (size_t i = at; i < at + 9 + len; ++i) {
            sum = static_cast<uint8_t>(sum + o[i]);
        }
        CHECK(o[at + 9 + len] == sum);
        CHECK(o[at + 9 + len + 1] == '\n');
        result.push_back({o[at + 7], std::vector<uint8_t>(&o[at + 9], &o[at + 9 + len])});
        at += 9 + len + 2;
    }
    board.out.clear();
    return result;
}

// The strings of an RPC result, after checking which command it answers.
std::vector<std::string> strings(const Packet& p, uint8_t command) {
    REQUIRE(p.type == kResult);
    REQUIRE(p.data.size() >= 2);
    CHECK(p.data[0] == command);
    CHECK(p.data[1] == p.data.size() - 2);
    std::vector<std::string> s;
    for (size_t at = 2; at < p.data.size(); at += 1 + p.data[at]) {
        s.emplace_back(reinterpret_cast<const char*>(&p.data[at + 1]), p.data[at]);
    }
    return s;
}

ImprovSerial::Board::Network net(const char* ssid, int8_t rssi, bool secured) {
    ImprovSerial::Board::Network n{};
    snprintf(n.ssid, sizeof n.ssid, "%s", ssid);
    n.rssi = rssi;
    n.secured = secured;
    return n;
}

const ImprovSerial::Identity kId{"GBS Reimagined", "1.0.0", "ESP8266", "GBS-A1B2"};

} // namespace

TEST_CASE("console text passes through untouched") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    CHECK(feed(improv, "status\r\nset picture.sharpness medium\n") ==
          "status\r\nset picture.sharpness medium\n");
    CHECK(board.out.empty());
}

TEST_CASE("text that starts like a packet goes back to the console, in order") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    CHECK(feed(improv, "IMPx\n") == "IMPx\n");
    CHECK(feed(improv, "IIMPROW") == "IIMPROW");
    CHECK(feed(improv, "INPUT rgb\n") == "INPUT rgb\n");
    CHECK(board.out.empty());
}

TEST_CASE("a header start left waiting goes to the console when the next byte comes late") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    CHECK(feed(improv, "IM", 1000).empty());
    CHECK(feed(improv, "x", 3000) == "IMx");
}

TEST_CASE("the current state: ready without a network, provisioned with the page to open") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    CHECK(feed(improv, rpc(kGetState)) == "\n"); // the client's newline reaches the console
    auto p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(p[0].type == kError); // "no error" first, as the protocol asks
    CHECK(p[0].data == std::vector<uint8_t>{0});
    CHECK(p[1].type == kState);
    CHECK(p[1].data == std::vector<uint8_t>{kReady});

    board.state = Link::Joined;
    feed(improv, rpc(kGetState));
    p = packets(board);
    REQUIRE(p.size() == 3);
    CHECK(p[1].data == std::vector<uint8_t>{kProvisioned});
    CHECK(strings(p[2], kGetState) == std::vector<std::string>{"http://10.0.0.119/"});
}

TEST_CASE("the state asked while the saved network is being joined waits for the outcome") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    board.state = Link::Joining;
    feed(improv, rpc(kGetState), 1000);
    feed(improv, rpc(kGetState), 2000); // the client asks again every second
    improv.tick(3000);
    CHECK(packets(board).size() == 2); // the two "no error" packets only

    SUBCASE("joined: provisioned") {
        board.state = Link::Joined;
        improv.tick(5000);
        const auto p = packets(board);
        REQUIRE(p.size() == 2);
        CHECK(p[0].data == std::vector<uint8_t>{kProvisioned});
        CHECK(strings(p[1], kGetState) == std::vector<std::string>{"http://10.0.0.119/"});
        improv.tick(20000);
        CHECK(board.out.empty()); // answered once
    }
    SUBCASE("still joining after 8 s: ready") {
        improv.tick(8999);
        CHECK(board.out.empty());
        improv.tick(9001);
        const auto p = packets(board);
        REQUIRE(p.size() == 1);
        CHECK(p[0].data == std::vector<uint8_t>{kReady});
    }
}

TEST_CASE("the device information, in the order the installer reads it") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    feed(improv, rpc(kGetInfo));
    const auto p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(strings(p[1], kGetInfo) ==
          std::vector<std::string>{"GBS Reimagined", "1.0.0", "ESP8266", "GBS-A1B2"});
}

TEST_CASE("Wi-Fi settings: provisioning, then provisioned with the page to open") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    feed(improv, wifiSettings("Home network", "a password with spaces"), 1000);
    CHECK(board.joins == 1);
    CHECK(board.ssid == "Home network");
    CHECK(board.password == "a password with spaces");
    auto p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(p[1].type == kState);
    CHECK(p[1].data == std::vector<uint8_t>{kProvisioning});

    improv.tick(2000);
    CHECK(board.out.empty());
    feed(improv, rpc(kGetState), 3000);
    p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(p[1].data == std::vector<uint8_t>{kProvisioning});

    board.state = Link::Joined;
    improv.tick(6000);
    p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(p[0].data == std::vector<uint8_t>{kProvisioned});
    CHECK(strings(p[1], kSendWifi) == std::vector<std::string>{"http://10.0.0.119/"});
}

TEST_CASE("Wi-Fi settings that fail: unable to connect, then ready again") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    feed(improv, wifiSettings("Home", "wrong"), 1000);
    packets(board);

    SUBCASE("the board gives up") {
        board.state = Link::Failed;
        improv.tick(9000);
    }
    SUBCASE("the board still tries after 35 s") {
        improv.tick(35999);
        CHECK(board.out.empty());
        improv.tick(36001);
    }
    const auto p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(p[0].type == kError);
    CHECK(p[0].data == std::vector<uint8_t>{0x03});
    CHECK(p[1].data == std::vector<uint8_t>{kReady});
}

TEST_CASE("Wi-Fi settings the board cannot use are refused at once") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    feed(improv, wifiSettings("", "x"));
    const auto p = packets(board);
    REQUIRE(p.size() == 3);
    CHECK(p[1].data == std::vector<uint8_t>{0x03});
    CHECK(p[2].data == std::vector<uint8_t>{kReady});
    CHECK_FALSE(improv.provisioning());
}

TEST_CASE("networks: one result each, strongest access point only, hidden ones left out") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    board.networks = {net("Home", -70, true), net("", -40, true), net("Cafe", -60, false),
                      net("Home", -50, true), net("Home", -50, true)};
    feed(improv, rpc(kGetNetworks));
    packets(board);
    improv.tick(1000);
    CHECK(board.out.empty()); // still scanning
    board.scanDone = true;
    std::vector<std::vector<std::string>> heard;
    for (int pass = 0; pass < 10; ++pass) {
        improv.tick(2000);
        for (const auto& p : packets(board)) {
            heard.push_back(strings(p, kGetNetworks));
        }
    }
    CHECK(heard ==
          std::vector<std::vector<std::string>>{{"Cafe", "-60", "NO"}, {"Home", "-50", "YES"}, {}});
}

TEST_CASE("a new command stops a list the client no longer waits for") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    board.networks = {net("A", -50, true), net("B", -60, true)};
    board.scanDone = true;
    feed(improv, rpc(kGetNetworks));
    improv.tick(1000); // scan done
    improv.tick(1000); // A
    packets(board);
    feed(improv, rpc(kGetState));
    improv.tick(1000);
    improv.tick(1000);
    const auto p = packets(board);
    REQUIRE(p.size() == 2); // no error, ready: no more networks
    CHECK(p[1].type == kState);
}

TEST_CASE("broken packets: a bad checksum and an unknown command are reported") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    auto bad = rpc(kGetState);
    bad[bad.size() - 2] ^= 0xFF;
    CHECK(feed(improv, bad) == "\n");
    auto p = packets(board);
    REQUIRE(p.size() == 1);
    CHECK(p[0].type == kError);
    CHECK(p[0].data == std::vector<uint8_t>{0x01});

    feed(improv, rpc(0x05)); // hostname: not offered
    p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(p[1].data == std::vector<uint8_t>{0x02});

    feed(improv, frame(kRpc, {kGetState, 3})); // a length that disagrees
    p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(p[1].data == std::vector<uint8_t>{0x01});
}

TEST_CASE("a packet longer than this board accepts is read to its end, not left to the console") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    std::vector<uint8_t> big(200, '\n');
    big[0] = kSendWifi;
    big[1] = 198;
    CHECK(feed(improv, frame(kRpc, big)) == "\n");
    const auto p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(p[1].data == std::vector<uint8_t>{0x01});
    CHECK(board.joins == 0);
    CHECK(feed(improv, "status\n") == "status\n");
}

TEST_CASE("binary bytes inside a packet never reach the console, even a newline") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    CHECK(feed(improv, wifiSettings("A\nB", "line\r\nbreak")) == "\n");
    CHECK(board.ssid == "A\nB");
    CHECK(board.password == "line\r\nbreak");
}

TEST_CASE("the state hold also works when the answer is due in the same millisecond") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    board.state = Link::Joining;
    for (const uint32_t at : {1000u, 1001u}) {
        feed(improv, rpc(kGetState), at);
        improv.tick(at);
        CHECK(packets(board).size() == 1); // "no error" only: the answer waits
    }
    improv.tick(9001);
    CHECK(packets(board).size() == 1); // ready, once
}

TEST_CASE("a packet cut short in its body is dropped; what follows is read normally") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    auto cut = rpc(kGetInfo);
    cut.resize(10);
    CHECK(feed(improv, cut, 1000).empty());
    SUBCASE("console text") {
        CHECK(feed(improv, "status\n", 1600) == "status\n");
    }
    SUBCASE("the client's retry a second later") {
        feed(improv, rpc(kGetInfo), 2000);
        const auto p = packets(board);
        REQUIRE(p.size() == 2);
        CHECK(strings(p[1], kGetInfo).size() == 4);
    }
    CHECK(board.out.empty());
}

TEST_CASE("the longest Wi-Fi settings fit; one byte more is refused") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    const std::string ssid(32, 's');
    const std::string password(64, 'p');
    feed(improv, wifiSettings(ssid, password)); // data: 2 + 1 + 32 + 1 + 64 = 100 bytes
    CHECK(board.joins == 1);
    CHECK(board.ssid == ssid);
    CHECK(board.password == password);
    packets(board);

    std::vector<uint8_t> args = {32};
    args.insert(args.end(), ssid.begin(), ssid.end());
    args.push_back(65);
    args.insert(args.end(), 65, 'p');
    feed(improv, rpc(kSendWifi, args)); // 101 bytes
    const auto p = packets(board);
    REQUIRE(p.size() == 2);
    CHECK(p[1].data == std::vector<uint8_t>{0x01});
    CHECK(board.joins == 1);
}

TEST_CASE("Wi-Fi settings whose lengths do not add up are an invalid packet") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    for (const auto& args :
         std::vector<std::vector<uint8_t>>{{}, {10, 'a', 'b'}, {1, 'a', 9, 'b'}}) {
        feed(improv, rpc(kSendWifi, args));
        const auto p = packets(board);
        REQUIRE(p.size() == 2);
        CHECK(p[1].type == kError);
        CHECK(p[1].data == std::vector<uint8_t>{0x01});
    }
    CHECK(board.joins == 0);
}

TEST_CASE("another protocol version is not Improv to this board: the console gets it") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    auto other = rpc(kGetState);
    other[6] = 2;
    CHECK(feed(improv, other) == std::string(other.begin(), other.end()));
    CHECK(board.out.empty());
}

TEST_CASE("a scan that never ends gives an empty list after 10 s") {
    FakeBoard board;
    ImprovSerial improv(board, kId);
    feed(improv, rpc(kGetNetworks), 1000);
    packets(board);
    improv.tick(10999);
    CHECK(board.out.empty());
    improv.tick(11000);
    const auto p = packets(board);
    REQUIRE(p.size() == 1);
    CHECK(strings(p[0], kGetNetworks).empty());
    improv.tick(12000);
    CHECK(board.out.empty());
}
