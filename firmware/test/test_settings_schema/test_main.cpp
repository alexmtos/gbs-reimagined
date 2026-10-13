#include <set>
#include <string>

#include "../support/doctest_main.h"
#include "core/settings_def.h"
#include "ui/strings.h"

using namespace gbsr;

namespace {

settings::Def def(uint8_t i) {
    settings::Def d{};
    memcpy_P(&d, &settings::kDefs[i], sizeof d);
    return d;
}

std::string idName(const settings::Def& d) {
    return std::string(settings::kIdNames + d.idOffset);
}

uint32_t fnv1a(const std::string& s) {
    uint32_t h = 0x811C9DC5u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x01000193u;
    }
    return h;
}

} // namespace

TEST_CASE("every generated setting has a consistent definition") {
    std::set<uint32_t> hashes;
    for (uint8_t i = 0; i < settings::kCount; ++i) {
        const settings::Def d = def(i);
        const std::string id = idName(d);
        CAPTURE(id);
        CHECK(d.hash == fnv1a(id));
        CHECK(hashes.insert(d.hash).second);
        CHECK(d.min <= d.def);
        CHECK(d.def <= d.max);
        CHECK(d.milestone() >= 1);
        CHECK(d.milestone() <= 3);
        if (d.type == settings::Type::Enum) {
            REQUIRE(d.valuesOffset != 0xFFFF);
            // The value list has exactly max + 1 NUL-separated entries.
            const char* p = settings::kValueIds + d.valuesOffset;
            int count = 0;
            while (*p != '\0') {
                ++count;
                p += strlen(p) + 1;
            }
            CHECK(count == d.max + 1);
        }
    }
}

TEST_CASE("the frame-lock and output-resolution settings are risky, picture ones are not") {
    CHECK(def(static_cast<uint8_t>(settings::Id::OutputResolution)).risky());
    CHECK(def(static_cast<uint8_t>(settings::Id::OutputFrameLock)).risky());
    CHECK_FALSE(def(static_cast<uint8_t>(settings::Id::PictureBrightness)).risky());
}

TEST_CASE("every locale has a label, a hint and value labels for every setting") {
    REQUIRE(i18n::kLocaleCount >= 2);
    for (uint8_t l = 0; l < i18n::kLocaleCount; ++l) {
        i18n::LocaleTables t{};
        memcpy_P(&t, &i18n::kLocales[l], sizeof t);
        for (uint8_t i = 0; i < settings::kCount; ++i) {
            const settings::Def d = def(i);
            CAPTURE(idName(d));
            CHECK(strlen(t.text + t.label[i]) > 0);
            CHECK(strlen(t.text + t.hint[i]) > 0);
            if (d.type == settings::Type::Enum) {
                REQUIRE(t.values[i] != 0xFFFF);
                // Labels are joined by 0x1F; count them.
                int labels = 1;
                for (const char* p = t.text + t.values[i]; *p; ++p) {
                    labels += (*p == '\x1f');
                }
                int specials =
                    (d.special[0] != settings::kNoSpecial) + (d.special[1] != settings::kNoSpecial);
                CHECK(labels == d.max + 1 + specials);
            }
        }
    }
}
