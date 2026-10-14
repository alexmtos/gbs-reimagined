// GBS Reimagined — composition root: constructs the drivers, the video engine, the settings
// controller and the front ends, and runs the main loop (docs/design/architecture.md).

#include <Arduino.h>

#include "chip/fields.gen.h"
#include "chip/tv5725.h"
#include "core/controller.h"
#include "core/settings.gen.h"
#include "core/status.h"
#include "hal/board.h"
#include "hal/esp_i2c_bus.h"
#include "hal/pulse_timer.h"
#include "hal/si5351.h"
#include "net/web.h"
#include "net/wifi.h"
#include "platform/storage_esp.h"
#include "ui/console.h"
#include "version.h"
#include "video/engine.h"

namespace {

using namespace gbsr;

constexpr uint32_t kVideoTickMs = 20;
constexpr uint32_t kStatusEveryMs = 250;

hal::EspI2cBus bus;
tv5725::Chip chip(bus);
hal::Si5351 clockGen(bus, hal::board().clockGenXtalHz);
hal::EspPulseTimer pulseTimer(hal::board().pinVsyncProbe);
video::Engine engine(chip, &clockGen, &pulseTimer);
platform::EspFileStore files;
core::Controller controller(files, engine);
core::StatusModel statusModel;
net::Wifi wifi(files);
net::Web web(controller, statusModel, wifi);
ui::LoopStats loopStats;
ui::Console console(controller, engine, chip, wifi, loopStats);

bool clockGenPresent = false;
uint32_t lastTickMs = 0;
uint32_t lastStatusMs = 0;
uint32_t heapMin = UINT32_MAX;
uint32_t rebootAtMs = 0;

void refreshStatus(uint32_t now) {
    core::BoardInfo b;
    b.uptimeS = now / 1000;
    const uint32_t heap = ESP.getFreeHeap();
    heapMin = heap < heapMin ? heap : heapMin;
    b.heapFree = heap;
    b.heapMin = heapMin;
    b.rssi = wifi.rssi();
    b.ip = wifi.stationIp() != 0 ? wifi.stationIp() : wifi.apIp();
    b.clockGen = clockGenPresent;
    const uint32_t revertMs = controller.revertRemainingMs(now);
    core::buildStatus(statusModel, engine.status(),
                      controller.get(settings::Id::OutputFrameLock) != 0,
                      static_cast<uint16_t>((revertMs + 999) / 1000), controller.safeMode(), b);
}

} // namespace

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.printf_P(PSTR("GBS Reimagined %s on %s, settings schema %s\r\n"), GBSR_VERSION,
                    hal::board().name, settings::kSchemaHash);
    const auto& b = hal::board();
    if (!bus.begin(b.pinSda, b.pinScl, b.i2cClockHz)) {
        Serial.println(F("I2C bus stuck after recovery"));
    }
    pulseTimer.begin();
    // Clock input off while the generator initialises (protects pin 40 from start-up spikes).
    if (chip.probe()) {
        chip.write(tv5725::fields::PAD_CKIN_ENZ, 1);
    }
    clockGenPresent = b.hasClockGen && clockGen.probe() && clockGen.begin();
    Serial.printf_P(PSTR("clock generator: %s\r\n"), clockGenPresent ? "Si5351" : "absent");
    if (!files.begin()) {
        Serial.println(F("filesystem unavailable: settings will not be saved"));
    }
    controller.begin(false, millis());
    engine.begin(clockGenPresent);
    wifi.begin(millis());
    web.begin();
    console.printStatus();
}

void loop() {
    console.poll();
    const uint32_t now = millis();
    wifi.tick(now);
    web.tick();
    controller.tick(now);
    if (now - lastStatusMs >= kStatusEveryMs) {
        lastStatusMs = now;
        refreshStatus(now);
    }
    if (web.rebootRequested()) {
        if (rebootAtMs == 0) {
            rebootAtMs = now + 300; // let the answer reach the browser first
        } else if (static_cast<int32_t>(now - rebootAtMs) >= 0) {
            ESP.restart();
        }
    }
    if (now - lastTickMs >= kVideoTickMs) {
        lastTickMs = now;
        engine.tick();
        const uint32_t took = millis() - now;
        if (took > loopStats.longestTickMs) {
            loopStats.longestTickMs = took;
        }
    }
}
