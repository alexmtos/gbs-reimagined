#include "core/update_check.h"

#include <string.h>

#include "platform/progmem.h"

namespace gbsr::core {

namespace {

constexpr uint8_t kFlagExtra = 0x04;
constexpr uint8_t kFlagName = 0x08;
constexpr uint8_t kFlagComment = 0x10;
constexpr size_t kFixedHeader = 10; // RFC 1952: ID1 ID2 CM FLG MTIME(4) XFL OS

bool plain(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
           ch == '.' || ch == '+' || ch == '-';
}

// Copies the value of `key=` (`key` in flash) from a space-separated comment: 1 to cap - 1
// characters of [0-9A-Za-z.+-], which keeps it safe to print and to put in JSON.
bool field(const char* comment, const char* key, char* out, size_t cap) {
    const size_t klen = strlen_P(key);
    for (const char* p = comment; *p != '\0';) {
        while (*p == ' ') {
            ++p;
        }
        const char* end = p;
        while (*end != '\0' && *end != ' ') {
            ++end;
        }
        if (static_cast<size_t>(end - p) > klen && strncmp_P(p, key, klen) == 0 && p[klen] == '=') {
            const char* value = p + klen + 1;
            const size_t n = static_cast<size_t>(end - value);
            if (n == 0 || n >= cap) {
                return false;
            }
            for (size_t i = 0; i < n; ++i) {
                if (!plain(value[i])) {
                    return false;
                }
            }
            memcpy(out, value, n);
            out[n] = '\0';
            return true;
        }
        p = end;
    }
    return false;
}

} // namespace

UpdateVerdict checkUpdateHead(const uint8_t* data, size_t len, const char* board,
                              UpdateHeader& out) {
    out = UpdateHeader{};
    if (len < 4) {
        return UpdateVerdict::Short;
    }
    if (data[0] != 0x1F || data[1] != 0x8B || data[2] != 8) {
        return UpdateVerdict::NotGzip;
    }
    const uint8_t flags = data[3];
    size_t at = kFixedHeader;
    if (len < at) {
        return UpdateVerdict::Short;
    }
    if (flags & kFlagExtra) {
        if (len < at + 2) {
            return UpdateVerdict::Short;
        }
        at += 2 + (data[at] | data[at + 1] << 8);
    }
    if (flags & kFlagName) {
        while (at < len && data[at] != 0) {
            ++at;
        }
        if (at >= len) {
            return UpdateVerdict::Short;
        }
        ++at;
    }
    if ((flags & kFlagComment) == 0) {
        return UpdateVerdict::NoBoardId;
    }
    char comment[96];
    size_t n = 0;
    while (at < len && data[at] != 0) {
        if (n < sizeof comment - 1) {
            comment[n++] = static_cast<char>(data[at]);
        }
        ++at;
    }
    if (at >= len) {
        return UpdateVerdict::Short;
    }
    comment[n] = '\0';
    // Not ours, or a board or version we would not trust to print.
    if (strncmp_P(comment, PSTR("gbsr "), 5) != 0 ||
        !field(comment, PSTR("board"), out.board, sizeof out.board) ||
        !field(comment, PSTR("version"), out.version, sizeof out.version)) {
        return UpdateVerdict::NoBoardId;
    }
    return strcmp(out.board, board) == 0 ? UpdateVerdict::Accept : UpdateVerdict::WrongBoard;
}

const char* updateReason(UpdateVerdict v) {
    switch (v) {
    case UpdateVerdict::Accept:
        return PSTR("");
    case UpdateVerdict::NotGzip:
        return PSTR("notGzip");
    case UpdateVerdict::NoBoardId:
        return PSTR("noBoardId");
    case UpdateVerdict::WrongBoard:
        return PSTR("wrongBoard");
    case UpdateVerdict::Short:
        break;
    }
    return PSTR("short");
}

} // namespace gbsr::core
