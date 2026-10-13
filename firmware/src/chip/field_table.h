#pragma once

#include <stdint.h>

#include "chip/field.h"
#include "platform/progmem.h"

namespace gbsr::tv5725 {

// Generated (field_table.gen.cpp): every field by name, sorted, in flash.
extern const char kFieldNames[] PROGMEM;
extern const FieldInfo kFieldTable[] PROGMEM;
extern const uint16_t kFieldTableSize;

// Looks a field up by its exact (case-insensitive) datasheet name. Returns false if unknown.
bool findField(const char* name, FieldInfo& out);

// Copies the name of table entry `index` into `buf` (always NUL-terminated).
void fieldName(uint16_t index, char* buf, uint8_t bufSize);

} // namespace gbsr::tv5725
