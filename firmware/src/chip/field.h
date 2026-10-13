#pragma once

#include <stdint.h>

namespace gbsr::tv5725 {

// A register field: `width` bits starting at bit `lsb` of byte `addr` in segment `seg`, spanning
// consecutive bytes little-endian. The template parameters make the vendor's access marking a
// compile-time property: a write to a read-only field does not compile.
template <bool Writable, bool Signed = false>
struct FieldT {
    uint8_t seg;
    uint8_t addr;
    uint8_t lsb;
    uint8_t width;

    static constexpr bool writable = Writable;
    static constexpr bool isSigned = Signed;

    constexpr uint8_t byteCount() const { return static_cast<uint8_t>((lsb + width + 7) / 8); }
    constexpr uint32_t mask() const { return width >= 32 ? 0xFFFFFFFFu : ((1u << width) - 1u); }
};

using RoField = FieldT<false>;
using RwField = FieldT<true>;
using SignedField = FieldT<true, true>;

// Runtime description of a field, for the name table used by the console and diagnostics.
struct FieldInfo {
    uint16_t nameOffset; // into kFieldNames
    uint8_t seg;
    uint8_t addr;
    uint8_t lsb;
    uint8_t width;
    uint8_t flags; // kFieldWritable | kFieldSigned
};

constexpr uint8_t kFieldWritable = 0x01;
constexpr uint8_t kFieldSigned = 0x02;

} // namespace gbsr::tv5725
