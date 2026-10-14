#pragma once

#include "platform/storage.h"

namespace gbsr::platform {

// LittleFS on the board's 1 MB filesystem partition.
class EspFileStore : public FileStore {
public:
    bool begin();
    int read(const char* path, uint8_t* buf, size_t cap) override;
    bool write(const char* path, const uint8_t* data, size_t len) override;
    bool remove(const char* path) override;

private:
    bool mounted_ = false;
};

} // namespace gbsr::platform
