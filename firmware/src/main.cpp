// GBS Reimagined — composition root: constructs the drivers, the video engine, the settings
// controller and the front ends, and runs the main loop (docs/design/architecture.md).

#include <Arduino.h>

#include "chip/fields.gen.h"
#include "chip/tv5725.h"
#include "core/controller.h"
#include "core/settings.gen.h"
#include "core/status.h"
#include "hal/board.h"
#include "hal/encoder.h"
#include "hal/esp_i2c_bus.h"
#include "hal/pulse_timer.h"
#include "hal/si5351.h"
#include "net/improv_wifi.h"
#include "net/web.h"
#include "net/wifi.h"
#include "platform/storage_esp.h"
#include "ui/console.h"
#include "ui/knob.h"
#include "ui/menu.h"
#include "ui/oled.h"
#include "version.h"
#include "video/engine.h"

namespace {

using namespace gbsr;

constexpr uint32_t kVideoTickMs = 20;
constexpr uint32_t kStatusEveryMs = 250;
constexpr uint32_t kOledEveryMs = 50;
constexpr uint32_t kSplashMs = 1500;
constexpr uint32_t kSafeModeHoldMs = 5000;
constexpr uint32_t kSaverMinutes[] = {0, 1, 3, 5, 10}; // system.oledSaver

// What the browser installer shows; the name must match the one in its manifests.
constexpr char kFirmwareName[] PROGMEM = "GBS Reimagined";
constexpr char kFirmwareVersion[] PROGMEM = GBSR_VERSION;
constexpr char kChipName[] PROGMEM = "ESP8266";

hal::EspI2cBus bus;
tv5725::Chip chip(bus);
hal::Si5351 clockGen(bus, hal::board().clockGenXtalHz);
hal::EspPulseTimer pulseTimer(hal::board().pinVsyncProbe);
video::Engine engine(chip, &clockGen, &pulseTimer);
platform::EspFileStore files;
core::Controller controller(files, engine);

// New sources reach the profiles before their program is chosen (ADR 0014).
class DetectedToProfiles : public video::SourceListener {
public:
    void sourceDetected(const video::DetectedSource& d) override {
        controller.sourceDetected(core::detectedSource(d), millis());
    }
} detectedToProfiles;
core::StatusModel statusModel;
net::Wifi wifi(files);
net::Web web(controller, statusModel, wifi);
ui::LoopStats loopStats;
net::ImprovWifi improvWifi(wifi);
core::ImprovSerial improv(improvWifi, {kFirmwareName, kFirmwareVersion, kChipName, wifi.apSsid()});
ui::Console console(controller, engine, chip, wifi, improv, loopStats);
hal::EspEncoder encoder(hal::board().pinEncoderA, hal::board().pinEncoderB,
                        hal::board().pinEncoderButton, hal::board().encoderClockwiseNegative);
ui::Knob knob;
ui::NetView netView;
ui::Menu menu(controller, statusModel, netView);
ui::Oled oled(bus, hal::board().addrDisplay);
ui::Frame frame;

bool clockGenPresent = false;
uint32_t lastTickMs = 0;
uint32_t lastStatusMs = 0;
uint32_t heapMin = UINT32_MAX;
uint32_t rebootAtMs = 0;
uint32_t lastOledMs = 0;
uint32_t headlineChangedMs = 0;
video::Headline lastHeadline = video::Headline::ScalerMissing;

// Boot splash, in both languages since the settings are not loaded yet. Holding the knob for five
// seconds while it shows starts in safe mode (docs/design/ux.md, Safety); at power-on the same
// button would select the ESP8266's bootloader, hence the wait for the splash.
bool splashAndSafeMode() {
    ui::Frame f;
    snprintf(f.line[0], sizeof f.line[0], "GBS Reimagined");
    snprintf(f.line[1], sizeof f.line[1], "%s", GBSR_VERSION);
    snprintf(f.line[3], sizeof f.line[3], "Hold knob: safe mode");
    snprintf(f.line[4], sizeof f.line[4], "Segure: modo seguro");
    f.inverted = 0x01;
    oled.show(f);
    const uint32_t start = millis();
    uint32_t heldSince = 0;
    while (millis() - start < kSplashMs || heldSince != 0) {
        if (encoder.buttonDown()) {
            heldSince = heldSince != 0 ? heldSince : millis();
            const uint32_t held = millis() - heldSince;
            if (held >= kSafeModeHoldMs) {
                return true;
            }
            snprintf(f.line[5], sizeof f.line[5], "%lu...",
                     static_cast<unsigned long>((kSafeModeHoldMs - held + 999) / 1000));
            oled.show(f);
        } else if (heldSince != 0) {
            return false; // let go before five seconds
        }
        delay(20);
    }
    return false;
}

void serviceKnobAndOled(uint32_t now) {
    const ui::Knob::Events ev = knob.update(encoder.buttonDown(), encoder.takeDetents(), now);
    if (ev.turn != 0) {
        const bool reverse = controller.get(settings::Id::SystemEncoderReverse) != 0;
        menu.turn(reverse ? -ev.turn : ev.turn, ev.coarse, now);
    }
    if (ev.press) {
        menu.press(now);
    }
    if (ev.longPress) {
        menu.longPress(now);
    }
    const bool input = ev.turn != 0 || ev.press || ev.longPress;
    if (!input && now - lastOledMs < kOledEveryMs) {
        return;
    }
    lastOledMs = now;
    netView.mode = static_cast<ui::NetView::Mode>(wifi.mode());
    netView.ssid = wifi.ssid();
    netView.apSsid = wifi.apSsid();
    netView.apPassword = net::Wifi::kApPassword;
    netView.ip = wifi.stationIp();
    netView.failure = static_cast<uint8_t>(wifi.lastFailure());
    menu.tick(now);
    if (statusModel.headline != lastHeadline) {
        lastHeadline = statusModel.headline;
        headlineChangedMs = now; // something happened: show it
    }
    const int16_t saver = controller.get(settings::Id::SystemOledSaver);
    const uint32_t saverMs = kSaverMinutes[saver >= 0 && saver < 5 ? saver : 3] * 60'000u;
    const uint32_t lastActivity =
        menu.lastInputMs() > headlineChangedMs ? menu.lastInputMs() : headlineChangedMs;
    oled.sleep(saverMs != 0 && now - lastActivity >= saverMs && !controller.revertPending());
    menu.render(frame, now);
    oled.show(frame);
    if (menu.rebootRequested() && rebootAtMs == 0) {
        rebootAtMs = millis() + 500;
    }
}

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
    controller.onSource(core::settledSource(engine.status()), now);
    const uint32_t revertMs = controller.revertRemainingMs(now);
    core::ProfileInfo p;
    p.slot = controller.activeProfile();
    p.modified = controller.profileModified();
    p.name = controller.profiles().activeName();
    core::buildStatus(statusModel, engine.status(),
                      controller.get(settings::Id::OutputFrameLock) != 0,
                      static_cast<uint16_t>((revertMs + 999) / 1000), controller.safeMode(), b, p,
                      controller.sequence());
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
    encoder.begin();
    oled.begin();
    const bool safeMode = splashAndSafeMode();
    if (safeMode) {
        Serial.println(F("safe mode: saved settings not loaded, nothing will be saved"));
    }
    controller.begin(safeMode, millis());
    engine.setSourceListener(&detectedToProfiles);
    engine.begin(clockGenPresent);
    wifi.begin(millis());
    web.begin();
    console.printStatus();
}

void loop() {
    console.poll();
    const uint32_t now = millis();
    wifi.tick(now);
    improv.tick(now);
    web.tick();
    controller.tick(now);
    if (now - lastStatusMs >= kStatusEveryMs) {
        lastStatusMs = now;
        refreshStatus(now);
    }
    serviceKnobAndOled(now);
    if (web.rebootRequested() && rebootAtMs == 0) {
        // Let the answer reach the browser first. From now, not from the pass's start: a firmware
        // upload keeps web.tick() busy for seconds, and the answer was lost (bench M3).
        rebootAtMs = millis() + 500;
    }
    if (rebootAtMs != 0 && static_cast<int32_t>(millis() - rebootAtMs) >= 0) {
        ESP.restart();
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
