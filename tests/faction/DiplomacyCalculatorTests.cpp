#include "TestHelpers.h"

#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/diplomacy/DiplomacyConfigParser.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;

TEST_CASE("ClampFriction keeps friction in the fixture range", "[diplomacy][calculator]")
{
    const DiplomacyConfig_t config =
        DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json"));
    const DiplomacyCalculator calculator(config);

    CHECK(calculator.ClampFriction(-1) == 0);
    CHECK(calculator.ClampFriction(0) == 0);
    CHECK(calculator.ClampFriction(20) == 20);
    CHECK(calculator.ClampFriction(21) == 20);
}
