// GBS Reimagined — composition root.
//
// M0 scope: bring up the bus, identify the board's chips and report them on the serial console.
// The video engine, settings, web UI and OLED arrive in M1 (docs/roadmap.md).

#include <Arduino.h>

#include "chip/fields.gen.h"
#include "chip/tv5725.h"
#include "core/settings.gen.h"
#include "hal/board.h"
#include "hal/esp_i2c_bus.h"
#include "hal/si5351.h"
#include "version.h"

namespace {

using namespace gbsr;

hal::EspI2cBus bus;
tv5725::Chip chip(bus);
hal::Si5351 clockGen(bus, hal::board().clockGenXtalHz);

bool scalerPresent = false;
bool clockGenPresent = false;
bool displayPresent = false;

void probeBoard() {
    scalerPresent = chip.probe();
    clockGenPresent = hal::board().hasClockGen && clockGen.probe();
    displayPresent = hal::board().hasDisplay && bus.probe(hal::board().addrDisplay);
}

void printStatus() {
    Serial.printf_P(PSTR("GBS Reimagined %s on %s\r\n"), GBSR_VERSION, hal::board().name);
    if (scalerPresent) {
        chip.clearError();
        const uint32_t product = chip.get(tv5725::fields::CHIP_ID_PRODUCT);
        const uint32_t revision = chip.get(tv5725::fields::CHIP_ID_REVISION);
        Serial.printf_P(PSTR("  scaler     TV5725 product 0x%02X revision 0x%02X%s\r\n"),
                        static_cast<unsigned>(product), static_cast<unsigned>(revision),
                        chip.ok() ? "" : " (read error)");
    } else {
        Serial.println(F("  scaler     not answering (power? I2C?)"));
    }
    Serial.printf_P(PSTR("  clock gen  %s\r\n"), clockGenPresent ? "Si5351 ready" : "absent");
    Serial.printf_P(PSTR("  display    %s\r\n"), displayPresent ? "SSD1306 found" : "absent");
    Serial.printf_P(PSTR("  settings   schema %s\r\n"), settings::kSchemaHash);
    Serial.printf_P(PSTR("  heap       %u bytes free\r\n"), ESP.getFreeHeap());
}

void handleLine(const char* line) {
    if (strcmp_P(line, PSTR("status")) == 0) {
        printStatus();
    } else if (strcmp_P(line, PSTR("probe")) == 0) {
        probeBoard();
        printStatus();
    } else if (strcmp_P(line, PSTR("reboot")) == 0) {
        ESP.restart();
    } else if (line[0] != '\0') {
        Serial.println(F("commands: status, probe, reboot"));
    }
}

char lineBuf[64];
uint8_t lineLen = 0;

void pollConsole() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            lineBuf[lineLen] = '\0';
            handleLine(lineBuf);
            lineLen = 0;
        } else if (lineLen < sizeof lineBuf - 1) {
            lineBuf[lineLen++] = c;
        }
    }
}

} // namespace

void setup() {
    Serial.begin(115200);
    Serial.println();
    const auto& b = hal::board();
    if (!bus.begin(b.pinSda, b.pinScl, b.i2cClockHz)) {
        Serial.println(F("I2C bus stuck after recovery"));
    }
    probeBoard();
    if (clockGenPresent) {
        clockGen.begin(); // all outputs off until the video engine adopts the clock
    }
    printStatus();
}

void loop() {
    pollConsole();
}
