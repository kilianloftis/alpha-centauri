#include "TestHelpers.h"

#include "game/probes/ProbeActionConfig.h"
#include "game/probes/ProbeActionConfigParser.h"
#include "game/probes/ProbeCalculator.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;

namespace
{

const ProbeCostConfig_t& FixtureCost_(const ProbeActionsConfig_t& rConfig, ProbeActionId_t id)
{
    const ProbeActionConfig_t* pAction = rConfig.Find(id);
    REQUIRE(pAction);
    REQUIRE(pAction->cost.has_value());
    return *pAction->cost;
}

} // namespace

TEST_CASE("The fixture's mind-control cost formula is the binary's", "[probe][calculator]")
{
    const ProbeActionsConfig_t config =
        ProbeActionConfigParser{}.ParseConfig(FixturePath("probe_actions.json"));
    LuaRuntime lua;
    const ProbeCalculator calculator(lua);

    MindControlCostInputs_t inputs;
    inputs.garrisonWeight = 3;
    inputs.garrisonMultiplier = 1;
    inputs.mindControlTotal = 9;
    inputs.population = 2;
    inputs.energy = 100;
    inputs.distance = 4;
    // (3 * (1 + 1) + 9 / 4 + 2) * ((100 + 1200) / (4 + 4)) = 10 * 162, dividing integers.
    CHECK(calculator.MindControlCost(FixtureCost_(config, ProbeActionId_t::MindControlBase),
                                     inputs)
          == 1620);
}

TEST_CASE("The fixture's subversion cost formula divides before multiplying",
          "[probe][calculator]")
{
    const ProbeActionsConfig_t config =
        ProbeActionConfigParser{}.ParseConfig(FixturePath("probe_actions.json"));
    LuaRuntime lua;
    const ProbeCalculator calculator(lua);

    SubvertCostInputs_t inputs;
    inputs.mineralCost = 20;
    inputs.energy = 0;
    inputs.distance = 12;
    // 20 * (800 / 14): 1140, where 16000 / 14 would be 1142.
    CHECK(calculator.SubvertCost(FixtureCost_(config, ProbeActionId_t::SubvertUnit), inputs)
          == 1140);
}

TEST_CASE("A cost formula naming an unknown variable throws", "[probe][calculator]")
{
    LuaRuntime lua;
    const ProbeCalculator calculator(lua);
    const ProbeCostConfig_t cost{.formula = "upkeep * 2"};
    CHECK_THROWS(calculator.SubvertCost(cost, SubvertCostInputs_t{}));
}

TEST_CASE("The fixture's drain formulas are the binary's", "[probe][calculator]")
{
    const ProbeActionsConfig_t config =
        ProbeActionConfigParser{}.ParseConfig(FixturePath("probe_actions.json"));
    const DrainFormulas_t& rDrain = *config.Find(ProbeActionId_t::DrainEnergy)->drain;
    LuaRuntime lua;
    const ProbeCalculator calculator(lua);

    // 1000 * 3 / (5 + 1); then 500 * 4 / 6; then 333 + 9 / 2.
    CHECK(calculator.DrainShare(rDrain, {.treasury = 1000, .population = 3,
                                         .factionPopulation = 5}) == 500);
    CHECK(calculator.DrainFloor(rDrain, 500, 4) == 333);
    CHECK(calculator.DrainFloor(rDrain, 500, 0) == 83);
    CHECK(calculator.DrainAmount(rDrain, 333, 9) == 337);
}

TEST_CASE("The fixture's plague formulas are the binary's", "[probe][calculator]")
{
    const ProbeActionsConfig_t config =
        ProbeActionConfigParser{}.ParseConfig(FixturePath("probe_actions.json"));
    const PlagueFormulas_t& rPlague = *config.Find(ProbeActionId_t::GeneticPlague)->plague;
    LuaRuntime lua;
    const ProbeCalculator calculator(lua);

    // 5 * (3 - 1) / 3 + 1 with known clamped up to 1; halved on a repeat.
    PlagueKillInputs_t kills{.population = 5, .defenseTechs = 3, .knownDefenseTechs = 0};
    CHECK(calculator.PlagueKills(rPlague, kills) == 4);
    kills.bRepeated = true;
    CHECK(calculator.PlagueKills(rPlague, kills) == 2);
    // No defense techs at all: the cap of 1 kills only the + 1.
    CHECK(calculator.PlagueKills(rPlague, {.population = 5, .defenseTechs = 0,
                                           .knownDefenseTechs = 0}) == 1);

    CHECK(calculator.PlagueGarrisonDamageBound(rPlague, 1) == 1);
    CHECK(calculator.PlagueGarrisonDamageBound(rPlague, 10) == 5);
    CHECK(calculator.PlagueGarrisonDamage(rPlague, 10, 3, false) == 8);
    CHECK(calculator.PlagueGarrisonDamage(rPlague, 10, 3, true) == 4);
}
