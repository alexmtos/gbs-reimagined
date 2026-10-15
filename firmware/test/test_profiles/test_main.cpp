#include <string.h>

#include "../support/doctest_main.h"
#include "../support/fake_storage.h"
#include "../support/fake_video.h"
#include "core/controller.h"
#include "core/profiles.h"

using namespace gbsr;
using core::Controller;
using core::InputSignature;
using core::Origin;
using core::Profile;
using core::ProfileStore;
using settings::Id;
using test::FakeVideo;
using Kind = Controller::LoadKind;
using Result = Controller::ProfileResult;
using Save = Controller::SaveSource;

namespace {

InputSignature sig(uint8_t port, uint8_t sync, uint8_t standard, uint16_t lines) {
    InputSignature s;
    s.valid = true;
    s.port = port;
    s.sync = sync;
    s.standard = standard;
    s.lines = lines;
    return s;
}

// RGB csync 240p, RGB csync 288p, component 480i.
const InputSignature kSnes = sig(1, 2, 1, 262);
const InputSignature kPal = sig(1, 2, 2, 312);
const InputSignature kPs2 = sig(0, 2, 1, 263);

struct Rig {
    test::FakeFileStore fs;
    FakeVideo video;
    Controller ctl{fs, video};
};

// A profile saved with brightness `b` for `source` (none when invalid) into `slot`.
void saveWith(Rig& r, uint8_t slot, int16_t b, const InputSignature& source, uint32_t now = 0) {
    r.ctl.onSource(source, now); // may auto-load an earlier profile
    r.ctl.set(Id::PictureBrightness, b, Origin::Oled, now);
    REQUIRE(r.ctl.saveProfile(slot, "", source.valid ? Save::This : Save::None));
}

} // namespace

TEST_CASE("signatures match most specifically on port, sync, standard and lines") {
    CHECK(core::signatureMatch(kSnes, kSnes) == 3);
    CHECK(core::signatureMatch(kSnes, sig(1, 2, 1, 263)) == 3); // 480i, two lines away at most
    CHECK(core::signatureMatch(kSnes, sig(1, 2, 1, 265)) == 2); // too many lines
    CHECK(core::signatureMatch(kSnes, sig(1, 1, 1, 262)) == 2); // other sync type
    CHECK(core::signatureMatch(kSnes, kPal) == 1);              // same port only
    CHECK(core::signatureMatch(kSnes, kPs2) == 0);
    CHECK(core::signatureMatch(InputSignature{}, kSnes) == 0);
    CHECK(core::signatureMatch(kSnes, InputSignature{}) == 0);
}

TEST_CASE("switcher lines name a profile slot") {
    uint8_t slot = 0;
    bool current = true;
    CHECK(core::parseSwitcherCommand("SVS NEW INPUT=3", slot, current));
    CHECK(slot == 3);
    CHECK_FALSE(current);
    CHECK(core::parseSwitcherCommand("  svs current input=16 ", slot, current));
    CHECK(slot == 16);
    CHECK(current);
    CHECK(core::parseSwitcherCommand("remote prof12", slot, current));
    CHECK(slot == 12);
    CHECK_FALSE(current);
    CHECK_FALSE(core::parseSwitcherCommand("SVS NEW INPUT=17", slot, current));
    CHECK_FALSE(core::parseSwitcherCommand("SVS NEW INPUT=0", slot, current));
    CHECK_FALSE(core::parseSwitcherCommand("SVS NEW INPUT=", slot, current));
    CHECK_FALSE(core::parseSwitcherCommand("SVS NEW INPUT=2x", slot, current));
    CHECK_FALSE(core::parseSwitcherCommand("remote menu", slot, current));
    CHECK_FALSE(core::parseSwitcherCommand("status", slot, current));
}

TEST_CASE("only the Output, Input and Picture settings belong in a profile") {
    CHECK(core::inProfile(Id::OutputResolution));
    CHECK(core::inProfile(Id::InputPort));
    CHECK(core::inProfile(Id::PictureScanlines));
    CHECK_FALSE(core::inProfile(Id::SystemLanguage));
    CHECK_FALSE(core::inProfile(Id::SystemStartupProfile));
}

TEST_CASE("a profile file keeps its name, signature and values, and the index survives a boot") {
    test::FakeFileStore fs;
    ProfileStore store(fs);
    store.begin();
    Profile p;
    strcpy(p.name, "Super Famicom");
    p.signature = kSnes;
    p.values[static_cast<uint8_t>(Id::PictureBrightness)] = -7;
    p.has[static_cast<uint8_t>(Id::PictureBrightness)] = true;
    p.values[static_cast<uint8_t>(Id::OutputResolution)] = 3;
    p.has[static_cast<uint8_t>(Id::OutputResolution)] = true;
    p.values[static_cast<uint8_t>(Id::SystemLanguage)] = 1; // never written
    p.has[static_cast<uint8_t>(Id::SystemLanguage)] = true;
    REQUIRE(store.write(5, p));
    CHECK(fs.files.count("/profile5.bin") == 1);
    CHECK_FALSE(store.write(0, p));
    CHECK_FALSE(store.write(17, p));

    ProfileStore again(fs);
    again.begin();
    CHECK(again.exists(5));
    CHECK_FALSE(again.exists(4));
    CHECK(again.signature(5) == kSnes);
    Profile back;
    REQUIRE(again.read(5, back));
    CHECK(strcmp(back.name, "Super Famicom") == 0);
    CHECK(back.has[static_cast<uint8_t>(Id::PictureBrightness)]);
    CHECK(back.values[static_cast<uint8_t>(Id::PictureBrightness)] == -7);
    CHECK(back.values[static_cast<uint8_t>(Id::OutputResolution)] == 3);
    CHECK_FALSE(back.has[static_cast<uint8_t>(Id::SystemLanguage)]);
    CHECK_FALSE(back.has[static_cast<uint8_t>(Id::PictureContrast)]);

    // A damaged file reads as missing.
    fs.files["/profile5.bin"][10] ^= 0xFF;
    CHECK_FALSE(again.read(5, back));
}

TEST_CASE("the best match wins, the lowest slot breaks a tie, and unsigned profiles never match") {
    test::FakeFileStore fs;
    ProfileStore store(fs);
    store.begin();
    Profile p;
    REQUIRE(store.write(1, p)); // no signature
    p.signature = sig(1, 1, 3, 525);
    REQUIRE(store.write(2, p)); // RGB, other standard: port only
    p.signature = sig(1, 1, 1, 262);
    REQUIRE(store.write(3, p)); // RGB 240p, other sync: port and standard
    REQUIRE(store.write(4, p)); // the same again
    CHECK(store.match(kSnes) == 3);
    p.signature = kSnes;
    REQUIRE(store.write(6, p));
    CHECK(store.match(kSnes) == 6);
    CHECK(store.match(kPal) == 2);
    CHECK(store.match(kPs2) == 0);
    store.remove(6);
    CHECK(store.match(kSnes) == 3);
    CHECK(fs.files.count("/profile6.bin") == 0);
}

TEST_CASE("loading copies a profile into the settings; a later change marks it modified") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 1, -5, InputSignature{});
    CHECK(r.ctl.activeProfile() == 1);
    CHECK_FALSE(r.ctl.profileModified());
    Profile p;
    REQUIRE(r.ctl.profiles().read(1, p));
    CHECK(strcmp(p.name, "Profile 1") == 0);
    CHECK_FALSE(p.signature.valid);

    r.ctl.set(Id::PictureBrightness, 9, Origin::Web, 10);
    CHECK(r.ctl.profileModified());
    CHECK(r.video.picture.brightness == 9);
    r.ctl.set(Id::SystemLanguage, 1, Origin::Web, 10); // global: does not touch the profile
    REQUIRE(r.ctl.profiles().read(1, p));
    CHECK(p.values[static_cast<uint8_t>(Id::PictureBrightness)] == -5); // snapshot model

    CHECK(r.ctl.loadProfile(1, Kind::Manual, 20) == Result::Loaded);
    CHECK(r.video.picture.brightness == -5);
    CHECK_FALSE(r.ctl.profileModified());
    CHECK(r.ctl.get(Id::SystemLanguage) == 1);
    CHECK(r.ctl.loadProfile(9, Kind::Manual, 20) == Result::Missing);
}

TEST_CASE("a manual load that changes a risky setting reverts as a whole unless kept") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.set(Id::OutputResolution, 1, Origin::Oled, 0); // r1080p
    saveWith(r, 2, 4, InputSignature{});
    r.ctl.set(Id::OutputResolution, 5, Origin::Oled, 0); // r480p
    r.ctl.set(Id::PictureBrightness, -2, Origin::Oled, 0);
    REQUIRE(r.ctl.profileModified());

    CHECK(r.ctl.loadProfile(2, Kind::Manual, 100) == Result::RevertArmed);
    CHECK(r.video.output == video::OutputMode::R1080p);
    CHECK(r.video.picture.brightness == 4);
    CHECK(r.ctl.activeProfile() == 2);
    r.ctl.tick(100 + 19'999);
    CHECK(r.ctl.revertPending());
    r.ctl.tick(100 + 20'000);
    CHECK_FALSE(r.ctl.revertPending());
    CHECK(r.video.output == video::OutputMode::R480p);
    CHECK(r.video.picture.brightness == -2); // not risky, yet it returns with the rest
    CHECK(r.ctl.activeProfile() == 2);       // what was active before, modified
    CHECK(r.ctl.profileModified());

    CHECK(r.ctl.loadProfile(2, Kind::Manual, 30'000) == Result::RevertArmed);
    r.ctl.keep();
    CHECK(r.video.output == video::OutputMode::R1080p);
    CHECK_FALSE(r.ctl.profileModified());

    // A profile that changes nothing risky loads without the revert.
    r.ctl.set(Id::PictureBrightness, 3, Origin::Oled, 40'000);
    CHECK(r.ctl.loadProfile(2, Kind::Manual, 40'000) == Result::Loaded);
    CHECK_FALSE(r.ctl.revertPending());
}

TEST_CASE("the active profile is written only once a guarded load is kept") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.set(Id::OutputResolution, 1, Origin::Oled, 0);
    saveWith(r, 3, 0, InputSignature{});
    r.ctl.set(Id::OutputResolution, 5, Origin::Oled, 0);
    saveWith(r, 4, 0, InputSignature{});
    REQUIRE(r.ctl.activeProfile() == 4);
    REQUIRE(r.ctl.loadProfile(3, Kind::Manual, 0) == Result::RevertArmed);

    // A power cut now boots into profile 4: the load was never confirmed.
    Rig cut;
    cut.fs = r.fs;
    cut.ctl.begin(false, 0);
    CHECK(cut.ctl.activeProfile() == 4);

    r.ctl.keep();
    Rig kept;
    kept.fs = r.fs;
    kept.ctl.begin(false, 0);
    CHECK(kept.ctl.activeProfile() == 3);
}

TEST_CASE("a newly settled source loads its best profile, once") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 1, 6, kSnes);
    saveWith(r, 2, -6, kPal);
    r.ctl.set(Id::InputPort, 2, Origin::Oled, 0); // saved with neither profile

    r.ctl.onSource(kSnes, 1000);
    CHECK(r.ctl.activeProfile() == 1);
    CHECK(r.video.picture.brightness == 6);
    CHECK(r.ctl.get(Id::InputPort) == 2); // auto-load leaves the port as it is
    CHECK_FALSE(r.ctl.revertPending());

    // The console resets, or switches to 480i: the same source, nothing reloads.
    r.ctl.set(Id::PictureBrightness, 1, Origin::Web, 2000);
    r.ctl.onSource(InputSignature{}, 3000);
    r.ctl.onSource(sig(1, 2, 1, 263), 4000);
    CHECK(r.video.picture.brightness == 1);
    CHECK(r.ctl.profileModified());

    r.ctl.onSource(kPal, 5000);
    CHECK(r.ctl.activeProfile() == 2);
    CHECK(r.video.picture.brightness == -6);
    // Back to the first source: its profile again, unsaved changes gone.
    r.ctl.onSource(kSnes, 6000);
    CHECK(r.ctl.activeProfile() == 1);
    CHECK(r.video.picture.brightness == 6);

    // A source no profile fits leaves everything as it is.
    r.ctl.onSource(kPs2, 7000);
    CHECK(r.ctl.activeProfile() == 1);
}

TEST_CASE("a manual or switcher load owns the next source, then auto-load resumes") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 1, 6, kSnes);
    saveWith(r, 2, -6, kPal);
    saveWith(r, 3, 2, InputSignature{});

    // The switcher announces input 3 before its new source settles.
    r.ctl.onSource(kSnes, 1000);
    REQUIRE(r.ctl.activeProfile() == 1);
    CHECK(r.ctl.switcherInput(3, false, 2000) == Result::Loaded);
    r.ctl.onSource(InputSignature{}, 2500);
    r.ctl.onSource(kPal, 4000); // inside the hold: profile 3 stays
    CHECK(r.ctl.activeProfile() == 3);
    r.ctl.onSource(kSnes, 20'000); // the next change is the user's again
    CHECK(r.ctl.activeProfile() == 1);

    // A manual load holds while the source stays; the hold expires for a later change.
    CHECK(r.ctl.loadProfile(3, Kind::Manual, 30'000) == Result::Loaded);
    r.ctl.onSource(kSnes, 31'000);
    CHECK(r.ctl.activeProfile() == 3);
    r.ctl.onSource(kPal, 45'000);
    CHECK(r.ctl.activeProfile() == 2);
}

TEST_CASE("SVS CURRENT INPUT reloads only when the profile is not already in use as saved") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 1, 6, InputSignature{});
    CHECK(r.ctl.switcherInput(1, true, 0) == Result::Unchanged);
    r.ctl.set(Id::PictureBrightness, 0, Origin::Web, 0);
    CHECK(r.ctl.switcherInput(1, true, 0) == Result::Loaded);
    CHECK(r.video.picture.brightness == 6);
    // A switcher load never arms the revert, even when the output changes.
    r.ctl.set(Id::OutputResolution, 1, Origin::Oled, 0);
    CHECK(r.ctl.switcherInput(1, false, 0) == Result::Loaded);
    CHECK_FALSE(r.ctl.revertPending());
    CHECK(r.ctl.switcherInput(7, false, 0) == Result::Missing);
}

TEST_CASE("startup: last used keeps the restored profile for its own source") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 1, 6, kSnes);
    saveWith(r, 2, -6, kPal);
    r.ctl.onSource(kSnes, 1000);
    r.ctl.set(Id::PictureBrightness, 11, Origin::Oled, 2000); // unsaved
    r.ctl.tick(10'000);

    Rig same;
    same.fs = r.fs;
    same.ctl.begin(false, 0);
    CHECK(same.ctl.activeProfile() == 1);
    CHECK(same.ctl.profileModified());
    same.ctl.onSource(kSnes, 1000);
    CHECK(same.video.picture.brightness == 11); // the unsaved change survives the reboot

    Rig other;
    other.fs = r.fs;
    other.ctl.begin(false, 0);
    other.ctl.onSource(kPal, 1000); // another console: its own profile
    CHECK(other.ctl.activeProfile() == 2);
    CHECK(other.video.picture.brightness == -6);
}

TEST_CASE("startup: auto by input reloads, and a fixed profile loads at boot") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 2, -6, InputSignature{});
    saveWith(r, 1, 6, kSnes);
    r.ctl.set(Id::PictureBrightness, 11, Origin::Oled, 2000);   // profile 1, modified
    r.ctl.set(Id::SystemStartupProfile, 0, Origin::Oled, 2000); // autoByInput
    r.ctl.tick(10'000);

    Rig autoLoad;
    autoLoad.fs = r.fs;
    autoLoad.ctl.begin(false, 0);
    autoLoad.ctl.onSource(kSnes, 1000);
    CHECK(autoLoad.video.picture.brightness == 6);
    CHECK_FALSE(autoLoad.ctl.profileModified());

    r.ctl.set(Id::SystemStartupProfile, 2, Origin::Oled, 20'000);
    r.ctl.tick(30'000);
    Rig fixed;
    fixed.fs = r.fs;
    fixed.ctl.begin(false, 0);
    CHECK(fixed.ctl.activeProfile() == 2);
    CHECK(fixed.video.picture.brightness == -6);
    CHECK_FALSE(fixed.ctl.revertPending());
    CHECK(fixed.ctl.get(Id::SystemStartupProfile) == 2); // not part of any profile
}

TEST_CASE("safe mode neither loads nor writes profiles") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 1, 6, kSnes);
    r.ctl.set(Id::SystemStartupProfile, 1, Origin::Oled, 0);
    r.ctl.tick(10'000);

    Rig safe;
    safe.fs = r.fs;
    safe.ctl.begin(true, 0);
    CHECK(safe.video.picture.brightness == 0);
    safe.ctl.onSource(kSnes, 1000);
    CHECK(safe.video.picture.brightness == 0);
    CHECK(safe.ctl.loadProfile(1, Kind::Manual, 0) == Result::Refused);
    CHECK_FALSE(safe.ctl.saveProfile(2, "x", Save::Keep));
    CHECK_FALSE(safe.ctl.deleteProfile(1));
}

TEST_CASE(
    "saving needs a valid slot, a drawable name, a source when asked, and no pending revert") {
    Rig r;
    r.ctl.begin(false, 0);
    CHECK(r.ctl.saveProfile(16, "Mega Drive", Save::Keep));
    CHECK_FALSE(r.ctl.saveProfile(17, "x", Save::Keep));
    CHECK_FALSE(r.ctl.saveProfile(0, "x", Save::Keep));
    CHECK(r.ctl.saveProfile(2, "S\xC3\xA3o Paulo \xC2\xBA", Save::Keep)); // Latin-1 letters
    CHECK_FALSE(r.ctl.saveProfile(2, "\xE2\x82\xAC", Save::Keep));        // the euro sign is not
    CHECK_FALSE(r.ctl.saveProfile(2, "tab\tname", Save::Keep));
    CHECK(r.ctl.saveProfile(2, "12345678901234567890123", Save::Keep)); // 23 bytes
    CHECK_FALSE(r.ctl.saveProfile(2, "123456789012345678901234", Save::Keep));
    CHECK_FALSE(r.ctl.saveProfile(3, "x", Save::This)); // no settled source to sign with
    r.ctl.set(Id::OutputResolution, 1, Origin::Web, 0);
    CHECK_FALSE(r.ctl.saveProfile(3, "x", Save::Keep)); // confirm or revert first
}

TEST_CASE("saving over a slot keeps its name and signature unless told otherwise") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.onSource(kSnes, 0);
    REQUIRE(r.ctl.saveProfile(4, "Saturn", Save::This));
    r.ctl.set(Id::PictureBrightness, 8, Origin::Oled, 0);
    REQUIRE(r.ctl.saveProfile(4, "", Save::Keep));
    Profile p;
    REQUIRE(r.ctl.profiles().read(4, p));
    CHECK(strcmp(p.name, "Saturn") == 0);
    CHECK(p.signature == kSnes);
    CHECK(p.values[static_cast<uint8_t>(Id::PictureBrightness)] == 8);
    REQUIRE(r.ctl.saveProfile(4, "Sega Saturn", Save::None));
    REQUIRE(r.ctl.profiles().read(4, p));
    CHECK(strcmp(p.name, "Sega Saturn") == 0);
    CHECK_FALSE(p.signature.valid);
}

TEST_CASE("rename, re-sign and delete") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 1, 6, InputSignature{});
    CHECK(r.ctl.renameProfile(1, "Saturn"));
    CHECK(strcmp(r.ctl.profiles().activeName(), "Saturn") == 0);
    CHECK_FALSE(r.ctl.renameProfile(1, ""));
    CHECK_FALSE(r.ctl.renameProfile(2, "Nothing here"));

    r.ctl.onSource(kPal, 0);
    CHECK(r.ctl.setProfileSource(1, true));
    CHECK(r.ctl.profiles().signature(1) == kPal);
    CHECK(r.ctl.setProfileSource(1, false));
    CHECK_FALSE(r.ctl.profiles().signature(1).valid);

    CHECK(r.ctl.deleteProfile(1));
    CHECK(r.ctl.activeProfile() == 0);
    CHECK(strcmp(r.ctl.profiles().activeName(), "") == 0);
    CHECK_FALSE(r.ctl.deleteProfile(1));
}

TEST_CASE("a factory reset keeps the profiles and leaves none active") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 1, 6, kSnes);
    r.ctl.factoryReset(0);
    CHECK(r.ctl.activeProfile() == 0);
    CHECK(r.ctl.profiles().exists(1));
    CHECK(r.video.picture.brightness == 0);
}

TEST_CASE("a load after an unconfirmed risky edit reverts to the profile before the edit") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.set(Id::OutputResolution, 5, Origin::Oled, 0); // r480p
    saveWith(r, 3, 0, InputSignature{});
    r.ctl.set(Id::OutputResolution, 1, Origin::Oled, 0); // r1080p
    saveWith(r, 5, 0, InputSignature{});
    REQUIRE(r.ctl.loadProfile(3, Kind::Oled, 0) == Result::Loaded);
    REQUIRE_FALSE(r.ctl.profileModified());

    r.ctl.set(Id::OutputResolution, 4, Origin::Web, 100); // r720p, waiting for keep
    CHECK(r.ctl.profileModified());
    CHECK(r.ctl.loadProfile(5, Kind::Manual, 200) == Result::RevertArmed);
    CHECK(r.ctl.activeProfile() == 5);
    r.ctl.tick(200 + 20'000); // the load restarted the countdown
    CHECK_FALSE(r.ctl.revertPending());
    CHECK(r.video.output == video::OutputMode::R480p);
    CHECK(r.ctl.activeProfile() == 3);
    CHECK_FALSE(r.ctl.profileModified()); // back to exactly what profile 3 holds
}

TEST_CASE("a new source or a switcher first undoes an unconfirmed change, then loads") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.set(Id::OutputResolution, 5, Origin::Oled, 0);
    saveWith(r, 1, 6, kSnes);
    r.ctl.onSource(InputSignature{}, 0);
    r.ctl.set(Id::OutputResolution, 1, Origin::Web, 100); // waiting for keep
    r.ctl.set(Id::PictureBrightness, 9, Origin::Web, 100);
    r.ctl.onSource(kPs2, 1000); // no profile for it: nothing changes
    CHECK(r.ctl.revertPending());
    r.ctl.onSource(kSnes, 2000);
    CHECK_FALSE(r.ctl.revertPending());
    CHECK(r.video.output == video::OutputMode::R480p);
    CHECK(r.video.picture.brightness == 6);
    r.ctl.tick(30'000); // nothing left to revert
    CHECK(r.video.picture.brightness == 6);

    r.ctl.set(Id::OutputResolution, 2, Origin::Console, 40'000);
    CHECK(r.ctl.switcherInput(1, false, 41'000) == Result::Loaded);
    CHECK_FALSE(r.ctl.revertPending());
    CHECK(r.video.output == video::OutputMode::R480p);
}

TEST_CASE("a startup profile that is gone, or an active slot whose file is gone, loads nothing") {
    Rig r;
    r.ctl.begin(false, 0);
    saveWith(r, 2, 6, InputSignature{});
    r.ctl.set(Id::SystemStartupProfile, 9, Origin::Oled, 0);
    r.ctl.tick(10'000);
    r.fs.files.erase("/profile2.bin");

    Rig again;
    again.fs = r.fs;
    again.ctl.begin(false, 0);
    CHECK(again.ctl.activeProfile() == 0);
    CHECK(again.video.picture.brightness == 6); // the saved current settings
    again.ctl.onSource(kSnes, 1000);
    CHECK(again.ctl.activeProfile() == 0);
}

TEST_CASE("a source chosen at detection sets the output once; the settled source finds it done") {
    Rig r;
    r.ctl.begin(false, 0);
    r.ctl.set(Id::OutputResolution, 1, Origin::Oled, 0); // r1080p
    saveWith(r, 1, 6, InputSignature{});
    r.ctl.onSource(kSnes, 0);
    REQUIRE(r.ctl.setProfileSource(1, true));
    r.ctl.onSource(kPs2, 0);                             // another console, no profile for it
    r.ctl.set(Id::OutputResolution, 5, Origin::Oled, 0); // r480p
    saveWith(r, 2, -6, InputSignature{});
    const int before = r.video.outputCalls;

    r.ctl.sourceDetected(kSnes, 1000); // before the engine picks the program
    CHECK(r.video.output == video::OutputMode::R1080p);
    CHECK(r.video.outputCalls == before + 1);
    CHECK(r.ctl.activeProfile() == 1);
    CHECK_FALSE(r.ctl.currentSource().valid); // nothing has settled yet
    r.ctl.onSource(sig(1, 2, 1, 263), 1500);  // settles one line away: the same source
    CHECK(r.video.outputCalls == before + 1);
    CHECK(r.ctl.currentSource() == sig(1, 2, 1, 263));

    // A load at the board still owns the next source, at detection too.
    REQUIRE(r.ctl.loadProfile(2, Kind::Oled, 2000) == Result::Loaded);
    r.ctl.sourceDetected(kPs2, 3000);
    CHECK(r.ctl.activeProfile() == 2);
    r.ctl.sourceDetected(kSnes, 4000);
    CHECK(r.ctl.activeProfile() == 1); // the hold covered only the next new source
}

TEST_CASE("an unnamed profile is named in the board's language, by the front end's strings") {
    Rig r;
    r.ctl.begin(false, 0);
    CHECK(r.ctl.saveProfile(3, "", Save::Keep));
    core::Profile p;
    REQUIRE(r.ctl.profiles().read(3, p));
    CHECK(std::string(p.name) == "Profile 3"); // no front end yet: English
    r.ctl.setDefaultName([](uint8_t slot, uint8_t language, char* out, size_t cap) {
        snprintf(out, cap, language == 1 ? "Perfil %u" : "Profile %u", slot);
    });
    r.ctl.set(Id::SystemLanguage, 1, Origin::Web, 0); // PortuguÃªs (Brasil)
    CHECK(r.ctl.saveProfile(4, "", Save::Keep));
    REQUIRE(r.ctl.profiles().read(4, p));
    CHECK(std::string(p.name) == "Perfil 4");
    CHECK(r.ctl.saveProfile(3, "", Save::Keep)); // an existing name stays
    REQUIRE(r.ctl.profiles().read(3, p));
    CHECK(std::string(p.name) == "Profile 3");
}
