#include "ui/menu.h"

#include <stdio.h>
#include <string.h>

#include "platform/progmem.h"
#include "ui/strings.h"
#include "version.h"
#include "video/adc_levels.h"

namespace gbsr::ui {

using settings::Id;

namespace {

// Setting groups in schema order (schema/settings.yaml `groups`), by id prefix.
constexpr const char* kGroupPrefix[] = {"output.", "input.", "picture.", "system."};
constexpr uint8_t kRows = 5; // list rows; the sixth line carries the hint

// The root menu in docs/design/ux.md order: Output, Input, Picture, Profiles, Network, Status,
// System, then Back.
enum class RootEntry : uint8_t { Group, Profiles, Network, Status, Back };
struct RootItem {
    RootEntry entry;
    uint8_t group; // for RootEntry::Group
};
constexpr RootItem kRoot[] = {
    {RootEntry::Group, 0},    {RootEntry::Group, 1},   {RootEntry::Group, 2},
    {RootEntry::Profiles, 0}, {RootEntry::Network, 0}, {RootEntry::Status, 0},
    {RootEntry::Group, 3},    {RootEntry::Back, 0},
};
constexpr uint8_t kRootItems = sizeof kRoot / sizeof kRoot[0];

uint8_t rootIndex(RootEntry entry, uint8_t group = 0) {
    for (uint8_t i = 0; i < kRootItems; ++i) {
        if (kRoot[i].entry == entry && (entry != RootEntry::Group || kRoot[i].group == group)) {
            return i;
        }
    }
    return 0;
}

// --- UTF-8 text in a fixed number of glyphs ------------------------------------------------------

size_t glyphs(const char* s) {
    size_t n = 0;
    for (; *s != '\0'; ++s) {
        n += (static_cast<uint8_t>(*s) & 0xC0) != 0x80;
    }
    return n;
}

// Copies at most `max` glyphs (never a partial character).
void copyGlyphs(char* dst, size_t cap, const char* src, size_t max) {
    size_t used = 0;
    size_t count = 0;
    while (*src != '\0') {
        size_t len = 1;
        const uint8_t c = static_cast<uint8_t>(*src);
        if (c >= 0xF0) {
            len = 4;
        } else if (c >= 0xE0) {
            len = 3;
        } else if (c >= 0xC0) {
            len = 2;
        }
        if (count == max || used + len >= cap) {
            break;
        }
        memcpy(dst + used, src, len);
        used += len;
        src += len;
        ++count;
    }
    dst[used] = '\0';
}

void setLine(Frame& f, uint8_t n, const char* text) {
    copyGlyphs(f.line[n], sizeof f.line[n], text, Frame::kCols);
}

// `left` and `right` on one line, the right part flush right; the left part gives way.
void twoColumns(Frame& f, uint8_t n, const char* left, const char* right) {
    const size_t r = glyphs(right) < Frame::kCols ? glyphs(right) : Frame::kCols;
    const size_t room = r < Frame::kCols ? Frame::kCols - r - 1 : 0;
    char l[Frame::kCols * 2 + 1];
    copyGlyphs(l, sizeof l, left, room);
    char out[160]; // the left part, padding and a right part of any length; setLine trims it
    const size_t pad = Frame::kCols - glyphs(l) - r;
    snprintf(out, sizeof out, "%s%*s%s", l, static_cast<int>(pad), "", right);
    setLine(f, n, out);
}

void centered(Frame& f, uint8_t n, const char* text) {
    const size_t g = glyphs(text);
    char out[Frame::kCols * 4 + 2];
    const int pad = g < Frame::kCols ? static_cast<int>((Frame::kCols - g) / 2) : 0;
    snprintf(out, sizeof out, "%*s%s", pad, "", text);
    setLine(f, n, out);
}

// Breaks `text` over lines n and n+1 at the last space that fits.
void wrapTwo(Frame& f, uint8_t n, const char* text) {
    if (glyphs(text) <= Frame::kCols) {
        setLine(f, n, text);
        return;
    }
    size_t count = 0;
    const char* cut = nullptr;
    for (const char* p = text; *p != '\0' && count <= Frame::kCols; ++p) {
        if (*p == ' ') {
            cut = p;
        }
        count += (static_cast<uint8_t>(*p) & 0xC0) != 0x80;
    }
    if (cut == nullptr) {
        setLine(f, n, text);
        return;
    }
    char first[Frame::kCols * 2 + 1];
    const size_t len = static_cast<size_t>(cut - text) < sizeof first - 1
                           ? static_cast<size_t>(cut - text)
                           : sizeof first - 1;
    memcpy(first, text, len);
    first[len] = '\0';
    setLine(f, n, first);
    setLine(f, static_cast<uint8_t>(n + 1), cut + 1);
}

// --- generated tables ----------------------------------------------------------------------------

i18n::LocaleTables tables(uint8_t locale) {
    i18n::LocaleTables t{};
    memcpy_P(&t, &i18n::kLocales[locale < i18n::kLocaleCount ? locale : 0], sizeof t);
    return t;
}

void copyText(const char* blob, uint16_t offset, char* out, size_t cap) {
    strncpy_P(out, blob + offset, cap - 1);
    out[cap - 1] = '\0';
}

// The `index`-th label of a list joined by 0x1F.
bool listLabel(const char* blob, uint16_t offset, uint8_t index, char* out, size_t cap) {
    char all[160];
    copyText(blob, offset, all, sizeof all);
    char* p = all;
    for (uint8_t i = 0; i < index; ++i) {
        p = strchr(p, '\x1f');
        if (p == nullptr) {
            return false;
        }
        ++p;
    }
    char* end = strchr(p, '\x1f');
    if (end != nullptr) {
        *end = '\0';
    }
    snprintf(out, cap, "%s", p);
    return true;
}

// The headline sentences follow video::Headline's order, so one offset maps a headline to its text.
static_assert(static_cast<uint8_t>(i18n::Msg::HeadlinePaused) -
                      static_cast<uint8_t>(i18n::Msg::HeadlineScalerMissing) ==
                  static_cast<uint8_t>(video::Headline::Paused),
              "oled.headline.* messages must follow video::Headline");

bool inGroup(Id id, uint8_t group) {
    char name[32];
    settings::idName(id, name, sizeof name);
    return strncmp(name, kGroupPrefix[group], strlen(kGroupPrefix[group])) == 0;
}

} // namespace

bool Frame::operator==(const Frame& o) const {
    if (inverted != o.inverted) {
        return false;
    }
    for (uint8_t i = 0; i < kLines; ++i) {
        if (strcmp(line[i], o.line[i]) != 0) {
            return false;
        }
    }
    return true;
}

uint8_t Menu::locale() const {
    return static_cast<uint8_t>(ctl_.get(Id::SystemLanguage));
}

void Menu::msg(i18n::Msg m, char* out, size_t cap) const {
    const i18n::LocaleTables t = tables(locale());
    copyText(t.text, pgm_read_word(&t.msg[static_cast<uint8_t>(m)]), out, cap);
}

void Menu::valueText(Id id, int16_t v, char* out, size_t cap) const {
    const settings::Def d = settings::def(id);
    if (d.type == settings::Type::Bool) {
        msg(v ? i18n::Msg::On : i18n::Msg::Off, out, cap);
        return;
    }
    const i18n::LocaleTables t = tables(locale());
    const uint16_t values = pgm_read_word(&t.values[static_cast<uint8_t>(id)]);
    if (d.type == settings::Type::Enum && values != 0xFFFF &&
        listLabel(t.text, values, static_cast<uint8_t>(v), out, cap)) {
        return;
    }
    // Integers: a special value has a label after the enum labels (there are none for ints).
    for (uint8_t s = 0; s < 2; ++s) {
        if (d.special[s] != settings::kNoSpecial && d.special[s] == v && values != 0xFFFF &&
            listLabel(t.text, values, s, out, cap)) {
            return;
        }
    }
    if (id == Id::SystemStartupProfile && v > 0) {
        profileSlotName(static_cast<uint8_t>(v), locale(), out, cap);
        return;
    }
    snprintf(out, cap, d.min < 0 ? "%+d" : "%d", v);
}

void profileSlotName(uint8_t slot, uint8_t language, char* out, size_t cap) {
    const i18n::LocaleTables t = tables(language);
    char fmt[32];
    copyText(t.text, pgm_read_word(&t.msg[static_cast<uint8_t>(i18n::Msg::ProfilesSlot)]), fmt,
             sizeof fmt); // "Profile {n}"
    char* at = strstr(fmt, "{n}");
    if (at == nullptr) {
        snprintf(out, cap, "%u", slot);
        return;
    }
    *at = '\0';
    snprintf(out, cap, "%s%u%s", fmt, slot, at + 3);
}

// ------------------------------------------------------------------------------------------------
// Navigation

void Menu::openRoot() {
    screen_ = Screen::Root;
    cursor_ = top_ = 0;
}

void Menu::openGroup(uint8_t group) {
    group_ = group;
    itemCount_ = 0;
    const bool advanced = ctl_.get(Id::SystemShowAdvanced) != 0;
    for (uint8_t i = 0; i < settings::kCount && itemCount_ < 12; ++i) {
        const Id id = static_cast<Id>(i);
        const settings::Def d = settings::def(id);
        if (settings::available(id) && inGroup(id, group) && (advanced || !d.advanced())) {
            items_[itemCount_++] = {Kind::Setting, id};
        }
    }
    if (group == 1 && advanced) {
        items_[itemCount_++] = {Kind::Calibrate, Id{}};
    }
    if (group == 3) {
        items_[itemCount_++] = {Kind::Reboot, Id{}};
        items_[itemCount_++] = {Kind::FactoryReset, Id{}};
    }
    items_[itemCount_++] = {Kind::Back, Id{}};
    screen_ = Screen::Group;
    cursor_ = top_ = 0;
}

void Menu::startEdit(Id id) {
    editId_ = id;
    editValue_ = editOriginal_ = ctl_.get(id);
    screen_ = Screen::Edit;
}

void Menu::moveCursor(int steps, uint8_t count) {
    int c = cursor_ + steps;
    c = c < 0 ? 0 : (c >= count ? count - 1 : c);
    cursor_ = static_cast<uint8_t>(c);
    if (cursor_ < top_) {
        top_ = cursor_;
    } else if (cursor_ >= top_ + kRows) {
        top_ = static_cast<uint8_t>(cursor_ - kRows + 1);
    }
}

void Menu::activate(const Item& item, uint32_t nowMs) {
    switch (item.kind) {
    case Kind::Setting:
        startEdit(item.id);
        break;
    case Kind::Calibrate:
        pending_ = Kind::Calibrate;
        actionStarted_ = ctl_.calibrateLevels();
        screen_ = Screen::Action;
        break;
    case Kind::Reboot:
    case Kind::FactoryReset:
        pending_ = item.kind;
        screen_ = Screen::Confirm;
        break;
    case Kind::Back:
        leaveToRoot(rootIndex(RootEntry::Group, group_));
        break;
    }
    (void)nowMs;
}

void Menu::leaveToRoot(uint8_t index) {
    openRoot();
    moveCursor(index, kRootItems);
}

void Menu::turn(int steps, bool coarse, uint32_t nowMs) {
    if (steps == 0) {
        return;
    }
    lastInputMs_ = nowMs;
    switch (screen_) {
    case Screen::Root:
        moveCursor(steps, kRootItems);
        break;
    case Screen::Group:
        moveCursor(steps, itemCount_);
        break;
    case Screen::Profiles: {
        const uint8_t top = top_;
        moveCursor(steps, rowCount_);
        if (top_ != top) {
            loadRowNames();
        }
        break;
    }
    case Screen::ProfileItem:
        moveCursor(steps, 5);
        break;
    case Screen::Edit: {
        const settings::Def d = settings::def(editId_);
        int v = editValue_;
        if (d.type == settings::Type::Bool) {
            v = (steps % 2 != 0) ? !v : v;
        } else {
            v += steps * (coarse && d.type == settings::Type::Int ? 10 : 1);
            v = v < d.min ? d.min : (v > d.max ? d.max : v);
        }
        editValue_ = static_cast<int16_t>(v);
        if (!d.risky()) {
            ctl_.set(editId_, editValue_, core::Origin::Oled, nowMs);
        }
        break;
    }
    case Screen::Revert:
        revertChoiceKeep_ = !revertChoiceKeep_;
        break;
    default:
        break;
    }
}

void Menu::press(uint32_t nowMs) {
    lastInputMs_ = nowMs;
    switch (screen_) {
    case Screen::Home:
        openRoot();
        break;
    case Screen::Root:
        switch (kRoot[cursor_].entry) {
        case RootEntry::Group:
            openGroup(kRoot[cursor_].group);
            break;
        case RootEntry::Profiles:
            profilesFromHome_ = false;
            openProfiles(ctl_.activeProfile());
            break;
        case RootEntry::Network:
            screen_ = Screen::Network;
            break;
        case RootEntry::Status:
            screen_ = Screen::Status;
            break;
        case RootEntry::Back:
            screen_ = Screen::Home;
            break;
        }
        break;
    case Screen::Profiles:
        pressProfiles(nowMs);
        break;
    case Screen::ProfileItem:
        pressProfileItem(nowMs);
        break;
    case Screen::ProfileConfirm: {
        bool ok = false;
        if (profileAct_ == ProfileAct::Save) {
            ok = ctl_.saveProfile(slot_, "", core::Controller::SaveSource::Keep);
        } else {
            ok = ctl_.deleteProfile(slot_);
        }
        profileDone(profileAct_, ok,
                    profileAct_ == ProfileAct::Save ? i18n::Msg::ProfilesSaved
                                                    : i18n::Msg::ProfilesDeleted);
        break;
    }
    case Screen::ProfileDone:
        if (profileAct_ == ProfileAct::Load && profileMsg_ == i18n::Msg::ProfilesLoaded) {
            screen_ = Screen::Home; // to the picture's figures
        } else {
            openProfiles(slot_);
        }
        break;
    case Screen::Group:
        activate(items_[cursor_], nowMs);
        break;
    case Screen::Edit:
        if (settings::def(editId_).risky()) {
            ctl_.set(editId_, editValue_, core::Origin::Oled, nowMs);
        }
        screen_ = Screen::Group;
        if (editId_ == Id::SystemShowAdvanced) {
            // The list itself shows more or fewer settings now; stay on this one.
            openGroup(group_);
            for (uint8_t i = 0; i < itemCount_; ++i) {
                if (items_[i].kind == Kind::Setting && items_[i].id == editId_) {
                    moveCursor(i, itemCount_);
                }
            }
        }
        break;
    case Screen::Confirm:
        if (pending_ == Kind::FactoryReset) {
            ctl_.factoryReset(nowMs);
        } else {
            rebootRequested_ = true;
        }
        actionStarted_ = true;
        screen_ = Screen::Action;
        break;
    case Screen::Action:
        screen_ = Screen::Group;
        break;
    case Screen::Network:
    case Screen::Status:
        screen_ = Screen::Root;
        break;
    case Screen::Revert:
        if (revertChoiceKeep_) {
            ctl_.keep();
        } else {
            ctl_.revert();
        }
        screen_ = beforeRevert_ == Screen::Revert ? Screen::Home : beforeRevert_;
        break;
    }
}

void Menu::longPress(uint32_t nowMs) {
    lastInputMs_ = nowMs;
    switch (screen_) {
    case Screen::Edit:
        if (!settings::def(editId_).risky() && editValue_ != editOriginal_) {
            ctl_.set(editId_, editOriginal_, core::Origin::Oled, nowMs); // back out the change
        }
        screen_ = Screen::Group;
        break;
    case Screen::Group:
        leaveToRoot(rootIndex(RootEntry::Group, group_));
        break;
    case Screen::Network:
    case Screen::Status:
        leaveToRoot(rootIndex(screen_ == Screen::Network ? RootEntry::Network : RootEntry::Status));
        break;
    case Screen::Confirm:
    case Screen::Action:
        screen_ = Screen::Group;
        break;
    case Screen::Root:
        screen_ = Screen::Home;
        break;
    case Screen::Home:
        profilesFromHome_ = true;
        openProfiles(ctl_.activeProfile());
        break;
    case Screen::Profiles:
        if (profilesFromHome_) {
            screen_ = Screen::Home;
        } else {
            leaveToRoot(rootIndex(RootEntry::Profiles));
        }
        break;
    case Screen::ProfileItem:
    case Screen::ProfileDone:
        openProfiles(slot_);
        break;
    case Screen::ProfileConfirm:
        openProfileItem(slot_);
        cursor_ = profileAct_ == ProfileAct::Save ? 1 : 3; // back on the action
        break;
    default:
        break;
    }
}

// ------------------------------------------------------------------------------------------------
// Profiles

void Menu::openProfiles(uint8_t cursorSlot) {
    rowCount_ = 0;
    bool free = false;
    uint8_t at = 0;
    for (uint8_t slot = 1; slot <= core::kProfileSlots; ++slot) {
        if (ctl_.profiles().exists(slot)) {
            at = slot == cursorSlot ? rowCount_ : at;
            rows_[rowCount_++] = slot;
        } else {
            free = true;
        }
    }
    if (free) {
        rows_[rowCount_++] = kRowNew;
    }
    rows_[rowCount_++] = kRowBack;
    screen_ = Screen::Profiles;
    cursor_ = top_ = 0;
    moveCursor(at, rowCount_);
    loadRowNames();
}

void Menu::loadRowNames() {
    for (uint8_t row = 0; row < kListRows; ++row) {
        const uint8_t i = static_cast<uint8_t>(top_ + row);
        core::Profile p;
        rowNames_[row][0] = '\0';
        if (i < rowCount_ && rows_[i] <= core::kProfileSlots && ctl_.profiles().read(rows_[i], p)) {
            memcpy(rowNames_[row], p.name, sizeof rowNames_[row]);
        }
    }
}

void Menu::openProfileItem(uint8_t slot) {
    slot_ = slot;
    core::Profile p;
    slotName_[0] = '\0';
    if (ctl_.profiles().read(slot, p)) {
        memcpy(slotName_, p.name, sizeof slotName_);
    }
    screen_ = Screen::ProfileItem;
    cursor_ = top_ = 0;
}

void Menu::profileDone(ProfileAct act, bool ok, i18n::Msg okMsg) {
    profileAct_ = act;
    profileMsg_ = ok ? okMsg : i18n::Msg::ProfilesFailed;
    screen_ = Screen::ProfileDone;
}

void Menu::pressProfiles(uint32_t nowMs) {
    const uint8_t row = rows_[cursor_];
    if (row == kRowBack) {
        longPress(nowMs);
        return;
    }
    if (row != kRowNew) {
        openProfileItem(row);
        return;
    }
    uint8_t slot = 1;
    while (slot <= core::kProfileSlots && ctl_.profiles().exists(slot)) {
        ++slot;
    }
    slot_ = slot;
    const bool ok = slot <= core::kProfileSlots &&
                    ctl_.saveProfile(slot, "", core::Controller::SaveSource::None);
    profileDone(ProfileAct::New, ok, i18n::Msg::ProfilesSaved);
    if (slot > core::kProfileSlots) {
        profileMsg_ = i18n::Msg::ProfilesFull;
    }
}

void Menu::pressProfileItem(uint32_t nowMs) {
    switch (cursor_) {
    case 0: {
        const auto r = ctl_.loadProfile(slot_, core::Controller::LoadKind::Oled, nowMs);
        profileDone(ProfileAct::Load, r == core::Controller::ProfileResult::Loaded,
                    i18n::Msg::ProfilesLoaded);
        break;
    }
    case 1:
        profileAct_ = ProfileAct::Save;
        screen_ = Screen::ProfileConfirm;
        break;
    case 2: {
        const bool on = !ctl_.profiles().signature(slot_).valid;
        if (!ctl_.setProfileSource(slot_, on)) {
            profileDone(ProfileAct::Save, false, i18n::Msg::ProfilesFailed);
            if (on && !ctl_.currentSource().valid) {
                profileMsg_ = i18n::Msg::ProfilesNoSource;
            }
        }
        break;
    }
    case 3:
        profileAct_ = ProfileAct::Delete;
        screen_ = Screen::ProfileConfirm;
        break;
    default:
        openProfiles(slot_);
        break;
    }
}

void Menu::tick(uint32_t nowMs) {
    if (ctl_.revertPending() && screen_ != Screen::Revert) {
        beforeRevert_ = screen_ == Screen::Edit ? Screen::Group : screen_;
        screen_ = Screen::Revert;
        revertChoiceKeep_ = true;
        lastInputMs_ = nowMs;
        return;
    }
    if (screen_ == Screen::Revert && !ctl_.revertPending()) {
        screen_ = Screen::Home; // answered elsewhere, or timed out
        return;
    }
    if (screen_ != Screen::Home && screen_ != Screen::Revert &&
        nowMs - lastInputMs_ >= kHomeAfterMs) {
        screen_ = Screen::Home;
    }
}

// ------------------------------------------------------------------------------------------------
// Rendering

void Menu::render(Frame& f, uint32_t nowMs) const {
    f = Frame{};
    switch (screen_) {
    case Screen::Home:
        renderHome(f);
        break;
    case Screen::Root:
    case Screen::Group:
        renderList(f);
        break;
    case Screen::Edit:
        renderEdit(f);
        break;
    case Screen::Network:
        renderNetwork(f);
        break;
    case Screen::Status:
        renderStatus(f);
        break;
    case Screen::Confirm:
        renderConfirm(f);
        break;
    case Screen::Action:
        renderAction(f);
        break;
    case Screen::Revert:
        renderRevert(f, nowMs);
        break;
    case Screen::Profiles:
        renderProfiles(f);
        break;
    case Screen::ProfileItem:
        renderProfileItem(f);
        break;
    case Screen::ProfileConfirm:
    case Screen::ProfileDone:
        renderProfileMessage(f);
        break;
    }
}

void Menu::renderHome(Frame& f) const {
    char a[64];
    char b[48];
    const core::StatusModel& s = status_;
    const uint8_t h = static_cast<uint8_t>(s.headline); // Headline order = Msg Headline* order
    msg(static_cast<i18n::Msg>(static_cast<uint8_t>(i18n::Msg::HeadlineScalerMissing) + h), a,
        sizeof a);
    if (s.headline == video::Headline::Scaling) {
        setLine(f, 0, a);
        valueText(Id::OutputResolution, static_cast<int16_t>(s.mode), b, sizeof b);
        char line[64];
        snprintf(line, sizeof line, "%s > %s", core::standardText(s.standard, s.interlaced), b);
        setLine(f, 1, line);
    } else {
        wrapTwo(f, 0, a);
    }
    if (s.sync != video::SyncType::None) {
        const unsigned long centiHz = (s.vMilliHz + 5) / 10;
        snprintf(a, sizeof a, "%s %s %lu.%02lu Hz",
                 s.port == video::InputPort::Component ? "YPbPr" : "RGB",
                 s.sync == video::SyncType::Csync ? "csync" : "HV", centiHz / 100, centiHz % 100);
        setLine(f, 2, a);
    }
    if (s.pixelClockHz != 0) {
        msg(s.frameLock == core::FrameLockState::Locked ? i18n::Msg::StatusLockOn
                                                        : i18n::Msg::StatusLockOff,
            b, sizeof b);
        snprintf(a, sizeof a, "%lu.%02lu MHz %s",
                 static_cast<unsigned long>(s.pixelClockHz / 1'000'000),
                 static_cast<unsigned long>(s.pixelClockHz / 10'000 % 100), b);
        setLine(f, 3, a);
    }
    if (net_.mode == NetView::Mode::Joined) {
        snprintf(a, sizeof a, "%u.%u.%u.%u", static_cast<unsigned>(net_.ip & 0xFF),
                 static_cast<unsigned>(net_.ip >> 8 & 0xFF),
                 static_cast<unsigned>(net_.ip >> 16 & 0xFF), static_cast<unsigned>(net_.ip >> 24));
        setLine(f, 4, a);
    } else if (net_.mode == NetView::Mode::Setup) {
        snprintf(a, sizeof a, "Wi-Fi %s", net_.apSsid);
        setLine(f, 4, a);
    } else {
        msg(i18n::Msg::NetJoining, a, sizeof a);
        setLine(f, 4, a);
    }
    if (!s.safeMode && s.profile != 0) {
        // The profile in use; "*" as editors mark unsaved changes.
        snprintf(a, sizeof a, "%u %s%s", s.profile, s.profileName, s.profileModified ? "*" : "");
    } else {
        msg(s.safeMode ? i18n::Msg::SafeMode : i18n::Msg::MenuOpen, a, sizeof a);
    }
    setLine(f, 5, a);
    if (s.safeMode) {
        f.inverted |= 1u << 5;
    }
}

void Menu::renderList(Frame& f) const {
    const i18n::LocaleTables t = tables(locale());
    const bool root = screen_ == Screen::Root;
    const uint8_t count = root ? kRootItems : itemCount_;
    char label[48];
    char value[48];
    for (uint8_t row = 0; row < kRows && top_ + row < count; ++row) {
        const uint8_t i = static_cast<uint8_t>(top_ + row);
        if (root) {
            switch (kRoot[i].entry) {
            case RootEntry::Group:
                copyText(t.text, pgm_read_word(&t.group[kRoot[i].group]), label, sizeof label);
                break;
            case RootEntry::Profiles:
                msg(i18n::Msg::MenuProfiles, label, sizeof label);
                break;
            case RootEntry::Network:
                msg(i18n::Msg::MenuNetwork, label, sizeof label);
                break;
            case RootEntry::Status:
                msg(i18n::Msg::MenuStatus, label, sizeof label);
                break;
            case RootEntry::Back:
                msg(i18n::Msg::MenuBack, label, sizeof label);
                break;
            }
            setLine(f, row, label);
        } else {
            const Item& it = items_[i];
            switch (it.kind) {
            case Kind::Setting:
                copyText(t.text, pgm_read_word(&t.label[static_cast<uint8_t>(it.id)]), label,
                         sizeof label);
                valueText(it.id, ctl_.get(it.id), value, sizeof value);
                twoColumns(f, row, label, value);
                break;
            case Kind::Calibrate:
                msg(i18n::Msg::Calibrate, label, sizeof label);
                setLine(f, row, label);
                break;
            case Kind::Reboot:
                msg(i18n::Msg::Reboot, label, sizeof label);
                setLine(f, row, label);
                break;
            case Kind::FactoryReset:
                msg(i18n::Msg::FactoryReset, label, sizeof label);
                setLine(f, row, label);
                break;
            case Kind::Back:
                msg(i18n::Msg::MenuBack, label, sizeof label);
                setLine(f, row, label);
                break;
            }
        }
        if (i == cursor_) {
            f.inverted |= static_cast<uint8_t>(1u << row);
        }
    }
    // Bottom line: the hint of the highlighted setting, or how to go back.
    if (!root && items_[cursor_].kind == Kind::Setting) {
        copyText(t.text, pgm_read_word(&t.hint[static_cast<uint8_t>(items_[cursor_].id)]), label,
                 sizeof label);
    } else if (!root && items_[cursor_].kind == Kind::Calibrate) {
        msg(i18n::Msg::CalibrateShow, label, sizeof label);
    } else {
        msg(i18n::Msg::MenuHintBack, label, sizeof label);
    }
    setLine(f, 5, label);
}

void Menu::renderEdit(Frame& f) const {
    const i18n::LocaleTables t = tables(locale());
    char text[64];
    copyText(t.text, pgm_read_word(&t.label[static_cast<uint8_t>(editId_)]), text, sizeof text);
    setLine(f, 0, text);
    f.inverted |= 1u;
    char value[48];
    valueText(editId_, editValue_, value, sizeof value);
    snprintf(text, sizeof text, "< %s >", value);
    centered(f, 2, text);
    if (editId_ == Id::InputSyncThreshold || editId_ == Id::InputPhase ||
        editId_ == Id::InputVideoLpf) {
        // Live feedback while sampling settings change (docs/design/ux.md).
        char label[24];
        msg(i18n::Msg::EditSync, label, sizeof label);
        if (status_.syncQuality <= 100) {
            snprintf(text, sizeof text, "%s %u%%", label, status_.syncQuality);
        } else {
            snprintf(text, sizeof text, "%s --", label);
        }
        centered(f, 4, text);
    }
    copyText(t.text, pgm_read_word(&t.hint[static_cast<uint8_t>(editId_)]), text, sizeof text);
    setLine(f, 5, text);
}

void Menu::renderNetwork(Frame& f) const {
    char text[64];
    switch (net_.mode) {
    case NetView::Mode::Joined:
        msg(i18n::Msg::NetJoined, text, sizeof text);
        setLine(f, 0, text);
        setLine(f, 1, net_.ssid);
        snprintf(text, sizeof text, "%u.%u.%u.%u", static_cast<unsigned>(net_.ip & 0xFF),
                 static_cast<unsigned>(net_.ip >> 8 & 0xFF),
                 static_cast<unsigned>(net_.ip >> 16 & 0xFF), static_cast<unsigned>(net_.ip >> 24));
        setLine(f, 2, text);
        setLine(f, 3, "http://gbs.local");
        break;
    case NetView::Mode::Joining:
        msg(i18n::Msg::NetJoining, text, sizeof text);
        setLine(f, 0, text);
        setLine(f, 1, net_.ssid);
        break;
    case NetView::Mode::Setup:
        msg(i18n::Msg::NetSetup, text, sizeof text);
        setLine(f, 0, text);
        setLine(f, 1, net_.apSsid);
        msg(i18n::Msg::NetPassword, text, sizeof text);
        setLine(f, 2, text);
        setLine(f, 3, net_.apPassword);
        if (net_.failure >= 1 && net_.failure <= 3) {
            msg(net_.failure == 1   ? i18n::Msg::NetRefused
                : net_.failure == 2 ? i18n::Msg::NetNotFound
                                    : i18n::Msg::NetNoAnswer,
                text, sizeof text);
            setLine(f, 4, text);
        } else {
            setLine(f, 4, "http://192.168.4.1");
        }
        break;
    }
    msg(i18n::Msg::MenuHintBack, text, sizeof text);
    setLine(f, 5, text);
}

void Menu::renderStatus(Frame& f) const {
    const core::StatusModel& s = status_;
    char text[64];
    char word[40];
    snprintf(text, sizeof text, "%s %u", core::standardText(s.standard, s.interlaced), s.lines);
    setLine(f, 0, text);
    snprintf(text, sizeof text, "H %lu.%02lu kHz V %lu.%02lu",
             static_cast<unsigned long>(s.hHz / 1000),
             static_cast<unsigned long>(s.hHz % 1000 / 10),
             static_cast<unsigned long>(s.vMilliHz / 1000),
             static_cast<unsigned long>(s.vMilliHz % 1000 / 10));
    setLine(f, 1, text);
    valueText(Id::OutputResolution, static_cast<int16_t>(s.mode), word, sizeof word);
    snprintf(text, sizeof text, "> %s %lu.%02lu MHz", word,
             static_cast<unsigned long>(s.pixelClockHz / 1'000'000),
             static_cast<unsigned long>(s.pixelClockHz / 10'000 % 100));
    setLine(f, 2, text);
    msg(s.frameLock == core::FrameLockState::Locked ? i18n::Msg::StatusLockOn
                                                    : i18n::Msg::StatusLockOff,
        word, sizeof word);
    snprintf(text, sizeof text, "%lu.%03lu Hz %s", static_cast<unsigned long>(s.rateMilliHz / 1000),
             static_cast<unsigned long>(s.rateMilliHz % 1000), word);
    setLine(f, 3, text);
    msg(i18n::Msg::StatusVersion, word, sizeof word);
    snprintf(text, sizeof text, "%s %s", word, GBSR_VERSION);
    setLine(f, 4, text);
    msg(i18n::Msg::StatusHeap, word, sizeof word);
    snprintf(text, sizeof text, "%s %lu KB", word,
             static_cast<unsigned long>(s.board.heapFree / 1024));
    setLine(f, 5, text);
}

void Menu::renderConfirm(Frame& f) const {
    char text[64];
    msg(pending_ == Kind::FactoryReset ? i18n::Msg::FactoryReset : i18n::Msg::Reboot, text,
        sizeof text);
    setLine(f, 0, text);
    f.inverted |= 1u;
    msg(i18n::Msg::Confirm, text, sizeof text);
    setLine(f, 2, text);
    msg(i18n::Msg::MenuHintBack, text, sizeof text);
    setLine(f, 5, text);
}

void Menu::renderAction(Frame& f) const {
    char text[64];
    using Outcome = video::GainCalibrator::Outcome;
    if (pending_ == Kind::Calibrate) {
        msg(i18n::Msg::Calibrate, text, sizeof text);
        setLine(f, 0, text);
        i18n::Msg m = i18n::Msg::CalibrateNoPicture;
        if (actionStarted_) {
            switch (static_cast<Outcome>(status_.levelCalibration)) {
            case Outcome::Running:
                m = i18n::Msg::CalibrateRunning;
                break;
            case Outcome::Done:
                m = i18n::Msg::CalibrateDone;
                break;
            case Outcome::NotWhite:
                m = i18n::Msg::CalibrateNotWhite;
                break;
            case Outcome::TooHot:
                m = i18n::Msg::CalibrateTooHot;
                break;
            default:
                m = i18n::Msg::CalibrateNoPicture;
                break;
            }
        }
        msg(m, text, sizeof text);
        setLine(f, 2, text);
    } else {
        msg(pending_ == Kind::FactoryReset ? i18n::Msg::FactoryReset : i18n::Msg::Reboot, text,
            sizeof text);
        setLine(f, 0, text);
        msg(i18n::Msg::Done, text, sizeof text);
        setLine(f, 2, text);
    }
    f.inverted |= 1u;
    msg(i18n::Msg::MenuHintBack, text, sizeof text);
    setLine(f, 5, text);
}

void Menu::slotTitle(char* out, size_t cap) const {
    snprintf(out, cap, "%u %s", slot_, slotName_);
}

void Menu::renderProfiles(Frame& f) const {
    char text[64];
    for (uint8_t row = 0; row < kRows && top_ + row < rowCount_; ++row) {
        const uint8_t i = static_cast<uint8_t>(top_ + row);
        if (rows_[i] == kRowNew || rows_[i] == kRowBack) {
            msg(rows_[i] == kRowNew ? i18n::Msg::ProfilesNew : i18n::Msg::MenuBack, text,
                sizeof text);
            setLine(f, row, text);
        } else {
            snprintf(text, sizeof text, "%u %s", rows_[i], rowNames_[row]);
            // "«" marks the profile in use.
            twoColumns(f, row, text, rows_[i] == ctl_.activeProfile() ? "\xC2\xAB" : "");
        }
        if (i == cursor_) {
            f.inverted |= static_cast<uint8_t>(1u << row);
        }
    }
    // Bottom line: what the highlighted profile answers to, or how to go back.
    const uint8_t slot = rows_[cursor_];
    if (slot > core::kProfileSlots) {
        msg(i18n::Msg::MenuHintBack, text, sizeof text);
    } else if (slot == ctl_.activeProfile() && ctl_.profileModified()) {
        msg(i18n::Msg::ProfilesModified, text, sizeof text);
    } else if (ctl_.profiles().signature(slot).valid) {
        const core::InputSignature& s = ctl_.profiles().signature(slot);
        snprintf(text, sizeof text, "%s %s %u",
                 s.port == static_cast<uint8_t>(video::InputPort::Component) ? "YPbPr" : "RGB",
                 core::standardFamily(static_cast<video::Standard>(s.standard)), s.lines);
    } else {
        msg(i18n::Msg::ProfilesManual, text, sizeof text);
    }
    setLine(f, 5, text);
}

void Menu::renderProfileItem(Frame& f) const {
    static constexpr i18n::Msg kItems[] = {i18n::Msg::ProfilesLoad, i18n::Msg::ProfilesSave,
                                           i18n::Msg::ProfilesAuto, i18n::Msg::ProfilesDelete,
                                           i18n::Msg::MenuBack};
    char text[64];
    char value[24];
    for (uint8_t row = 0; row < 5; ++row) {
        msg(kItems[row], text, sizeof text);
        if (row == 2) {
            msg(ctl_.profiles().signature(slot_).valid ? i18n::Msg::On : i18n::Msg::Off, value,
                sizeof value);
            twoColumns(f, row, text, value);
        } else {
            setLine(f, row, text);
        }
    }
    f.inverted |= static_cast<uint8_t>(1u << cursor_);
    slotTitle(text, sizeof text);
    setLine(f, 5, text);
}

void Menu::renderProfileMessage(Frame& f) const {
    char text[64];
    slotTitle(text, sizeof text);
    setLine(f, 0, text);
    f.inverted |= 1u;
    if (screen_ == Screen::ProfileConfirm) {
        msg(profileAct_ == ProfileAct::Save ? i18n::Msg::ProfilesSave : i18n::Msg::ProfilesDelete,
            text, sizeof text);
        setLine(f, 1, text);
        msg(i18n::Msg::Confirm, text, sizeof text);
    } else {
        msg(profileMsg_, text, sizeof text);
    }
    setLine(f, 2, text);
    msg(i18n::Msg::MenuHintBack, text, sizeof text);
    setLine(f, 5, text);
}

void Menu::renderRevert(Frame& f, uint32_t nowMs) const {
    char text[64];
    char fmt[48];
    msg(i18n::Msg::RevertAsk, text, sizeof text);
    setLine(f, 0, text);
    msg(i18n::Msg::RevertIn, fmt, sizeof fmt);
    // "{s}" in the message is the seconds left.
    const unsigned long left = (ctl_.revertRemainingMs(nowMs) + 999) / 1000;
    char* at = strstr(fmt, "{s}");
    if (at != nullptr) {
        *at = '\0';
        snprintf(text, sizeof text, "%s%lu%s", fmt, left, at + 3);
    } else {
        snprintf(text, sizeof text, "%s", fmt);
    }
    setLine(f, 1, text);
    msg(i18n::Msg::Keep, text, sizeof text);
    centered(f, 3, text);
    msg(i18n::Msg::Revert, text, sizeof text);
    centered(f, 4, text);
    f.inverted |= static_cast<uint8_t>(1u << (revertChoiceKeep_ ? 3 : 4));
}

} // namespace gbsr::ui
