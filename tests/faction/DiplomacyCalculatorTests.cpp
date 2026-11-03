#include "TestHelpers.h"

#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/diplomacy/DiplomacyConfigParser.h"
#include "game/faction/FactionConfig.h"
#include "game/research/TechConfigParser.h"

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

namespace
{

const DiplomacyCalculator& GiftCalculator_()
{
    static const DiplomacyConfig_t s_config =
        DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json"));
    static const DiplomacyCalculator s_calculator(s_config);
    return s_calculator;
}

GiftSizeInputs_t GiftSize_(int value, int recipientCredits, int giverCredits, int turn)
{
    GiftSizeInputs_t inputs;
    inputs.value = value;
    inputs.recipientCredits = recipientCredits;
    inputs.giverCredits = giverCredits;
    inputs.turn = turn;
    return inputs;
}

} // namespace

TEST_CASE("A credit gift lowers friction by tiers of its value", "[diplomacy][calculator][gifts]")
{
    const DiplomacyCalculator& rCalculator = GiftCalculator_();

    CHECK(rCalculator.CreditGiftFriction(9) == 0);
    CHECK(rCalculator.CreditGiftFriction(10) == -1);
    CHECK(rCalculator.CreditGiftFriction(25) == -1);
    CHECK(rCalculator.CreditGiftFriction(100) == -3);
    CHECK(rCalculator.CreditGiftFriction(200) == -4);
    CHECK(rCalculator.CreditGiftFriction(500) == -7);
}

TEST_CASE("A credit gift is generous, notable or small by both treasuries and the turn",
          "[diplomacy][calculator][gifts]")
{
    const DiplomacyCalculator& rCalculator = GiftCalculator_();

    // Generous: at least half the recipient's 200, a third of the giver's 300 and twice turn 50.
    CHECK(rCalculator.CreditGiftSize(GiftSize_(100, 200, 300, 50)) == GiftSize_t::Generous);
    CHECK(rCalculator.CreditGiftSize(GiftSize_(100, 202, 300, 50)) == GiftSize_t::Notable);
    CHECK(rCalculator.CreditGiftSize(GiftSize_(100, 200, 303, 50)) == GiftSize_t::Notable);
    CHECK(rCalculator.CreditGiftSize(GiftSize_(100, 200, 300, 51)) == GiftSize_t::Notable);

    // Notable: a quarter of 200, at least 25, a sixth of 300 and the turn.
    CHECK(rCalculator.CreditGiftSize(GiftSize_(50, 200, 300, 50)) == GiftSize_t::Notable);
    CHECK(rCalculator.CreditGiftSize(GiftSize_(50, 204, 300, 50)) == GiftSize_t::Small);
    CHECK(rCalculator.CreditGiftSize(GiftSize_(50, 200, 306, 50)) == GiftSize_t::Small);
    CHECK(rCalculator.CreditGiftSize(GiftSize_(50, 200, 300, 51)) == GiftSize_t::Small);
    // Short of generous (half of 52 is 26), notable needs at least 25.
    CHECK(rCalculator.CreditGiftSize(GiftSize_(25, 52, 0, 1)) == GiftSize_t::Notable);
    CHECK(rCalculator.CreditGiftSize(GiftSize_(24, 52, 0, 1)) == GiftSize_t::Small);
}

TEST_CASE("A tech gift is generous from 16 whatever the treasuries, and lowers friction by its "
          "value",
          "[diplomacy][calculator][gifts]")
{
    const DiplomacyCalculator& rCalculator = GiftCalculator_();

    CHECK(rCalculator.TechGiftSize(GiftSize_(15, 0, 0, 0)) == GiftSize_t::Notable);
    CHECK(rCalculator.TechGiftSize(GiftSize_(16, 0, 0, 0)) == GiftSize_t::Generous);
    CHECK(rCalculator.TechGiftSize(GiftSize_(16, 10000, 10000, 300)) == GiftSize_t::Generous);

    CHECK(rCalculator.TechGiftFriction(7) == -7);
}

TEST_CASE("A base gift is generous by both treasuries and the turn, otherwise modest",
          "[diplomacy][calculator][gifts]")
{
    const DiplomacyCalculator& rCalculator = GiftCalculator_();

    CHECK(rCalculator.BaseGiftSize(GiftSize_(100, 200, 300, 50)) == GiftSize_t::Generous);
    CHECK(rCalculator.BaseGiftSize(GiftSize_(100, 202, 300, 50)) == GiftSize_t::Modest);
    CHECK(rCalculator.BaseGiftSize(GiftSize_(100, 200, 303, 50)) == GiftSize_t::Modest);
    CHECK(rCalculator.BaseGiftSize(GiftSize_(100, 200, 300, 51)) == GiftSize_t::Modest);

    CHECK(rCalculator.BaseGiftFriction(300) == -450);
    CHECK(rCalculator.BaseGiftFriction(101) == -151);
}

TEST_CASE("The giver's threshold scale applies after each threshold truncates",
          "[diplomacy][calculator][gifts]")
{
    const DiplomacyCalculator& rCalculator = GiftCalculator_();

    // Half of the recipient's 6 is 3; scaled by 0.5 it is 1.5, which 1 misses and 2 meets.
    GiftSizeInputs_t inputs = GiftSize_(1, 6, 0, 0);
    inputs.thresholdScale = 0.5;
    CHECK(rCalculator.BaseGiftSize(inputs) == GiftSize_t::Modest);
    inputs.value = 2;
    CHECK(rCalculator.BaseGiftSize(inputs) == GiftSize_t::Generous);

    // At 0 every threshold is 0.
    GiftSizeInputs_t free = GiftSize_(0, 10000, 10000, 300);
    free.thresholdScale = 0.0;
    CHECK(rCalculator.BaseGiftSize(free) == GiftSize_t::Generous);
    CHECK(rCalculator.CreditGiftSize(free) == GiftSize_t::Generous);
    CHECK(rCalculator.TechGiftSize(free) == GiftSize_t::Generous);

    GiftSizeInputs_t tech = GiftSize_(8, 0, 0, 0);
    tech.thresholdScale = 0.5;
    CHECK(rCalculator.TechGiftSize(tech) == GiftSize_t::Generous);
    tech.value = 7;
    CHECK(rCalculator.TechGiftSize(tech) == GiftSize_t::Notable);
}

TEST_CASE("A gift raises the giver's count by its size, capped at the giver's blemishes",
          "[diplomacy][calculator][gifts]")
{
    const DiplomacyCalculator& rCalculator = GiftCalculator_();

    CHECK(rCalculator.NextGifts(0, 5, GiftSize_t::Generous) == 2);
    CHECK(rCalculator.NextGifts(4, 5, GiftSize_t::Generous) == 5);
    CHECK(rCalculator.NextGifts(0, 5, GiftSize_t::Notable) == 1);
    CHECK(rCalculator.NextGifts(1, 5, GiftSize_t::Notable) == 1);
    CHECK(rCalculator.NextGifts(1, 5, GiftSize_t::Modest) == 2);
    CHECK(rCalculator.NextGifts(1, 5, GiftSize_t::Small) == 1);
    CHECK(rCalculator.NextGifts(0, 0, GiftSize_t::Generous) == 0);
    CHECK(rCalculator.NextGifts(0, 0, GiftSize_t::Notable) == 0);
}

TEST_CASE("A battle on allied ground floors the attacker's gift count",
          "[diplomacy][calculator][gifts]")
{
    const DiplomacyCalculator& rCalculator = GiftCalculator_();

    CHECK(rCalculator.AlliedTerritoryGifts(0) == 1);
    CHECK(rCalculator.AlliedTerritoryGifts(3) == 3);
}

TEST_CASE("A tech's value weighs each priority and halves without a match",
          "[diplomacy][calculator][gifts]")
{
    const DiplomacyCalculator& rCalculator = GiftCalculator_();
    const TechAiWeights_t weights{0, 3, 2, 2};
    AITendenciesConfig priorities;

    // 3 + 2 + 2 = 7, no priority matches: (7 + 1) / 2.
    CHECK(rCalculator.TechWeightValue(weights, priorities, 0, 4) == 4);

    // A tech priority doubles the tech weight and matches: 6 + 2 + 2.
    priorities.tech = true;
    CHECK(rCalculator.TechWeightValue(weights, priorities, 0, 4) == 10);

    // Growth matches below 4 bases without the priority.
    const AITendenciesConfig none;
    CHECK(rCalculator.TechWeightValue(weights, none, 0, 3) == 7);

    // Power matches on any Vendetta.
    const TechAiWeights_t power{3, 0, 0, 0};
    CHECK(rCalculator.TechWeightValue(power, none, 0, 4) == 2);
    CHECK(rCalculator.TechWeightValue(power, none, 1, 4) == 3);
}

TEST_CASE("An AI lacking a tech values it less for other owners and a deeper tree",
          "[diplomacy][calculator][gifts]")
{
    const DiplomacyCalculator& rCalculator = GiftCalculator_();

    CHECK(rCalculator.UnownedTechValue(10, 1, 3, 3) == 10);
    CHECK(rCalculator.UnownedTechValue(10, 3, 3, 3) == 6);
    // (10 x (1 + 1)) / (4 + 1).
    CHECK(rCalculator.UnownedTechValue(10, 0, 1, 4) == 4);
    CHECK(rCalculator.UnownedTechValue(2, 4, 1, 4) == 1);
}
