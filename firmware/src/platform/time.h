#pragma once

#include <stdint.h>

// Time source for every module, so host tests can control it. On the board these map to the
// Arduino core; sleeping yields to the Wi-Fi stack (ADR 0013 bounds how long a step may sleep).
namespace gbsr::platform {

uint32_t nowMs();
uint32_t nowUs();
void sleepMs(uint32_t ms);
void sleepUs(uint32_t us);

#if !defined(ARDUINO)
// Host builds run on a simulated clock (time_host.cpp); tests move it forward with this.
void advanceMs(uint32_t ms);
#endif

// Milliseconds elapsed since `since`, robust to the 49-day wrap.
inline uint32_t elapsedMs(uint32_t since) {
    return nowMs() - since;
}

} // namespace gbsr::platform
