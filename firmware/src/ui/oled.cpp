#if defined(ARDUINO)

#include "ui/oled.h"

#include <U8g2lib.h>
#include <string.h>

#include "platform/time.h"

namespace gbsr::ui {

namespace {

// U8g2 talks to the display through these two callbacks; they reach our bus through a pointer, as
// there is one display.
hal::I2cBus* gBus = nullptr;
uint8_t gBuf[160];
size_t gLen = 0;
u8g2_t gU8g2;

uint8_t byteCallback(u8x8_t* u8x8, uint8_t msg, uint8_t argInt, void* argPtr) {
    switch (msg) {
    case U8X8_MSG_BYTE_START_TRANSFER:
        gLen = 0;
        return 1;
    case U8X8_MSG_BYTE_SEND:
        if (gLen + argInt <= sizeof gBuf) {
            memcpy(gBuf + gLen, argPtr, argInt);
            gLen += argInt;
        }
        return 1;
    case U8X8_MSG_BYTE_END_TRANSFER:
        return gBus->write(u8x8_GetI2CAddress(u8x8) >> 1, gBuf, gLen) ? 1 : 0;
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC:
        return 1;
    default:
        return 0;
    }
}

uint8_t gpioCallback(u8x8_t*, uint8_t msg, uint8_t argInt, void*) {
    switch (msg) {
    case U8X8_MSG_DELAY_MILLI:
        platform::sleepMs(argInt);
        return 1;
    case U8X8_MSG_DELAY_10MICRO:
        platform::sleepUs(10u * argInt);
        return 1;
    case U8X8_MSG_DELAY_100NANO:
    case U8X8_MSG_DELAY_NANO:
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        return 1;
    default:
        return 1; // no reset or control pins on this board
    }
}

constexpr uint8_t kLineHeight = 10;
constexpr uint8_t kBaseline = 8; // 6x10 font: ascent 7, one pixel of air above

} // namespace

bool Oled::begin() {
    present_ = bus_.probe(address_);
    if (!present_) {
        return false;
    }
    gBus = &bus_;
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&gU8g2, U8G2_R0, byteCallback, gpioCallback);
    u8x8_SetI2CAddress(&gU8g2.u8x8, static_cast<uint8_t>(address_ << 1));
    u8g2_InitDisplay(&gU8g2);
    u8g2_SetPowerSave(&gU8g2, 0);
    u8g2_SetFont(&gU8g2, u8g2_font_6x10_tf);
    u8g2_SetFontMode(&gU8g2, 1);
    return true;
}

void Oled::show(const Frame& f) {
    if (!present_ || asleep_ || (shownValid_ && f == shown_)) {
        return;
    }
    u8g2_ClearBuffer(&gU8g2);
    for (uint8_t i = 0; i < Frame::kLines; ++i) {
        const uint8_t top = static_cast<uint8_t>(i * kLineHeight + 2);
        const bool inverted = (f.inverted >> i) & 1;
        if (inverted) {
            u8g2_SetDrawColor(&gU8g2, 1);
            u8g2_DrawBox(&gU8g2, 0, top - 1, 128, kLineHeight);
            u8g2_SetDrawColor(&gU8g2, 0);
        } else {
            u8g2_SetDrawColor(&gU8g2, 1);
        }
        u8g2_DrawUTF8(&gU8g2, 1, top + kBaseline - 1, f.line[i]);
    }
    u8g2_SetDrawColor(&gU8g2, 1);
    u8g2_SendBuffer(&gU8g2);
    shown_ = f;
    shownValid_ = true;
}

void Oled::sleep(bool asleep) {
    if (!present_ || asleep == asleep_) {
        return;
    }
    asleep_ = asleep;
    u8g2_SetPowerSave(&gU8g2, asleep ? 1 : 0);
    if (!asleep) {
        shownValid_ = false; // redraw on wake
    }
}

} // namespace gbsr::ui

#endif
