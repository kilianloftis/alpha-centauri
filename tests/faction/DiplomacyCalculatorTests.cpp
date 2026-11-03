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

namespace
{

// 1000 / (6 + 4) x 3 = 300 under the fixture's base_value, with no region multiplier.
BaseValueInputs_t PlainBase_()
{
    BaseValueInputs_t inputs;
    inputs.distance = 6;
    inputs.credits = 0;
    inputs.population = 3;
    inputs.bGarrisoned = true;
    inputs.evaluatorRegionBases = 2;
    inputs.counterpartyRegionBases = 1;
    return inputs;
}

} // namespace

TEST_CASE("BaseValue scales the treasury by distance and population, with floors",
          "[diplomacy][calculator][base_value]")
{
    const DiplomacyConfig_t config =
        DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json"));
    const DiplomacyCalculator calculator(config);

    BaseValueInputs_t inputs = PlainBase_();
    CHECK(calculator.BaseValue(inputs) == 300);

    inputs.population = 1;
    CHECK(calculator.BaseValue(inputs) == 300);

    inputs.population = 5;
    CHECK(calculator.BaseValue(inputs) == 500);

    inputs.population = 3;
    inputs.distance = 46;
    CHECK(calculator.BaseValue(inputs) == 100);
}

TEST_CASE("BaseValue halves an empty base, truncating", "[diplomacy][calculator][base_value]")
{
    const DiplomacyConfig_t config =
        DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json"));
    const DiplomacyCalculator calculator(config);

    BaseValueInputs_t inputs = PlainBase_();
    inputs.credits = 70;
    CHECK(calculator.BaseValue(inputs) == 321);

    inputs.bGarrisoned = false;
    CHECK(calculator.BaseValue(inputs) == 160);
}

TEST_CASE("BaseValue scales by who holds bases in the region",
          "[diplomacy][calculator][base_value]")
{
    const DiplomacyConfig_t config =
        DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json"));
    const DiplomacyCalculator calculator(config);
    BaseValueInputs_t inputs = PlainBase_();

    SECTION("the counterparty's absence and the evaluator's lone base stack")
    {
        inputs.evaluatorRegionBases = 1;
        inputs.counterpartyRegionBases = 0;
        CHECK(calculator.BaseValue(inputs) == 1200);

        inputs.counterpartyRegionBases = 1;
        CHECK(calculator.BaseValue(inputs) == 600);

        inputs.evaluatorRegionBases = 0;
        inputs.counterpartyRegionBases = 0;
        CHECK(calculator.BaseValue(inputs) == 600);
    }

    SECTION("an outnumbered evaluator halves the value from exactly the ratio")
    {
        inputs.evaluatorRegionBases = 2;
        inputs.counterpartyRegionBases = 10;
        CHECK(calculator.BaseValue(inputs) == 150);

        inputs.counterpartyRegionBases = 9;
        CHECK(calculator.BaseValue(inputs) == 300);
    }
}

TEST_CASE("BaseValue adds facilities by their order, projects and the evaluator's units",
          "[diplomacy][calculator][base_value]")
{
    const DiplomacyConfig_t config =
        DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json"));
    const DiplomacyCalculator calculator(config);
    BaseValueInputs_t inputs = PlainBase_();

    inputs.facilityMinerals = {30, 50};
    CHECK(calculator.BaseValue(inputs) == 300 + (30 + 2 * 50) / 10);

    inputs.facilityMinerals.clear();
    inputs.projectMinerals = 200;
    CHECK(calculator.BaseValue(inputs) == 300 + 500);

    inputs.projectMinerals = 0;
    inputs.evaluatorUnitMinerals = 50;
    CHECK(calculator.BaseValue(inputs) == 300 + 10);
}
