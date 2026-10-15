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
// confirm, long press = back (on the home screen: the profiles). Risky settings apply on the
// press, so turning through output modes does not reload a program at every detent; picture
// settings apply while turning.
// "Profile 3" in a language of the board (i18n tables): the controller's default profile name.
void profileSlotName(uint8_t slot, uint8_t language, char* out, size_t cap);

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
        Revert,
        Profiles,       // the saved profiles, "Save as new" and Back
        ProfileItem,    // what to do with one profile
        ProfileConfirm, // a second press before saving over or deleting it
        ProfileDone,    // how it went
    };
    enum class ProfileAct : uint8_t { Load, Save, Delete, New };
    static constexpr uint8_t kRowNew = 0xFE; // rows_ entries besides slot numbers
    static constexpr uint8_t kRowBack = 0xFF;
    static constexpr uint8_t kListRows = 5;
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
    void leaveToRoot(uint8_t rootIndex);
    void openProfiles(uint8_t cursorSlot);
    void loadRowNames();
    void openProfileItem(uint8_t slot);
    void profileDone(ProfileAct act, bool ok, i18n::Msg ok_msg);
    void pressProfiles(uint32_t nowMs);
    void pressProfileItem(uint32_t nowMs);

    void renderHome(Frame& f) const;
    void renderList(Frame& f) const;
    void renderEdit(Frame& f) const;
    void renderNetwork(Frame& f) const;
    void renderStatus(Frame& f) const;
    void renderConfirm(Frame& f) const;
    void renderAction(Frame& f) const;
    void renderRevert(Frame& f, uint32_t nowMs) const;
    void renderProfiles(Frame& f) const;
    void renderProfileItem(Frame& f) const;
    void renderProfileMessage(Frame& f) const;
    void slotTitle(char* out, size_t cap) const;

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

    // Profiles: the list's rows (slot numbers, kRowNew, kRowBack), the names of the visible ones
    // (read from flash when the list opens or scrolls, not at every frame), and the chosen slot.
    uint8_t rows_[core::kProfileSlots + 2] = {};
    uint8_t rowCount_ = 0;
    char rowNames_[kListRows][core::kProfileNameCap] = {};
    uint8_t slot_ = 0;
    char slotName_[core::kProfileNameCap] = {};
    bool profilesFromHome_ = false;
    ProfileAct profileAct_ = ProfileAct::Load;
    i18n::Msg profileMsg_ = i18n::Msg::ProfilesFailed;
};

} // namespace gbsr::ui
