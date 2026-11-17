#include <string.h>
#include <string>

#include "../support/doctest_main.h"
#include "ui/qr.h"

using namespace gbsr;

TEST_CASE("the Wi-Fi text is the WIFI: form phone cameras read, special characters escaped") {
    char out[64];
    CHECK(ui::wifiQrText("GBS-EAC4", "qqqqqqqq", out, sizeof out));
    CHECK(std::string(out) == "WIFI:T:WPA;S:GBS-EAC4;P:qqqqqqqq;;");
    CHECK(ui::wifiQrText("a;b,c", "p:\"q\\", out, sizeof out));
    CHECK(std::string(out) == "WIFI:T:WPA;S:a\\;b\\,c;P:p\\:\\\"q\\\\;;");
}

TEST_CASE("a Wi-Fi text that does not fit is refused, not cut") {
    char out[24];
    CHECK_FALSE(ui::wifiQrText("GBS-EAC4", "qqqqqqqq", out, sizeof out));
    CHECK(out[0] == '\0');
}

TEST_CASE("the address text opens the board's page") {
    char out[32];
    ui::urlQrText(0x7701000A, out, sizeof out); // 10.0.1.119, first octet in the low byte
    CHECK(std::string(out) == "HTTP://10.0.1.119/");
}

TEST_CASE("the board's texts fit version 3, the largest the screen shows at two pixels") {
    uint8_t code[ui::kQrBufferLen];
    char text[48];
    CHECK(ui::wifiQrText("GBS-EAC4", "qqqqqqqq", text, sizeof text));
    REQUIRE(ui::qrEncode(text, code));
    CHECK(qrcodegen_getSize(code) <= 29);
    ui::urlQrText(0xFFFFFFFF, text, sizeof text); // the longest address
    REQUIRE(ui::qrEncode(text, code));
    CHECK(qrcodegen_getSize(code) == 21); // version 1: three pixels per module
    // Finder pattern in the top-left corner: a dark ring around a light ring around a dark core.
    CHECK(qrcodegen_getModule(code, 0, 0));
    CHECK(qrcodegen_getModule(code, 6, 6));
    CHECK_FALSE(qrcodegen_getModule(code, 1, 1));
    CHECK(qrcodegen_getModule(code, 3, 3));
}

TEST_CASE("a text too long for version 3 is refused") {
    uint8_t code[ui::kQrBufferLen];
    char text[80];
    memset(text, 'x', sizeof text - 1);
    text[sizeof text - 1] = '\0';
    CHECK_FALSE(ui::qrEncode(text, code));
}
