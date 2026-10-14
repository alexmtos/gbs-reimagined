#pragma once

#include <map>
#include <string>
#include <vector>

#include "platform/storage.h"

namespace gbsr::test {

// Files in memory, with a write counter and an injectable failure.
class FakeFileStore : public platform::FileStore {
public:
    std::map<std::string, std::vector<uint8_t>> files;
    int writes = 0;
    bool failWrites = false;

    int read(const char* path, uint8_t* buf, size_t cap) override {
        auto it = files.find(path);
        if (it == files.end()) {
            return -1;
        }
        const size_t n = it->second.size() < cap ? it->second.size() : cap;
        memcpy(buf, it->second.data(), n);
        return static_cast<int>(n);
    }

    bool write(const char* path, const uint8_t* data, size_t len) override {
        ++writes;
        if (failWrites) {
            return false;
        }
        files[path].assign(data, data + len);
        return true;
    }

    bool remove(const char* path) override {
        files.erase(path);
        return true;
    }
};

} // namespace gbsr::test
