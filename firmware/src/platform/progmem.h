#pragma once

// Flash-resident constants. On the ESP8266, `const` data without PROGMEM is copied to RAM at
// boot, so tables and strings live in flash and are read with the *_P accessors. The host build
// (unit tests) maps the same names onto ordinary memory.

#if defined(ARDUINO)
#include <pgmspace.h>
#else
#include <stdint.h>
#include <string.h>

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t*>(addr))
#define pgm_read_dword(addr) (*reinterpret_cast<const uint32_t*>(addr))
#define pgm_read_ptr(addr) (*reinterpret_cast<const void* const*>(addr))
#define memcpy_P memcpy
#define strcmp_P strcmp
#define strcasecmp_P strcasecmp
#define strlen_P strlen
#define strncpy_P strncpy
#endif
