#pragma once

#include <stdint.h>

#include "chip/tv5725.h"
#include "core/controller.h"
#include "net/wifi.h"
#include "video/engine.h"

namespace gbsr::ui {

// Main-loop figures the console reports.
struct LoopStats {
    uint32_t longestTickMs = 0;
};

// The serial console (docs/design/ux.md, "Serial console"): line-based, case-insensitive, English.
// Settings go through the controller like every other front end; raw chip access is an expert
// tool behind `expert on`.
class Console {
public:
    Console(core::Controller& ctl, video::Engine& engine, tv5725::Chip& chip, net::Wifi& wifi,
            const LoopStats& loop)
        : ctl_(ctl), engine_(engine), chip_(chip), wifi_(wifi), loop_(loop) {}

    // Reads pending serial bytes and runs complete lines; reports revert outcomes.
    void poll();
    void printStatus();

private:
    void handleLine(char* line);
    void cmdGet(char* args);
    void cmdSet(char* args);
    void cmdList(const char* prefix);
    void cmdReg(char* args);
    void cmdDump(const char* args);
    void cmdLevels(char* args);
    void cmdWifi(char* args);
    void reportSet(const char* name, core::Controller::Outcome outcome);
    void help();

    core::Controller& ctl_;
    video::Engine& engine_;
    tv5725::Chip& chip_;
    net::Wifi& wifi_;
    const LoopStats& loop_;
    char line_[96] = {};
    uint8_t len_ = 0;
    bool expert_ = false;
    bool revertWasPending_ = false;
    bool factoryResetAsked_ = false;
};

} // namespace gbsr::ui
