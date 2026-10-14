#include "../support/doctest_main.h"
#include "ui/knob.h"

using gbsr::ui::Knob;

namespace {

// Feeds the knob a button level for `ms` milliseconds in 10 ms steps; collects the events.
struct Feed {
    Knob knob;
    uint32_t now = 0;
    int presses = 0, longPresses = 0, turned = 0, coarse = 0;

    void hold(bool down, uint32_t ms, int detentsAtStart = 0) {
        for (uint32_t t = 0; t < ms; t += 10) {
            const Knob::Events e = knob.update(down, t == 0 ? detentsAtStart : 0, now += 10);
            presses += e.press;
            longPresses += e.longPress;
            turned += e.turn;
            coarse += e.coarse ? e.turn : 0;
        }
    }
};

} // namespace

TEST_CASE("a short press is one press") {
    Feed f;
    f.hold(false, 100);
    f.hold(true, 200);
    f.hold(false, 100);
    CHECK(f.presses == 1);
    CHECK(f.longPresses == 0);
}

TEST_CASE("contact bounce shorter than the debounce time is ignored") {
    Feed f;
    f.hold(true, 10);
    f.hold(false, 10);
    f.hold(true, 10);
    f.hold(false, 100);
    CHECK(f.presses == 0);
}

TEST_CASE("holding fires one long press while still held, and no press on release") {
    Feed f;
    f.hold(true, 1500);
    CHECK(f.longPresses == 1);
    f.hold(false, 100);
    CHECK(f.presses == 0);
    CHECK(f.longPresses == 1);
}

TEST_CASE("turning while held is coarse and cancels the press") {
    Feed f;
    f.hold(true, 100);
    f.hold(true, 50, 2);
    f.hold(false, 100);
    CHECK(f.turned == 2);
    CHECK(f.coarse == 2);
    CHECK(f.presses == 0);
    CHECK(f.longPresses == 0);
    f.hold(false, 50, -1);
    CHECK(f.turned == 1);
    CHECK(f.coarse == 2); // released: fine steps again
}
