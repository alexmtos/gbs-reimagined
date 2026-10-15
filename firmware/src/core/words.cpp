#include "core/words.h"

namespace gbsr::core {

char* nextWord(char*& s) {
    while (*s == ' ') {
        ++s;
    }
    const char end = *s == '"' ? '"' : ' ';
    if (end == '"') {
        ++s;
    }
    char* word = s;
    while (*s != '\0' && *s != end) {
        ++s;
    }
    if (*s != '\0') {
        *s++ = '\0';
    }
    return word;
}

} // namespace gbsr::core
