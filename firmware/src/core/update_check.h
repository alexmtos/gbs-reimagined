#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gbsr::core {

// The firmware update file (ADR 0012): a gzip image whose header comment reads
// "gbsr board=<id> version=<version>", written by tools/release/ota_image.py. The core's updater
// and bootloader take the gzip as it is (the bootloader skips the comment); the comment lets the
// board refuse a file meant for another board before anything reaches the flash.
struct UpdateHeader {
    char board[16] = {};
    char version[32] = {};
};

enum class UpdateVerdict : uint8_t {
    Accept,
    NotGzip,    // not a gzip file: a raw .bin, or something else
    NoBoardId,  // a gzip without our comment, or with a board or version we cannot use
    WrongBoard, // made for another board
    Short,      // the header did not fit in the first piece of the upload
};

// Checks the first bytes of an upload against `board` (hal::BoardProfile::id).
UpdateVerdict checkUpdateHead(const uint8_t* data, size_t len, const char* board,
                              UpdateHeader& out);

// The reason a verdict refuses, as the API spells it ("notGzip", ...); "" for Accept. Flash text.
const char* updateReason(UpdateVerdict v);

} // namespace gbsr::core
