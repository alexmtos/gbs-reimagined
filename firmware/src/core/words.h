#pragma once

namespace gbsr::core {

// Splits off the first word of `s` (in place) and returns it; `s` moves past it. A word in double
// quotes may hold spaces (wifi "My Net" "pass word"); an unterminated quote runs to the end, and a
// quote inside a word is an ordinary character. Returns "" when nothing is left.
char* nextWord(char*& s);

} // namespace gbsr::core
