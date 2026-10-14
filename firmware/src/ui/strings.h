#pragma once

#include <stdint.h>

#include "platform/progmem.h"

namespace gbsr::i18n {

// Generated per-locale string tables for the OLED (ui/strings.gen.cpp). Every string is UTF-8 in
// one flash blob; the tables hold offsets into it. Enum value labels of one setting are joined by
// the unit separator 0x1F, in schema order, followed by the labels of its special values.
struct LocaleTables {
    const char* text;
    const uint16_t* label;  // per setting
    const uint16_t* hint;   // per setting
    const uint16_t* values; // per setting, 0xFFFF when none
    const uint16_t* group;  // per group
    const uint16_t* msg;    // per i18n::Msg (ui/messages.gen.h)
};

extern const LocaleTables kLocales[] PROGMEM;
extern const uint8_t kLocaleCount;

} // namespace gbsr::i18n
