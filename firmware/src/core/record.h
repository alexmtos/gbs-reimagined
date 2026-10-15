#pragma once

#include <stddef.h>
#include <stdint.h>

#include "platform/storage.h"

namespace gbsr::core {

uint32_t crc32(const uint8_t* data, size_t len);

// A small persisted record: magic (4 bytes), version, payload length (2 bytes), CRC-32 of the
// payload (4 bytes), then the payload. A record that fails any check reads as absent, so a
// half-written or foreign file can never be mistaken for data.
constexpr size_t kRecordHeader = 11;
constexpr uint16_t kMaxRecordPayload = 512;

bool saveRecord(platform::FileStore& fs, const char* path, uint32_t magic, uint8_t version,
                const uint8_t* payload, uint16_t len);

// Returns the payload length, or -1 when the file is absent, foreign, of another version or
// damaged.
int loadRecord(platform::FileStore& fs, const char* path, uint32_t magic, uint8_t version,
               uint8_t* payload, uint16_t cap);

} // namespace gbsr::core
