#include "chip/tv5725.h"

#include "chip/fields.gen.h"

namespace gbsr::tv5725 {

namespace {

// ADC_UNUSED_69 (S5_69) is documented as unused and is writable: a safe scratch byte for the
// presence test. The pattern is arbitrary.
constexpr uint8_t kScratchSeg = 5;
constexpr uint8_t kScratchAddr = 0x69;
constexpr uint8_t kScratchPattern = 0x6A;

} // namespace

bool Chip::fail() {
    ok_ = false;
    ++errors_;
    invalidateSegment();
    return false;
}

bool Chip::selectSegment(uint8_t seg) {
    if (seg >= kSegments) {
        return fail();
    }
    if (segment_ == seg) {
        return true;
    }
    ++transactions_;
    if (!bus_.writeReg(kAddress, kSegmentRegister, seg)) {
        return fail();
    }
    segment_ = seg;
    return true;
}

bool Chip::probe() {
    if (!bus_.probe(kAddress)) {
        return fail();
    }
    invalidateSegment();
    uint8_t original = 0;
    uint8_t readBack = 0;
    if (!readByte(kScratchSeg, kScratchAddr, original) ||
        !writeByte(kScratchSeg, kScratchAddr, kScratchPattern) ||
        !readByte(kScratchSeg, kScratchAddr, readBack)) {
        return false;
    }
    writeByte(kScratchSeg, kScratchAddr, original);
    return readBack == kScratchPattern;
}

bool Chip::readBytes(uint8_t seg, uint8_t addr, uint8_t* out, uint16_t count) {
    if (count == 0) {
        return true;
    }
    if (addr + count > 0x100 || !selectSegment(seg)) {
        return fail();
    }
    while (count > 0) {
        const uint8_t chunk = count > kMaxBurst ? kMaxBurst : static_cast<uint8_t>(count);
        ++transactions_;
        if (!bus_.writeRead(kAddress, &addr, 1, out, chunk)) {
            return fail();
        }
        addr = static_cast<uint8_t>(addr + chunk);
        out += chunk;
        count = static_cast<uint16_t>(count - chunk);
    }
    return true;
}

bool Chip::writeBytes(uint8_t seg, uint8_t addr, const uint8_t* data, uint16_t count) {
    if (count == 0) {
        return true;
    }
    if (addr + count > 0x100 || !selectSegment(seg)) {
        return fail();
    }
    uint8_t buf[kMaxBurst + 1];
    while (count > 0) {
        const uint8_t chunk = count > kMaxBurst ? kMaxBurst : static_cast<uint8_t>(count);
        buf[0] = addr;
        for (uint8_t i = 0; i < chunk; ++i) {
            buf[i + 1] = data[i];
        }
        ++transactions_;
        if (!bus_.write(kAddress, buf, chunk + 1u)) {
            return fail();
        }
        if (addr == kSegmentRegister ||
            (addr < kSegmentRegister && addr + chunk > kSegmentRegister)) {
            invalidateSegment(); // a raw write covered the segment register itself
        }
        if (observer_ != nullptr) {
            for (uint8_t i = 0; i < chunk; ++i) {
                observer_->onChipWrite(seg, static_cast<uint8_t>(addr + i), data[i]);
            }
        }
        addr = static_cast<uint8_t>(addr + chunk);
        data += chunk;
        count = static_cast<uint16_t>(count - chunk);
    }
    return true;
}

bool Chip::readField(uint8_t seg, uint8_t addr, uint8_t lsb, uint8_t width, bool isSigned,
                     uint32_t& out) {
    const uint8_t nbytes = static_cast<uint8_t>((lsb + width + 7) / 8);
    if (width == 0 || width > 32 || nbytes > 5) {
        return fail();
    }
    uint8_t raw[5] = {0, 0, 0, 0, 0};
    if (!readBytes(seg, addr, raw, nbytes)) {
        return false;
    }
    uint64_t word = 0;
    for (uint8_t i = 0; i < nbytes; ++i) {
        word |= static_cast<uint64_t>(raw[i]) << (8 * i);
    }
    const uint64_t mask = (width >= 32) ? 0xFFFFFFFFull : ((1ull << width) - 1);
    uint32_t value = static_cast<uint32_t>((word >> lsb) & mask);
    if (isSigned && width < 32 && (value & (1u << (width - 1)))) {
        value |= ~static_cast<uint32_t>(mask); // sign-extend
    }
    out = value;
    return true;
}

bool Chip::writeField(uint8_t seg, uint8_t addr, uint8_t lsb, uint8_t width, uint32_t value) {
    const uint8_t nbytes = static_cast<uint8_t>((lsb + width + 7) / 8);
    if (width == 0 || width > 32 || nbytes > 5) {
        return fail();
    }
    const uint64_t mask = ((width >= 32) ? 0xFFFFFFFFull : ((1ull << width) - 1)) << lsb;
    uint8_t raw[5] = {0, 0, 0, 0, 0};
    const bool wholeBytes = lsb == 0 && (width % 8) == 0;
    if (!wholeBytes && !readBytes(seg, addr, raw, nbytes)) {
        return false;
    }
    uint64_t word = 0;
    for (uint8_t i = 0; i < nbytes; ++i) {
        word |= static_cast<uint64_t>(raw[i]) << (8 * i);
    }
    word = (word & ~mask) | ((static_cast<uint64_t>(value) << lsb) & mask);
    for (uint8_t i = 0; i < nbytes; ++i) {
        raw[i] = static_cast<uint8_t>(word >> (8 * i));
    }
    return writeBytes(seg, addr, raw, nbytes);
}

bool Chip::writeProgram(ProgramId id) {
    const ProgramInfo info = programInfo(id);
    const uint8_t* p = info.records;
    const uint8_t* end = info.records + info.recordsSize;
    uint8_t values[kMaxBurst];
    while (p < end) {
        const uint8_t seg = pgm_read_byte(p++);
        const uint8_t start = pgm_read_byte(p++);
        const uint8_t count = pgm_read_byte(p++);
        if (count > kMaxBurst) {
            return fail();
        }
        memcpy_P(values, p, count);
        p += count;
        if (!writeBytes(seg, start, values, count)) {
            return false;
        }
    }
    return true;
}

ProgramInfo programInfo(ProgramId id) {
    ProgramInfo info{};
    const uint8_t index = static_cast<uint8_t>(id);
    if (index < static_cast<uint8_t>(ProgramId::Count)) {
        memcpy_P(&info, &kPrograms[index], sizeof info);
    }
    return info;
}

} // namespace gbsr::tv5725
