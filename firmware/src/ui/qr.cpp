#include "ui/qr.h"

#include <stdio.h>
#include <string.h>

namespace gbsr::ui {

bool qrEncode(const char* text, uint8_t (&code)[kQrBufferLen]) {
    uint8_t temp[kQrBufferLen];
    return qrcodegen_encodeText(text, temp, code, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
                                kQrMaxVersion, qrcodegen_Mask_AUTO, true);
}

namespace {

// Appends `value` with the WIFI: format's special characters escaped; false when out of room.
bool appendEscaped(char* out, size_t cap, size_t& len, const char* value) {
    for (const char* p = value; *p != '\0'; ++p) {
        const bool special = strchr("\\;,:\"", *p) != nullptr;
        if (len + (special ? 2 : 1) >= cap) {
            return false;
        }
        if (special) {
            out[len++] = '\\';
        }
        out[len++] = *p;
    }
    out[len] = '\0';
    return true;
}

bool appendText(char* out, size_t cap, size_t& len, const char* text) {
    const size_t n = strlen(text);
    if (len + n >= cap) {
        return false;
    }
    memcpy(out + len, text, n + 1);
    len += n;
    return true;
}

} // namespace

bool wifiQrText(const char* ssid, const char* password, char* out, size_t cap) {
    if (cap == 0) {
        return false;
    }
    size_t len = 0;
    out[0] = '\0';
    const bool ok = appendText(out, cap, len, "WIFI:T:WPA;S:") &&
                    appendEscaped(out, cap, len, ssid) && appendText(out, cap, len, ";P:") &&
                    appendEscaped(out, cap, len, password) && appendText(out, cap, len, ";;");
    if (!ok) {
        out[0] = '\0';
    }
    return ok;
}

void urlQrText(uint32_t ip, char* out, size_t cap) {
    // Upper case keeps the text in the QR alphanumeric set, so any address fits version 1 (21
    // modules), drawn at three pixels per module; the scheme is case-insensitive (RFC 3986).
    snprintf(out, cap, "HTTP://%u.%u.%u.%u/", static_cast<unsigned>(ip & 0xFF),
             static_cast<unsigned>(ip >> 8 & 0xFF), static_cast<unsigned>(ip >> 16 & 0xFF),
             static_cast<unsigned>(ip >> 24));
}

} // namespace gbsr::ui
