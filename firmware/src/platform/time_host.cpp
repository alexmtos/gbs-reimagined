#if !defined(ARDUINO)

#include "platform/time.h"

namespace gbsr::platform {

namespace {

// Simulated clock for host tests. Every reading costs a little time, as an I2C transaction does
// on the board, so loops bounded by the clock terminate.
uint64_t gNowUs = 0;
constexpr uint32_t kReadCostUs = 100;

} // namespace

uint32_t nowMs() {
    gNowUs += kReadCostUs;
    return static_cast<uint32_t>(gNowUs / 1000);
}

uint32_t nowUs() {
    gNowUs += kReadCostUs;
    return static_cast<uint32_t>(gNowUs);
}

void sleepMs(uint32_t ms) {
    gNowUs += static_cast<uint64_t>(ms) * 1000;
}

void sleepUs(uint32_t us) {
    gNowUs += us;
}

void advanceMs(uint32_t ms) {
    sleepMs(ms);
}

} // namespace gbsr::platform

#endif
