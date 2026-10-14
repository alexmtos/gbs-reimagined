#include "core/controller.h"

#include "core/record.h"

namespace gbsr::core {

using settings::Id;

namespace {

constexpr char kLevelsPath[] = "/levels.bin";
constexpr uint32_t kLevelsMagic = 0x4C524247; // "GBRL"
constexpr uint8_t kLevelsVersion = 1;
constexpr uint32_t kRevertWindowMs[3] = {10'000, 20'000, 30'000}; // system.revertSeconds
constexpr int16_t kSafeOutput = static_cast<int16_t>(video::OutputMode::R480p);

} // namespace

Controller::Controller(platform::FileStore& fs, video::VideoControl& video)
    : fs_(fs), video_(video), store_(fs) {}

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
    const bool guard = d.risky() && (origin == Origin::Web || origin == Origin::Console);
    if (guard) {
        if (!revertArmed_) {
            for (uint8_t i = 0; i < settings::kCount; ++i) {
                snapshot_[i] = store_.get(static_cast<Id>(i));
            }
            snapshot_[static_cast<uint8_t>(id)] = previous;
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
}

void Controller::revert() {
    if (!revertArmed_) {
        return;
    }
    revertArmed_ = false;
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        const Id id = static_cast<Id>(i);
        if (settings::def(id).risky() && store_.get(id) != snapshot_[i]) {
            store_.set(id, snapshot_[i], 0);
            apply(id);
        }
    }
}

void Controller::factoryReset(uint32_t nowMs) {
    revertArmed_ = false;
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
        applyGeometry();
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
    video_.setGeometry(adj);
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
