#pragma once

#include <stddef.h>
#include <stdint.h>

#include "qrcodegen.h"

namespace gbsr::ui {

// QR codes for the board's screen (ADR 0017): at most version 3, 29×29 modules, so a code fits the
// 64-pixel-high display at two pixels per module.
constexpr int kQrMaxVersion = 3;
constexpr size_t kQrBufferLen = qrcodegen_BUFFER_LEN_FOR_VERSION(kQrMaxVersion);

// Encodes `text` with the strongest error correction that still fits; false when the text does
// not fit version 3. Read the result with qrcodegen_getSize() and qrcodegen_getModule().
bool qrEncode(const char* text, uint8_t (&code)[kQrBufferLen]);

// "WIFI:T:WPA;S:<ssid>;P:<password>;;", the text phone cameras offer to join a network from, with
// \ ; , : and " escaped. False when it does not fit `cap`.
bool wifiQrText(const char* ssid, const char* password, char* out, size_t cap);

// "HTTP://a.b.c.d/" for an IPv4 address in the ESP's byte order (first octet in the low byte).
void urlQrText(uint32_t ip, char* out, size_t cap);

} // namespace gbsr::ui
