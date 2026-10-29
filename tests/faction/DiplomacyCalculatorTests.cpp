#include "TestHelpers.h"

#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/diplomacy/DiplomacyConfigParser.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;

TEST_CASE("NextFriction adds then clamps", "[diplomacy][calculator]")
{
    const DiplomacyConfig_t config =
        DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json"));
    const DiplomacyCalculator calculator(config);

    CHECK(calculator.NextFriction(19, 5) == 20);
    CHECK(calculator.NextFriction(3, -10) == 0);
}

TEST_CASE("NextBlemishes adds, raises to the floor, then clamps", "[diplomacy][calculator]")
{
    const DiplomacyConfig_t config =
        DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json"));
    const DiplomacyCalculator calculator(config);

    CHECK(calculator.NextBlemishes(0, 2, 6) == 6);
    CHECK(calculator.NextBlemishes(6, 2, 6) == 8);
    CHECK(calculator.NextBlemishes(0, 2, 150) == 99);
    CHECK(calculator.NextBlemishes(1, -5, 0) == 0);
}
