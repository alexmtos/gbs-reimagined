#if defined(ARDUINO)

#include "platform/storage_esp.h"

#include <LittleFS.h>

namespace gbsr::platform {

bool EspFileStore::begin() {
    mounted_ = LittleFS.begin(); // formats an unreadable partition on its own
    return mounted_;
}

int EspFileStore::read(const char* path, uint8_t* buf, size_t cap) {
    if (!mounted_ || !LittleFS.exists(path)) {
        return -1;
    }
    File f = LittleFS.open(path, "r");
    if (!f) {
        return -1;
    }
    const size_t n = f.read(buf, cap);
    f.close();
    return static_cast<int>(n);
}

bool EspFileStore::write(const char* path, const uint8_t* data, size_t len) {
    if (!mounted_) {
        return false;
    }
    char tmp[40];
    snprintf(tmp, sizeof tmp, "%s.new", path);
    File f = LittleFS.open(tmp, "w");
    if (!f) {
        return false;
    }
    const bool ok = f.write(data, len) == len;
    f.close();
    // LittleFS renames atomically: the old file stays valid until the new one is complete.
    return ok && LittleFS.rename(tmp, path);
}

bool EspFileStore::remove(const char* path) {
    return mounted_ && (!LittleFS.exists(path) || LittleFS.remove(path));
}

} // namespace gbsr::platform

#endif
