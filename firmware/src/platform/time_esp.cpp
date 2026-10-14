#if defined(ARDUINO)

#include <Arduino.h>

#include "platform/time.h"

namespace gbsr::platform {

uint32_t nowMs() {
    return millis();
}

uint32_t nowUs() {
    return micros();
}

void sleepMs(uint32_t ms) {
    delay(ms);
}

void sleepUs(uint32_t us) {
    delayMicroseconds(us);
}

} // namespace gbsr::platform

#endif
