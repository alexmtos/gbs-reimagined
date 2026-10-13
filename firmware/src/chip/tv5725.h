#pragma once

#include <stdint.h>

#include "chip/field.h"
#include "chip/programs.h"
#include "hal/i2c_bus.h"

namespace gbsr::tv5725 {

// Receives every byte written to the chip, after the write succeeded. The journal implements it.
class WriteObserver {
public:
    virtual ~WriteObserver() = default;
    virtual void onChipWrite(uint8_t seg, uint8_t addr, uint8_t value) = 0;
};

// Register access to the Tvia TV5725 over I2C.
//
// The chip exposes six 256-byte segments behind one address; writing register 0xF0 selects the
// segment and the register pointer auto-increments. It answers at most 16 bytes per read
// transaction, so longer reads are split.
//
// Threading rule: only the main loop calls into this class. Network callbacks never touch the
// bus; they read the status snapshot instead. That keeps the cached segment valid by
// construction.
//
// Errors are sticky: a failed transaction sets `ok()` false until `clearError()`, so a sequence
// of accesses can be checked once at the end.
class Chip {
public:
    static constexpr uint8_t kAddress = 0x17;
    static constexpr uint8_t kSegmentRegister = 0xF0;
    static constexpr uint8_t kMaxBurst = 16;
    static constexpr uint8_t kSegments = 6;

    explicit Chip(hal::I2cBus& bus) : bus_(bus) {}

    // True when the chip acknowledges and a scratch register reads back what was written.
    bool probe();

    bool readBytes(uint8_t seg, uint8_t addr, uint8_t* out, uint16_t count);
    bool writeBytes(uint8_t seg, uint8_t addr, const uint8_t* data, uint16_t count);
    bool readByte(uint8_t seg, uint8_t addr, uint8_t& out) { return readBytes(seg, addr, &out, 1); }
    bool writeByte(uint8_t seg, uint8_t addr, uint8_t value) {
        return writeBytes(seg, addr, &value, 1);
    }

    // Field access. A partial-byte write is a read-modify-write of the bytes the field occupies.
    template <bool W, bool S>
    bool read(FieldT<W, S> f, uint32_t& out) {
        return readField(f.seg, f.addr, f.lsb, f.width, S, out);
    }

    template <bool W, bool S>
    uint32_t get(FieldT<W, S> f) {
        uint32_t v = 0;
        read(f, v);
        return v;
    }

    template <bool S>
    bool write(FieldT<true, S> f, uint32_t value) {
        return writeField(f.seg, f.addr, f.lsb, f.width, value);
    }

    // Untyped access by locus, for the console and diagnostics (name lookups resolve to a locus).
    bool readField(uint8_t seg, uint8_t addr, uint8_t lsb, uint8_t width, bool isSigned,
                   uint32_t& out);
    bool writeField(uint8_t seg, uint8_t addr, uint8_t lsb, uint8_t width, uint32_t value);

    // Writes a baseline program's records as they are stored (see programs.h). The caller owns
    // the sequencing around it (resets, clocks); this only moves bytes.
    bool writeProgram(ProgramId id);

    // Forget the cached segment, e.g. after a bus error or when another master may have used it.
    void invalidateSegment() { segment_ = kUnknownSegment; }

    void setObserver(WriteObserver* observer) { observer_ = observer; }

    bool ok() const { return ok_; }
    void clearError() { ok_ = true; }
    uint32_t errorCount() const { return errors_; }
    uint32_t transactionCount() const { return transactions_; }

private:
    static constexpr int16_t kUnknownSegment = -1;

    bool selectSegment(uint8_t seg);
    bool fail();

    hal::I2cBus& bus_;
    WriteObserver* observer_ = nullptr;
    int16_t segment_ = kUnknownSegment;
    bool ok_ = true;
    uint32_t errors_ = 0;
    uint32_t transactions_ = 0;
};

} // namespace gbsr::tv5725
