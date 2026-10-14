#include "../support/doctest_main.h"
#include "../support/fake_i2c.h"
#include "chip/fields.gen.h"
#include "chip/tv5725.h"
#include "video/ops.h"
#include "video/tuning.h"

using namespace gbsr;
using namespace gbsr::tv5725::fields;

namespace {

struct Rig {
    test::FakeI2cBus bus;
    test::FakeTv5725 dev;
    tv5725::Chip chip{bus};
    video::LockState lock;
    video::Ops ops{chip, lock};
    video::PathContext ctx;
    Rig() { bus.attach(tv5725::Chip::kAddress, dev); }
};

} // namespace

TEST_CASE("a fixed SOG threshold wins over every automatic change, which is kept for later") {
    Rig r;
    r.ops.setSogLevel(13);
    CHECK(r.chip.get(ADC_SOGCTRL) == 13);
    r.lock.sogManual = 7;
    r.ops.setSogLevel(12); // an automatic step while the user's value stands
    CHECK(r.chip.get(ADC_SOGCTRL) == 7);
    CHECK(r.ops.sogLevelInEffect() == 7);
    CHECK(r.lock.sogLevel == 13); // the automatic choice, untouched
    r.lock.sogManual = -1;
    r.ops.setSogLevel(r.lock.sogLevel); // back to Auto
    CHECK(r.chip.get(ADC_SOGCTRL) == 13);
}

TEST_CASE("a fixed ADC phase is what every latch writes; the sync phase stays automatic") {
    Rig r;
    r.lock.phaseAdc = 16;
    r.lock.phaseSp = 9;
    r.lock.phaseAdcManual = 3;
    r.ops.setSogLevel(5); // latches both phases
    CHECK(r.chip.get(PA_ADC_S) == 3);
    CHECK(r.chip.get(PA_SP_S) == 9);
    CHECK(r.ops.phaseAdcInEffect() == 3);
    r.lock.phaseAdcManual = -1;
    r.ops.latchPhaseAdc();
    CHECK(r.chip.get(PA_ADC_S) == 16);
}

TEST_CASE("the automatic SOG search does not run under a fixed threshold") {
    Rig r;
    r.ctx.syncCsync = true;
    r.chip.write(SP_SOG_MODE, 1);
    r.lock.sogManual = 9;
    video::SogOptimizer opt;
    opt.start(r.ops, r.ctx);
    CHECK_FALSE(opt.busy());
    CHECK(r.lock.sogManual == 9);
}
