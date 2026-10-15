#include "core/controller.h"

#include <stdio.h>
#include <string.h>

#include "core/record.h"

namespace gbsr::core {

using settings::Id;

namespace {

constexpr char kLevelsPath[] = "/levels.bin";
constexpr uint32_t kLevelsMagic = 0x4C524247; // "GBRL"
constexpr uint8_t kLevelsVersion = 1;
constexpr uint32_t kRevertWindowMs[3] = {10'000, 20'000, 30'000}; // system.revertSeconds
constexpr int16_t kSafeOutput = static_cast<int16_t>(video::OutputMode::R480p);
// system.startupProfile: -1 last used, 0 auto by input, 1..16 that profile.
constexpr int16_t kStartupAuto = 0;
// How long a manual or switcher load owns the next new source. A design choice, not a
// measurement: a switch, the engine's detection and its settling fit well inside it.
constexpr uint32_t kLoadHoldMs = 10'000;

// The same console, as far as auto-load is concerned: line counts within two (480i alternates).
bool sameSource(const InputSignature& a, const InputSignature& b) {
    if (a.valid != b.valid) {
        return false;
    }
    return !a.valid || (a.port == b.port && a.sync == b.sync && a.standard == b.standard &&
                        a.lines + 2 >= b.lines && b.lines + 2 >= a.lines);
}

} // namespace

Controller::Controller(platform::FileStore& fs, video::VideoControl& video)
    : fs_(fs), video_(video), store_(fs), profiles_(fs) {}

void Controller::begin(bool safeMode, uint32_t nowMs) {
    safeMode_ = safeMode;
    if (!safeMode_) {
        store_.load();
        loadLevels();
    } else {
        store_.set(Id::OutputResolution, kSafeOutput, nowMs);
    }
    savedLevelsSequence_ = video_.levelsSequence();
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        apply(static_cast<Id>(i));
    }
    profiles_.begin();
    if (safeMode_) {
        return; // saved profiles are left alone too
    }
    const int16_t startup = store_.get(Id::SystemStartupProfile);
    if (startup >= 1) {
        loadProfile(static_cast<uint8_t>(startup), LoadKind::Startup, nowMs);
    }
    startupHold_ = startup != kStartupAuto && profiles_.active() != 0;
}

void Controller::tick(uint32_t nowMs) {
    if (revertArmed_ && static_cast<int32_t>(nowMs - revertDeadlineMs_) >= 0) {
        revert();
    }
    if (safeMode_) {
        return; // nothing is saved in safe mode
    }
    if (!revertArmed_) {
        // An unconfirmed risky value must not survive a power cut: saves wait for the answer.
        store_.tick(nowMs);
    }
    if (video_.levelsSequence() != savedLevelsSequence_) {
        saveLevels();
    }
}

Controller::Outcome Controller::set(Id id, int16_t value, Origin origin, uint32_t nowMs) {
    if (!settings::available(id)) {
        return Outcome::Unavailable;
    }
    const settings::Def d = settings::def(id);
    const int16_t previous = store_.get(id);
    switch (store_.set(id, value, nowMs)) {
    case settings::Store::SetResult::OutOfRange:
        return Outcome::Invalid;
    case settings::Store::SetResult::Unchanged:
        return Outcome::Unchanged;
    case settings::Store::SetResult::Changed:
        break;
    }
    if (origin != Origin::Startup && profiles_.active() != 0 && inProfile(id)) {
        // Inside a load's revert window the flag is written when the load is kept.
        profiles_.setActive(profiles_.active(), true, !revertAll_);
    }
    const bool guard = d.risky() && (origin == Origin::Web || origin == Origin::Console);
    if (guard) {
        if (!revertArmed_) {
            for (uint8_t i = 0; i < settings::kCount; ++i) {
                snapshot_[i] = store_.get(static_cast<Id>(i));
            }
            snapshot_[static_cast<uint8_t>(id)] = previous;
            revertActive_ = profiles_.active();
        }
        revertArmed_ = true;
        revertDeadlineMs_ = nowMs + revertWindowMs();
    }
    apply(id);
    return guard ? Outcome::RevertArmed : Outcome::Applied;
}

uint32_t Controller::revertRemainingMs(uint32_t nowMs) const {
    if (!revertArmed_ || static_cast<int32_t>(revertDeadlineMs_ - nowMs) <= 0) {
        return 0;
    }
    return revertDeadlineMs_ - nowMs;
}

void Controller::keep() {
    revertArmed_ = false;
    if (revertAll_) {
        revertAll_ = false;
        profiles_.persistActive(); // the load is confirmed
    }
}

void Controller::revert() {
    if (!revertArmed_) {
        return;
    }
    revertArmed_ = false;
    const bool all = revertAll_;
    revertAll_ = false;
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        const Id id = static_cast<Id>(i);
        if ((all || settings::def(id).risky()) && store_.get(id) != snapshot_[i]) {
            store_.set(id, snapshot_[i], 0);
            apply(id);
        }
    }
    if (all) {
        profiles_.setActive(revertActive_, false); // the profile before the load
    }
    refreshModified();
}

void Controller::refreshModified() {
    Profile p;
    if (profiles_.active() == 0 || !profiles_.read(profiles_.active(), p)) {
        return;
    }
    bool modified = false;
    for (uint8_t i = 0; i < settings::kCount && !modified; ++i) {
        const Id id = static_cast<Id>(i);
        // Auto-load leaves the port as it was, so the port alone never counts as a change.
        modified = p.has[i] && id != Id::InputPort && settings::available(id) &&
                   store_.get(id) != p.values[i];
    }
    profiles_.setActive(profiles_.active(), modified);
}

void Controller::factoryReset(uint32_t nowMs) {
    revertArmed_ = false;
    revertAll_ = false;
    profiles_.setActive(0, false);
    store_.resetToDefaults();
    store_.saveNow();
    fs_.remove(kLevelsPath);
    const uint8_t preset[3] = {0x7B, 0x7B, 0x7B};
    video_.setBaseGains(0, preset);
    video_.setBaseGains(1, preset);
    savedLevelsSequence_ = video_.levelsSequence();
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        apply(static_cast<Id>(i));
    }
    (void)nowMs;
}

Controller::ProfileResult Controller::loadProfile(uint8_t slot, LoadKind kind, uint32_t nowMs) {
    if (safeMode_) {
        return ProfileResult::Refused;
    }
    Profile p;
    if (!profiles_.read(slot, p)) {
        return ProfileResult::Missing;
    }
    if (revertArmed_ && (kind == LoadKind::Auto || kind == LoadKind::Switcher)) {
        // An unconfirmed change goes first: the new source or the switcher starts from safe ground.
        revert();
    }
    int16_t before[settings::kCount];
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        before[i] = store_.get(static_cast<Id>(i));
    }
    bool changed[settings::kCount] = {};
    bool riskyChanged = false;
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        const Id id = static_cast<Id>(i);
        if (!p.has[i] || !settings::available(id)) {
            continue;
        }
        if (kind == LoadKind::Auto && id == Id::InputPort) {
            continue; // the source it matched is already on that input; avoids ping-pong
        }
        if (store_.set(id, p.values[i], nowMs) == settings::Store::SetResult::Changed) {
            changed[i] = true;
            riskyChanged = riskyChanged || settings::def(id).risky();
        }
    }
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        if (changed[i]) {
            apply(static_cast<Id>(i));
        }
    }
    if (kind == LoadKind::Manual || kind == LoadKind::Oled || kind == LoadKind::Switcher) {
        evaluatedSource_ = currentSource_; // holds until the source changes
        loadHold_ = true;
        loadHoldUntilMs_ = nowMs + kLoadHoldMs;
    }
    const bool guard = kind == LoadKind::Manual && riskyChanged;
    if (guard) {
        if (!revertArmed_) {
            for (uint8_t i = 0; i < settings::kCount; ++i) {
                snapshot_[i] = before[i];
            }
            revertActive_ = profiles_.active();
        }
        revertArmed_ = true;
        revertAll_ = true;
        revertDeadlineMs_ = nowMs + revertWindowMs();
    }
    profiles_.setActive(slot, false, !guard);
    return guard ? ProfileResult::RevertArmed : ProfileResult::Loaded;
}

Controller::ProfileResult Controller::switcherInput(uint8_t slot, bool current, uint32_t nowMs) {
    if (current && profiles_.active() == slot && !profiles_.modified()) {
        return ProfileResult::Unchanged;
    }
    return loadProfile(slot, LoadKind::Switcher, nowMs);
}

bool Controller::validProfileName(const char* name) {
    size_t n = 0;
    for (const uint8_t* p = reinterpret_cast<const uint8_t*>(name); *p != 0; ++p, ++n) {
        if (*p >= 0x20 && *p < 0x7F) {
            continue;
        }
        // Latin-1 in UTF-8: C2 A0..BF or C3 80..BF.
        if ((p[0] == 0xC2 && p[1] >= 0xA0 && p[1] <= 0xBF) ||
            (p[0] == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF)) {
            ++p;
            ++n;
            continue;
        }
        return false;
    }
    return n < kProfileNameCap;
}

bool Controller::saveProfile(uint8_t slot, const char* name, SaveSource source) {
    if (safeMode_ || revertArmed_ || slot < 1 || slot > kProfileSlots || !validProfileName(name) ||
        (source == SaveSource::This && !currentSource_.valid)) {
        return false;
    }
    Profile old;
    const bool existed = profiles_.read(slot, old);
    Profile p;
    if (name[0] != '\0') {
        strncpy(p.name, name, kProfileNameCap - 1);
    } else if (existed) {
        memcpy(p.name, old.name, sizeof p.name);
    } else {
        defaultName(slot, p.name, sizeof p.name);
    }
    if (source == SaveSource::This) {
        p.signature = currentSource_;
    } else if (source == SaveSource::Keep && existed) {
        p.signature = old.signature;
    }
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        const Id id = static_cast<Id>(i);
        if (inProfile(id) && settings::available(id)) {
            p.values[i] = store_.get(id);
            p.has[i] = true;
        }
    }
    if (!profiles_.write(slot, p)) {
        return false;
    }
    profiles_.setActive(slot, false);
    return true;
}

bool Controller::renameProfile(uint8_t slot, const char* name) {
    Profile p;
    if (safeMode_ || !validProfileName(name) || name[0] == '\0' || !profiles_.read(slot, p)) {
        return false;
    }
    memset(p.name, 0, sizeof p.name);
    strncpy(p.name, name, kProfileNameCap - 1);
    return profiles_.write(slot, p);
}

bool Controller::setProfileSource(uint8_t slot, bool forThisSource) {
    Profile p;
    if (safeMode_ || !profiles_.read(slot, p) || (forThisSource && !currentSource_.valid)) {
        return false;
    }
    p.signature = forThisSource ? currentSource_ : InputSignature{};
    return profiles_.write(slot, p);
}

void Controller::defaultName(uint8_t slot, char* out, size_t cap) const {
    if (defaultName_ != nullptr) {
        defaultName_(slot, static_cast<uint8_t>(get(Id::SystemLanguage)), out, cap);
    } else {
        snprintf(out, cap, "Profile %u", static_cast<unsigned>(slot));
    }
}

bool Controller::importProfile(uint8_t slot, const Profile& p) {
    if (safeMode_ || !validProfileName(p.name)) {
        return false;
    }
    Profile copy = p;
    if (copy.name[0] == '\0') {
        defaultName(slot, copy.name, sizeof copy.name);
    }
    if (!profiles_.write(slot, copy)) {
        return false;
    }
    if (profiles_.active() == slot) {
        profiles_.setActive(0, false); // the settings in use no longer came from this slot
    }
    return true;
}

bool Controller::restoreLevels(const uint8_t (&component)[3], const uint8_t (&rgb)[3]) {
    if (safeMode_) {
        return false;
    }
    video_.setBaseGains(0, component);
    video_.setBaseGains(1, rgb);
    saveLevels();
    return true;
}

bool Controller::deleteProfile(uint8_t slot) {
    return !safeMode_ && profiles_.exists(slot) && profiles_.remove(slot);
}

void Controller::onSource(const InputSignature& now, uint32_t nowMs) {
    currentSource_ = now;
    evaluateSource(now, nowMs);
}

void Controller::sourceDetected(const InputSignature& source, uint32_t nowMs) {
    evaluateSource(source, nowMs);
}

void Controller::evaluateSource(const InputSignature& now, uint32_t nowMs) {
    if (!now.valid || safeMode_ || sameSource(now, evaluatedSource_)) {
        return;
    }
    evaluatedSource_ = now;
    const bool held = loadHold_ && static_cast<int32_t>(nowMs - loadHoldUntilMs_) < 0;
    const bool boot = startupHold_;
    loadHold_ = false;
    startupHold_ = false;
    if (held) {
        return;
    }
    const uint8_t best = profiles_.match(now);
    if (best == 0 || (boot && best == profiles_.active())) {
        return; // nothing fits; or the profile restored at boot, unsaved changes included
    }
    if (best != profiles_.active() || profiles_.modified()) {
        loadProfile(best, LoadKind::Auto, nowMs);
    }
}

uint32_t Controller::revertWindowMs() const {
    const int16_t v = store_.get(Id::SystemRevertSeconds);
    return kRevertWindowMs[v >= 0 && v < 3 ? v : 1];
}

void Controller::apply(Id id) {
    const int16_t v = store_.get(id);
    switch (id) {
    case Id::OutputResolution:
        video_.setOutputPreference(static_cast<video::OutputMode>(v));
        break;
    case Id::OutputFrameLock:
        video_.setFrameLock(v != 0);
        break;
    case Id::OutputForce60:
        video_.setForce60(v != 0);
        break;
    case Id::InputPort:
        video_.setInputPreference(static_cast<video::InputPreference>(v));
        break;
    case Id::InputAutoGain:
        video_.setAutoGain(v != 0);
        break;
    case Id::OutputAspect:
    case Id::OutputHPosition:
    case Id::OutputVPosition:
    case Id::OutputHSize:
    case Id::OutputVSize:
    case Id::OutputIntegerScale:
        applyGeometry();
        break;
    case Id::InputSyncThreshold:
    case Id::InputPhase:
    case Id::InputVideoLpf:
        applySampling();
        break;
    case Id::PictureScanlines:
    case Id::PictureScanlineStrength:
    case Id::PictureDeinterlace:
        applyLines();
        break;
    case Id::PictureBrightness:
    case Id::PictureContrast:
    case Id::PictureSaturation:
    case Id::PictureSharpness:
        applyPicture();
        break;
    default:
        break; // read by the front ends, or not offered yet
    }
}

void Controller::applyPicture() {
    video::PictureAdjust adj;
    adj.brightness = static_cast<int8_t>(store_.get(Id::PictureBrightness));
    adj.contrast = static_cast<int8_t>(store_.get(Id::PictureContrast));
    adj.saturation = static_cast<int8_t>(store_.get(Id::PictureSaturation));
    adj.sharpness = static_cast<uint8_t>(store_.get(Id::PictureSharpness));
    video_.setPicture(adj);
}

void Controller::applyGeometry() {
    video::GeometryAdjust adj;
    adj.aspect = static_cast<video::Aspect>(store_.get(Id::OutputAspect));
    adj.hPosition = static_cast<int8_t>(store_.get(Id::OutputHPosition));
    adj.vPosition = static_cast<int8_t>(store_.get(Id::OutputVPosition));
    adj.hSize = static_cast<int8_t>(store_.get(Id::OutputHSize));
    adj.vSize = static_cast<int8_t>(store_.get(Id::OutputVSize));
    adj.integer = store_.get(Id::OutputIntegerScale) != 0;
    video_.setGeometry(adj);
}

void Controller::applyLines() {
    video::LineAdjust adj;
    adj.scanlines = store_.get(Id::PictureScanlines) != 0;
    adj.strength = static_cast<uint8_t>(store_.get(Id::PictureScanlineStrength));
    adj.bob = store_.get(Id::PictureDeinterlace) == 1;
    video_.setLines(adj);
}

void Controller::applySampling() {
    video::SamplingAdjust adj;
    adj.sog = static_cast<int8_t>(store_.get(Id::InputSyncThreshold));
    adj.phase = static_cast<int8_t>(store_.get(Id::InputPhase));
    adj.lpf = static_cast<uint8_t>(store_.get(Id::InputVideoLpf));
    video_.setSampling(adj);
}

// /levels.bin: the base gains of both inputs (component, then RGB), three bytes each.
void Controller::loadLevels() {
    uint8_t buf[6];
    if (loadRecord(fs_, kLevelsPath, kLevelsMagic, kLevelsVersion, buf, sizeof buf) !=
        static_cast<int>(sizeof buf)) {
        return;
    }
    for (uint8_t port = 0; port < 2; ++port) {
        const uint8_t gains[3] = {buf[port * 3], buf[port * 3 + 1], buf[port * 3 + 2]};
        video_.setBaseGains(port, gains);
    }
}

void Controller::saveLevels() {
    uint8_t buf[6];
    for (uint8_t port = 0; port < 2; ++port) {
        uint8_t gains[3];
        video_.baseGains(port, gains);
        for (uint8_t c = 0; c < 3; ++c) {
            buf[port * 3 + c] = gains[c];
        }
    }
    if (saveRecord(fs_, kLevelsPath, kLevelsMagic, kLevelsVersion, buf, sizeof buf)) {
        savedLevelsSequence_ = video_.levelsSequence();
    }
}

} // namespace gbsr::core
