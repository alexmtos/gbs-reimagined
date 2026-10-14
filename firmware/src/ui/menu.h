#pragma once

#include <stdint.h>

#include "core/controller.h"
#include "core/settings_store.h"
#include "core/status.h"
#include "ui/messages.gen.h"

namespace gbsr::ui {

// What the OLED shows: six lines of at most 21 glyphs (UTF-8, Latin-1 range), and which lines are
// highlighted. The renderer (ui/oled.cpp) draws it; the model below fills it.
struct Frame {
    static constexpr uint8_t kLines = 6;
    static constexpr uint8_t kCols = 21;
    char line[kLines][kCols * 2 + 1] = {};
    uint8_t inverted = 0; // bit n: line n highlighted

    bool operator==(const Frame& o) const;
    bool operator!=(const Frame& o) const { return !(*this == o); }
};

// The network as the OLED needs it, filled by the composition root from net::Wifi.
struct NetView {
    enum class Mode : uint8_t { Joining, Joined, Setup };
    Mode mode = Mode::Setup;
    const char* ssid = "";
    const char* apSsid = "";
    const char* apPassword = "";
    uint32_t ip = 0;
    uint8_t failure = 0; // net::Wifi::Failure
};

// The OLED and knob interface (docs/design/ux.md, "OLED + encoder"): home screen, menu of setting
// groups, value editing, network and status pages, actions, and the revert takeover. Pure logic:
// it reads the status model and changes settings through the controller like every front end, and
// renders text lines; host-tested without a display.
//
// Gestures: turn = move or change (coarse ×10 while the knob is held down), press = enter or
// confirm, long press = back. Risky settings apply on the press, so turning through output modes
// does not reload a program at every detent; picture settings apply while turning.
class Menu {
public:
    static constexpr uint32_t kHomeAfterMs = 30'000;

    Menu(core::Controller& ctl, const core::StatusModel& status, const NetView& net)
        : ctl_(ctl), status_(status), net_(net) {}

    void turn(int steps, bool coarse, uint32_t nowMs);
    void press(uint32_t nowMs);
    void longPress(uint32_t nowMs);
    // Revert takeover and the return home after inactivity.
    void tick(uint32_t nowMs);
    void render(Frame& f, uint32_t nowMs) const;

    bool rebootRequested() const { return rebootRequested_; }
    uint32_t lastInputMs() const { return lastInputMs_; }

private:
    enum class Screen : uint8_t {
        Home,
        Root,
        Group,
        Edit,
        Network,
        Status,
        Confirm,
        Action,
        Revert
    };
    enum class Kind : uint8_t { Setting, Calibrate, Reboot, FactoryReset, Back };
    struct Item {
        Kind kind;
        settings::Id id;
    };

    void openRoot();
    void openGroup(uint8_t group);
    void startEdit(settings::Id id);
    void activate(const Item& item, uint32_t nowMs);
    void moveCursor(int steps, uint8_t count);

    void renderHome(Frame& f) const;
    void renderList(Frame& f) const;
    void renderEdit(Frame& f) const;
    void renderNetwork(Frame& f) const;
    void renderStatus(Frame& f) const;
    void renderConfirm(Frame& f) const;
    void renderAction(Frame& f) const;
    void renderRevert(Frame& f, uint32_t nowMs) const;

    uint8_t locale() const;
    void msg(i18n::Msg m, char* out, size_t cap) const;
    void valueText(settings::Id id, int16_t v, char* out, size_t cap) const;

    core::Controller& ctl_;
    const core::StatusModel& status_;
    const NetView& net_;

    Screen screen_ = Screen::Home;
    Screen beforeRevert_ = Screen::Home;
    uint8_t group_ = 0;
    uint8_t cursor_ = 0;
    uint8_t top_ = 0; // first visible row of a list
    Item items_[16] = {};
    uint8_t itemCount_ = 0;
    settings::Id editId_{};
    int16_t editValue_ = 0;
    int16_t editOriginal_ = 0;
    Kind pending_ = Kind::Back; // the action a Confirm or Action screen is about
    bool actionStarted_ = false;
    bool revertChoiceKeep_ = true;
    bool rebootRequested_ = false;
    uint32_t lastInputMs_ = 0;
};

} // namespace gbsr::ui
