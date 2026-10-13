#include "chip/field_table.h"

#include <ctype.h>

namespace gbsr::tv5725 {

namespace {

FieldInfo entry(uint16_t index) {
    FieldInfo info{};
    memcpy_P(&info, &kFieldTable[index], sizeof info);
    return info;
}

// Case-insensitive comparison of a RAM string against a name in the flash blob.
int compareName(const char* name, uint16_t nameOffset) {
    const char* p = kFieldNames + nameOffset;
    for (;; ++name, ++p) {
        const int a = toupper(static_cast<unsigned char>(*name));
        const int b = static_cast<unsigned char>(pgm_read_byte(p));
        if (a != b || a == 0) {
            return a - b;
        }
    }
}

} // namespace

bool findField(const char* name, FieldInfo& out) {
    int lo = 0;
    int hi = static_cast<int>(kFieldTableSize) - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const FieldInfo info = entry(static_cast<uint16_t>(mid));
        const int cmp = compareName(name, info.nameOffset);
        if (cmp == 0) {
            out = info;
            return true;
        }
        if (cmp < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return false;
}

void fieldName(uint16_t index, char* buf, uint8_t bufSize) {
    if (bufSize == 0) {
        return;
    }
    buf[0] = '\0';
    if (index >= kFieldTableSize) {
        return;
    }
    const FieldInfo info = entry(index);
    strncpy_P(buf, kFieldNames + info.nameOffset, bufSize - 1);
    buf[bufSize - 1] = '\0';
}

} // namespace gbsr::tv5725
