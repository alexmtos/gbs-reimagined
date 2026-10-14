#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gbsr::platform {

// Small whole-file storage: LittleFS on the board (storage_esp.cpp), memory in host tests. Only
// the main loop calls it (ADR 0003). Writes are atomic: a power cut leaves the old file or the new
// one, never a mix.
class FileStore {
public:
    virtual ~FileStore() = default;
    // Reads up to `cap` bytes. Returns the number read, or -1 when the file does not exist.
    virtual int read(const char* path, uint8_t* buf, size_t cap) = 0;
    virtual bool write(const char* path, const uint8_t* data, size_t len) = 0;
    virtual bool remove(const char* path) = 0;
};

} // namespace gbsr::platform
