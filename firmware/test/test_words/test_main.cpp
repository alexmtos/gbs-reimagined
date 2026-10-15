#include <string>
#include <vector>

#include "../support/doctest_main.h"
#include "core/words.h"

namespace {

// Every word of `line`, as the console splits it.
std::vector<std::string> words(std::string line) {
    std::vector<char> buf(line.begin(), line.end());
    buf.push_back('\0');
    char* s = buf.data();
    std::vector<std::string> out;
    while (*s != '\0') {
        out.emplace_back(gbsr::core::nextWord(s));
    }
    return out;
}

using W = std::vector<std::string>;

} // namespace

TEST_CASE("words split on spaces, and quotes keep spaces together") {
    CHECK(words("set picture.sharpness  medium") == W{"set", "picture.sharpness", "medium"});
    CHECK(words("wifi \"My Net\" \"pass word\"") == W{"wifi", "My Net", "pass word"});
    CHECK(words("wifi Home \"a b\"") == W{"wifi", "Home", "a b"});
}

TEST_CASE("odd quotes: empty, unterminated, inside a word, glued after a closing one") {
    CHECK(words("wifi \"\" x") == W{"wifi", "", "x"});
    CHECK(words("wifi \"abc d") == W{"wifi", "abc d"});
    CHECK(words("ab\"c d") == W{"ab\"c", "d"});
    CHECK(words("\"a\"b") == W{"a", "b"});
}

TEST_CASE("nothing left gives an empty word") {
    char buf[] = "   ";
    char* s = buf;
    CHECK(std::string(gbsr::core::nextWord(s)).empty());
}
