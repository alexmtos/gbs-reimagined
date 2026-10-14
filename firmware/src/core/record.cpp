#include "core/record.h"

#include <string.h>

namespace gbsr::core {

namespace {

constexpr uint16_t kMaxPayload = 512;

void put32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        p[i] = static_cast<uint8_t>(v >> (8 * i));
    }
}

uint32_t get32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
           static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

} // namespace

uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

bool saveRecord(platform::FileStore& fs, const char* path, uint32_t magic, uint8_t version,
                const uint8_t* payload, uint16_t len) {
    if (len > kMaxPayload) {
        return false;
    }
    uint8_t buf[kRecordHeader + kMaxPayload];
    put32(buf, magic);
    buf[4] = version;
    buf[5] = static_cast<uint8_t>(len);
    buf[6] = static_cast<uint8_t>(len >> 8);
    put32(buf + 7, crc32(payload, len));
    memcpy(buf + kRecordHeader, payload, len);
    return fs.write(path, buf, kRecordHeader + len);
}

int loadRecord(platform::FileStore& fs, const char* path, uint32_t magic, uint8_t version,
               uint8_t* payload, uint16_t cap) {
    uint8_t buf[kRecordHeader + kMaxPayload];
    const int n = fs.read(path, buf, sizeof buf);
    if (n < static_cast<int>(kRecordHeader) || get32(buf) != magic || buf[4] != version) {
        return -1;
    }
    const uint16_t len = static_cast<uint16_t>(buf[5] | buf[6] << 8);
    if (len > cap || n != static_cast<int>(kRecordHeader + len) ||
        crc32(buf + kRecordHeader, len) != get32(buf + 7)) {
        return -1;
    }
    memcpy(payload, buf + kRecordHeader, len);
    return len;
}

} // namespace gbsr::core
