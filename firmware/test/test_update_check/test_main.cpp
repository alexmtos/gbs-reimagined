#include <string.h>
#include <string>
#include <vector>

#include "../support/doctest_main.h"
#include "core/update_check.h"

using namespace gbsr;
using core::UpdateHeader;
using core::UpdateVerdict;

namespace {

// A gzip header as tools/release/ota_image.py writes it, then some compressed bytes.
std::vector<uint8_t> header(uint8_t flags, const std::string& name, const std::string& comment) {
    std::vector<uint8_t> h = {0x1F, 0x8B, 8, flags, 0, 0, 0, 0, 2, 255};
    if (flags & 0x04) {
        h.insert(h.end(), {3, 0, 'a', 'b', 'c'});
    }
    if (flags & 0x08) {
        h.insert(h.end(), name.begin(), name.end());
        h.push_back(0);
    }
    if (flags & 0x10) {
        h.insert(h.end(), comment.begin(), comment.end());
        h.push_back(0);
    }
    h.insert(h.end(), {0xED, 0xBD, 0x07, 0x60});
    return h;
}

UpdateVerdict check(const std::vector<uint8_t>& data, UpdateHeader& out) {
    return core::checkUpdateHead(data.data(), data.size(), "gbsc", out);
}

} // namespace

TEST_CASE("a release update file for this board is accepted, with its version") {
    UpdateHeader h;
    const auto data =
        header(0x18, "gbs-reimagined-1.2.0-gbsc.bin", "gbsr board=gbsc version=1.2.0");
    CHECK(check(data, h) == UpdateVerdict::Accept);
    CHECK(std::string(h.board) == "gbsc");
    CHECK(std::string(h.version) == "1.2.0");
    // An extra field before the name is skipped too.
    CHECK(check(header(0x1C, "x.bin", "gbsr board=gbsc version=2.0.0-beta.1"), h) ==
          UpdateVerdict::Accept);
    CHECK(std::string(h.version) == "2.0.0-beta.1");
}

TEST_CASE("files for another board, or without our header, are refused before any write") {
    UpdateHeader h;
    CHECK(check(header(0x18, "x.bin", "gbsr board=other version=1.0"), h) ==
          UpdateVerdict::WrongBoard);
    CHECK(check(header(0x08, "firmware.bin", ""), h) == UpdateVerdict::NoBoardId); // plain gzip
    CHECK(check(header(0x10, "", "made by hand"), h) == UpdateVerdict::NoBoardId);
    CHECK(check(header(0x10, "", "gbsr version=1.0"), h) == UpdateVerdict::NoBoardId);
    const std::vector<uint8_t> raw = {0xE9, 0x03, 0x02, 0x40, 0x00, 0x10};
    CHECK(check(raw, h) == UpdateVerdict::NotGzip); // the full image, not the update file
    CHECK(check({0x1F, 0x8B}, h) == UpdateVerdict::Short);
    auto cut = header(0x18, "x.bin", "gbsr board=gbsc version=1.0");
    cut.resize(14); // the comment does not end in the first piece
    CHECK(check(cut, h) == UpdateVerdict::Short);
    CHECK(std::string(core::updateReason(UpdateVerdict::WrongBoard)) == "wrongBoard");
    CHECK(std::string(core::updateReason(UpdateVerdict::Accept)).empty());
}

TEST_CASE("a board or version that is missing, or not plain text, is not trusted") {
    UpdateHeader h;
    CHECK(check(header(0x10, "", "gbsr board=gbsc"), h) == UpdateVerdict::NoBoardId);
    CHECK(check(header(0x10, "", "gbsr board=gbsc version=1.0\"}"), h) == UpdateVerdict::NoBoardId);
    CHECK(check(header(0x10, "", "gbsr board=gb/sc version=1.0"), h) == UpdateVerdict::NoBoardId);
    const std::string longVersion(40, '9');
    CHECK(check(header(0x10, "", "gbsr board=gbsc version=" + longVersion), h) ==
          UpdateVerdict::NoBoardId);
    CHECK(check({}, h) == UpdateVerdict::Short); // an empty file
    // A long comment is read up to its first 95 characters; ours fit well within that.
    CHECK(check(header(0x10, "", "gbsr board=gbsc version=1.0 " + std::string(200, 'x')), h) ==
          UpdateVerdict::Accept);
}
