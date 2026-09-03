// Pop composition: graph-derived pop classes, the psych ladder, overflow resolution, and the
// load-time rules that keep the promotion graph classifiable.

#include "GameFixtures.h"
#include "TempConfigFile.h"
#include "TestHelpers.h"

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionApplyResult.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/ResearchManager.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/population/calculators/DroneCalculator.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <string>

using actest::TempConfigFile;

using namespace ac;
using Catch::Approx;
using Catch::Matchers::ContainsSubstring;

namespace
{

// Seats for one type id, or 0 when that type was not seated at all.
int SeatedCount_(const PopCompositionResult_t& rResult, const std::string& rTypeId)
{
    const auto it = std::find_if(rResult.droneSeats.begin(), rResult.droneSeats.end(),
                                 [&](const DroneSeat_t& rSeat) { return rSeat.typeId == rTypeId; });
    return it == rResult.droneSeats.end() ? 0 : it->count;
}

int SeatedBodies_(const PopCompositionResult_t& rResult)
{
    int total = 0;
    for (const DroneSeat_t& rSeat : rResult.droneSeats)
    {
        total += rSeat.count;
    }
    return total;
}

// A registry built from an inline pop_types document, for the load-time rules below.
PopTypeRegistry LoadPopTypes_(const char* pName, const std::string& rJson)
{
    const TempConfigFile file(pName, rJson);
    PopTypeRegistry registry;
    registry.Load(file.Path());
    return registry;
}

const char* k_ChainPrologue = R"([
  { "id": "Worker", "name": "Worker", "display_glyph": "W", "is_default": true,
    "can_work_tile": true, "player_assignable": true,
    "psych_to_promote": 2, "promotes_to": "Talent" },
  { "id": "Talent", "name": "Talent", "display_glyph": "A", "can_work_tile": true })";

struct CompositionFixture_
{
    LuaRuntime lua;
    PopCompositionConfig_t config =
        PopCompositionConfigParser{}.ParseConfig(actest::FixturePath("pop_composition.json"));
    DroneCalculator droneCalculator{config, lua};
};

} // namespace

TEST_CASE("Pop class comes from the promotion graph, not a config role",
          "[population][composition]")
{
    actest::PopTypeRegistryOnly reg;

    const Pop worker(reg.popTypes.Get("Worker"));
    CHECK(worker.IsPlainWorker());
    CHECK(worker.IsInCompositionGraph());
    CHECK_FALSE(worker.IsDrone());
    CHECK_FALSE(worker.IsTalent());
    // is_default, so not protected: default workers are what composition converts from.
    CHECK_FALSE(worker.IsPlayerChoiceType());

    const Pop drone(reg.popTypes.Get("Drone"));
    CHECK(drone.IsDrone());
    CHECK(drone.IsWorker());
    CHECK_FALSE(drone.IsPlainWorker());

    const Pop superDrone(reg.popTypes.Get("SuperDrone"));
    CHECK(superDrone.IsDrone());

    const Pop talent(reg.popTypes.Get("Talent"));
    CHECK(talent.IsTalent());
    CHECK(talent.IsWorker());
    CHECK_FALSE(talent.IsPlainWorker());

    // Outside the graph entirely: composition never touches a specialist.
    const Pop doctor(reg.popTypes.Get("Doctor"));
    CHECK_FALSE(doctor.IsInCompositionGraph());
    CHECK_FALSE(doctor.ParticipatesInComposition());
    CHECK_FALSE(doctor.IsPlainWorker());
    CHECK_FALSE(doctor.IsWorker());
    CHECK(doctor.IsPlayerChoiceType());
}

TEST_CASE("Drone weight and riot weight are different quantities", "[population][riot]")
{
    actest::PopTypeRegistryOnly reg;

    const Pop drone(reg.popTypes.Get("Drone"));
    const Pop superDrone(reg.popTypes.Get("SuperDrone"));

    // A super drone absorbs two drones of *pressure* into one body...
    CHECK(drone.GetDroneWeight() == 1);
    CHECK(superDrone.GetDroneWeight() == 2);

    // ...but riots like any single citizen. Conflating these is what riot_contribution did.
    CHECK(drone.GetMoodWeights().riot == 1);
    CHECK(superDrone.GetMoodWeights().riot == 1);

    const Pop talent(reg.popTypes.Get("Talent"));
    CHECK(talent.GetMoodWeights().riot == -1);
    CHECK(talent.GetMoodWeights().goldenAge == 1);

    const Pop worker(reg.popTypes.Get("Worker"));
    CHECK(worker.GetMoodWeights().riot == 0);
    CHECK(worker.GetMoodWeights().goldenAge == -1);

    // Specialists declare neither weight, which is what keeps them out of both sums.
    const Pop doctor(reg.popTypes.Get("Doctor"));
    CHECK(doctor.GetMoodWeights().riot == 0);
    CHECK(doctor.GetMoodWeights().goldenAge == 0);
}

TEST_CASE("ApplyCompositionResult seats configured drone tiers and talents",
          "[population][composition]")
{
    actest::BaseFixture fixture;
    ac::PopulationManager& pops = fixture.MakeBase(4, 4, /*initialPopulation*/ 0).GetPopulation();
    pops.AddPop("Worker");
    pops.AddPop("Worker");
    pops.AddPop("Drone");
    pops.AddPop("Worker");

    ac::PopCompositionResult_t targets;
    targets.droneSeats = {{"Drone", 2}};
    targets.expectedTalents = 1;

    pops.ApplyCompositionResult(targets);

    CHECK(pops.GetSize() == 4);
    CHECK(pops.GetDroneCount() == 2);
    CHECK(pops.GetTalentCount() == 1);
    CHECK(pops.GetPlainWorkerCount() == 1);
}

TEST_CASE("RecalculateComposition is stable when called repeatedly within a turn",
          "[population][composition][psych]")
{
    actest::BaseFixture fixture;
    ac::BaseManager& base = fixture.MakeBase(4, 4, /*initialPopulation*/ 6);
    ac::PopulationManager& pops = base.GetPopulation();

    const int dronesAfterFirst = pops.GetDroneCount();
    const int talentsAfterFirst = pops.GetTalentCount();
    const int workersAfterFirst = pops.GetPlainWorkerCount();

    for (int pass = 0; pass < 5; ++pass)
    {
        pops.RecalculateComposition();
        CHECK(pops.GetDroneCount() == dronesAfterFirst);
        CHECK(pops.GetTalentCount() == talentsAfterFirst);
        CHECK(pops.GetPlainWorkerCount() == workersAfterFirst);
    }
}

TEST_CASE("Mood sums range over the composition pool, never base size",
          "[population][riot][goldenage]")
{
    actest::BaseFixture fixture;
    ac::PopulationManager& pops = fixture.MakeBase(4, 4, /*initialPopulation*/ 0).GetPopulation();
    pops.AddPop("Drone");
    pops.AddPop("Drone");
    pops.AddPop("Talent");
    pops.AddPop("Talent");

    // 2 drones (+1 each) and 2 talents (−1 each) cancel exactly.
    CHECK(pops.GetMoodWeightSums().riot == 0);

    pops.AddPop("Drone");
    CHECK(pops.GetMoodWeightSums().riot == 1);

    // A doctor moves neither sum, even though it grows the base.
    const int riotBefore = pops.GetMoodWeightSums().riot;
    const int goldenBefore = pops.GetMoodWeightSums().goldenAge;
    pops.AddPop("Doctor");
    CHECK(pops.GetMoodWeightSums().riot == riotBefore);
    CHECK(pops.GetMoodWeightSums().goldenAge == goldenBefore);
}

TEST_CASE("Mood sums skip Outside pops even when they declare weights",
          "[population][riot][goldenage]")
{
    // Defense in depth for §9: Outside of the graph must not affect mood, regardless of config.
    const TempConfigFile file("ac_pop_types_outside_riot.json", R"([
  { "id": "Worker", "name": "Worker", "display_glyph": "W", "is_default": true,
    "can_work_tile": true, "player_assignable": true,
    "psych_to_promote": 2, "promotes_to": "Talent",
    "effects": [
      { "type": "StatModifier", "scope": "ThisPop",
        "parameters": { "stat": "golden_age_weight", "amount": -1, "op": "Add" } }
    ] },
  { "id": "Talent", "name": "Talent", "display_glyph": "A", "can_work_tile": true },
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Worker",
    "effects": [
      { "type": "StatModifier", "scope": "ThisPop",
        "parameters": { "stat": "riot_weight", "amount": 1, "op": "Add" } }
    ] },
  { "id": "Doctor", "name": "Doctor", "display_glyph": "D", "can_work_tile": false,
    "player_assignable": true,
    "effects": [
      { "type": "StatModifier", "scope": "ThisPop",
        "parameters": { "stat": "riot_weight", "amount": 5, "op": "Add" } }
    ] }
])");
    actest::BaseFixture fixture;
    fixture.popTypes().Load(file.Path());
    ac::PopulationManager& pops = fixture.MakeBase(4, 4, /*initialPopulation*/ 0).GetPopulation();
    pops.AddPop("Doctor");
    CHECK(pops.GetMoodWeightSums().riot == 0);

    pops.AddPop("Drone");
    CHECK(pops.GetMoodWeightSums().riot == 1);
}

TEST_CASE("Golden age is talents against workers and drones, ignoring specialists",
          "[population][goldenage]")
{
    actest::BaseFixture fixture;
    ac::PopulationManager& pops = fixture.MakeBase(4, 4, /*initialPopulation*/ 0).GetPopulation();
    pops.AddPop("Talent");
    pops.AddPop("Talent");
    pops.AddPop("Worker");
    pops.AddPop("Worker");

    // +1 +1 −1 −1 = 0, and the shipping threshold is 0: the tie is a golden age.
    REQUIRE(pops.GetMoodWeightSums().goldenAge == 0);
    pops.ForecastMood();
    pops.CommitMood();
    CHECK(pops.IsInGoldenAge());

    // Doctors are outside the calculation: a doctor-heavy base can still reach a golden age.
    pops.AddPop("Doctor");
    pops.AddPop("Doctor");
    pops.ForecastMood();
    pops.CommitMood();
    CHECK(pops.IsInGoldenAge());

    // One more plain worker tips the sum negative.
    pops.AddPop("Worker");
    REQUIRE(pops.GetMoodWeightSums().goldenAge == -1);
    pops.ForecastMood();
    pops.CommitMood();
    CHECK_FALSE(pops.IsInGoldenAge());
}

TEST_CASE("Any drone blocks a golden age regardless of the weight sum",
          "[population][goldenage]")
{
    actest::BaseFixture fixture;
    ac::PopulationManager& pops = fixture.MakeBase(4, 4, /*initialPopulation*/ 0).GetPopulation();
    for (int i = 0; i < 4; ++i)
    {
        pops.AddPop("Talent");
    }
    pops.AddPop("Drone");

    // Sum is +4 −1 = 3, comfortably over the threshold, but the gate is unconditional.
    REQUIRE(pops.GetMoodWeightSums().goldenAge == 3);
    pops.ForecastMood();
    pops.CommitMood();
    CHECK_FALSE(pops.IsInGoldenAge());
}

TEST_CASE("Golden age grants +1 econ and +2% growth via pop_composition effects",
          "[population][goldenage][effects]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4, /*initialPopulation*/ 0);
    PopulationManager& pops = base.GetPopulation();
    pops.AddPop("Talent");
    pops.AddPop("Talent");
    pops.AddPop("Worker");
    pops.AddPop("Worker");

    const auto resolveEcon = [&](double seed) {
        return ResolveBaseStat(base.GetBaseEffects(), StatId_t::Econ, seed);
    };
    const auto resolveGrowthRate = [&]() {
        return ResolveBaseStat(base.GetBaseEffects(), StatId_t::GrowthRate, 100.0);
    };

    CHECK(resolveEcon(4.0) == Approx(4.0));
    CHECK(resolveGrowthRate() == Approx(100.0));

    pops.ForecastMood();
    pops.CommitMood();
    REQUIRE(pops.IsInGoldenAge());

    CHECK(resolveEcon(4.0) == Approx(5.0));
    CHECK(resolveGrowthRate() == Approx(102.0));

    GrowthConfig_t growthConfig;
    growthConfig.nutrientsPerPop = 10;
    CHECK(GrowthCalculator::ComputeNutrientsRequired(growthConfig, 3, base.GetBaseEffects()) == 29);

    pops.AddPop("Worker");
    pops.ForecastMood();
    pops.CommitMood();
    REQUIRE_FALSE(pops.IsInGoldenAge());

    CHECK(resolveEcon(4.0) == Approx(4.0));
    CHECK(resolveGrowthRate() == Approx(100.0));
    CHECK(GrowthCalculator::ComputeNutrientsRequired(growthConfig, 3, base.GetBaseEffects()) == 30);
}

TEST_CASE("Composition seats drone pressure into the pool", "[population][composition]")
{
    actest::PopRulesFixture reg;
    CompositionFixture_ calc;
    PopCompositionCalculator composition(calc.config, reg.popTypes);

    SECTION("pressure below the pool needs no super drones")
    {
        PopCompositionInputs_t inputs;
        inputs.dronePressure = 3;
        inputs.poolSize = 5;
        const PopCompositionResult_t result = composition.Calculate(inputs);

        CHECK(SeatedCount_(result, "Drone") == 3);
        CHECK(SeatedCount_(result, "SuperDrone") == 0);
        CHECK(result.droppedPressure == 0);
    }

    SECTION("one over the pool consolidates a single body, freeing no worker")
    {
        PopCompositionInputs_t inputs;
        inputs.dronePressure = 5;
        inputs.poolSize = 4;
        const PopCompositionResult_t result = composition.Calculate(inputs);

        CHECK(SeatedCount_(result, "SuperDrone") == 1);
        CHECK(SeatedCount_(result, "Drone") == 3);
        // Four bodies carrying five pressure. Consolidation upgrades a body; it does not
        // release one back to the worker pool.
        CHECK(SeatedBodies_(result) == 4);
        CHECK(result.droppedPressure == 0);
    }

    SECTION("pressure past twice the pool is dropped rather than seated")
    {
        PopCompositionInputs_t inputs;
        inputs.dronePressure = 7;
        inputs.poolSize = 3;
        const PopCompositionResult_t result = composition.Calculate(inputs);

        CHECK(SeatedCount_(result, "SuperDrone") == 3);
        CHECK(SeatedCount_(result, "Drone") == 0);
        CHECK(result.droppedPressure == 1);
    }

    SECTION("dropping the excess is invisible: 6 and 7 pressure seat identically")
    {
        PopCompositionInputs_t six;
        six.dronePressure = 6;
        six.poolSize = 3;
        PopCompositionInputs_t seven;
        seven.dronePressure = 7;
        seven.poolSize = 3;

        const PopCompositionResult_t seatedSix = composition.Calculate(six);
        const PopCompositionResult_t seatedSeven = composition.Calculate(seven);

        CHECK(SeatedCount_(seatedSix, "SuperDrone") == SeatedCount_(seatedSeven, "SuperDrone"));
        CHECK(seatedSix.droppedPressure == 0);
        CHECK(seatedSeven.droppedPressure == 1);
    }
}

TEST_CASE("Seating never overshoots the pressure it was given", "[population][composition]")
{
    actest::PopRulesFixture reg;
    CompositionFixture_ calc;
    PopCompositionCalculator composition(calc.config, reg.popTypes);

    for (int pressure = 0; pressure <= 12; ++pressure)
    {
        for (int pool = 0; pool <= 6; ++pool)
        {
            PopCompositionInputs_t inputs;
            inputs.dronePressure = pressure;
            inputs.poolSize = pool;
            const PopCompositionResult_t result = composition.Calculate(inputs);

            int seated = 0;
            for (const DroneSeat_t& rSeat : result.droneSeats)
            {
                seated += rSeat.count * reg.popTypes.Get(rSeat.typeId).droneWeight;
            }
            CHECK(seated + result.droppedPressure == pressure);
            CHECK(seated <= pressure);
            CHECK(SeatedBodies_(result) <= pool);
        }
    }
}

TEST_CASE("Talents absorb overflow before any super drone is minted",
          "[population][composition]")
{
    actest::PopRulesFixture reg;
    CompositionFixture_ calc;
    PopCompositionCalculator composition(calc.config, reg.popTypes);

    PopCompositionInputs_t inputs;
    inputs.dronePressure = 3;
    inputs.resolvedTalents = 3;
    inputs.poolSize = 4;
    const PopCompositionResult_t result = composition.Calculate(inputs);

    // 3 + 3 > 4, so one drone/talent pair cancels: 2 pressure and 2 talents in 4 bodies.
    CHECK(result.expectedTalents == 2);
    CHECK(SeatedCount_(result, "Drone") == 2);
    CHECK(SeatedCount_(result, "SuperDrone") == 0);
}

TEST_CASE("The psych ladder is the only psych consumer", "[population][composition][psych]")
{
    actest::PopRulesFixture reg;
    CompositionFixture_ calc;
    PopCompositionCalculator composition(calc.config, reg.popTypes);

    SECTION("psych buys down drone pressure at the configured cost")
    {
        PopCompositionInputs_t inputs;
        inputs.dronePressure = 3;
        inputs.psychAvailable = 4;
        inputs.poolSize = 5;
        const PopCompositionResult_t result = composition.Calculate(inputs);

        // 2 psych per step, two steps taken.
        CHECK(result.psychSpent == 4);
        CHECK(SeatedCount_(result, "Drone") == 1);
    }

    SECTION("psych left over once pressure is gone promotes workers to talents")
    {
        PopCompositionInputs_t inputs;
        inputs.dronePressure = 1;
        inputs.psychAvailable = 6;
        inputs.poolSize = 5;
        const PopCompositionResult_t result = composition.Calculate(inputs);

        // 2 clears the last pressure; the remaining 4 buys two talents.
        CHECK(SeatedBodies_(result) == 0);
        CHECK(result.expectedTalents == 2);
        CHECK(result.psychSpent == 6);
    }

    SECTION("Talents contributions seat without psych")
    {
        PopCompositionInputs_t inputs;
        inputs.resolvedTalents = 2;
        inputs.psychAvailable = 0;
        inputs.poolSize = 5;
        const PopCompositionResult_t result = composition.Calculate(inputs);

        CHECK(result.expectedTalents == 2);
        CHECK(result.psychSpent == 0);
    }

    SECTION("psych below a step's cost buys nothing")
    {
        PopCompositionInputs_t inputs;
        inputs.dronePressure = 2;
        inputs.psychAvailable = 1;
        inputs.poolSize = 4;
        const PopCompositionResult_t result = composition.Calculate(inputs);

        CHECK(result.psychSpent == 0);
        CHECK(SeatedCount_(result, "Drone") == 2);
    }
}

TEST_CASE("Composition is stable across repeated recalculation within a turn",
          "[population][composition][psych]")
{
    actest::PopRulesFixture reg;
    CompositionFixture_ calc;
    PopCompositionCalculator composition(calc.config, reg.popTypes);

    PopCompositionInputs_t inputs;
    inputs.dronePressure = 4;
    inputs.psychAvailable = 4;
    inputs.poolSize = 6;

    // Psych is read, never consumed, so the same inputs give the same answer every time. A
    // draining read here is what would make composition flap inside a single turn.
    const PopCompositionResult_t first = composition.Calculate(inputs);
    for (int pass = 0; pass < 5; ++pass)
    {
        const PopCompositionResult_t again = composition.Calculate(inputs);
        CHECK(again.expectedTalents == first.expectedTalents);
        CHECK(again.psychSpent == first.psychSpent);
        CHECK(SeatedBodies_(again) == SeatedBodies_(first));
    }
}

TEST_CASE("Seating prefers the lightest types across arbitrary weight tiers",
          "[population][composition]")
{
    // A third drone tier, which the shipping config does not have. {1,2,3} with 3 bodies and 5
    // pressure must land on {2,2,1}: cheapest upgrade first, never the heaviest type it could
    // reach.
    PopTypeRegistry registry = LoadPopTypes_("ac_pop_types_three_tiers.json", R"([
  { "id": "Worker", "name": "Worker", "display_glyph": "W", "is_default": true,
    "can_work_tile": true, "player_assignable": true,
    "psych_to_promote": 2, "promotes_to": "Talent" },
  { "id": "Talent", "name": "Talent", "display_glyph": "A", "can_work_tile": true },
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Worker" },
  { "id": "SuperDrone", "name": "Super Drone", "display_glyph": "S", "can_work_tile": true,
    "drone_weight": 2, "psych_to_promote": 2, "promotes_to": "Drone" },
  { "id": "MegaDrone", "name": "Mega Drone", "display_glyph": "M", "can_work_tile": true,
    "drone_weight": 3, "psych_to_promote": 2, "promotes_to": "SuperDrone" }
])");

    CompositionFixture_ calc;
    PopCompositionCalculator composition(calc.config, registry);

    PopCompositionInputs_t inputs;
    inputs.dronePressure = 5;
    inputs.poolSize = 3;
    const PopCompositionResult_t result = composition.Calculate(inputs);

    CHECK(SeatedCount_(result, "SuperDrone") == 2);
    CHECK(SeatedCount_(result, "Drone") == 1);
    CHECK(SeatedCount_(result, "MegaDrone") == 0);
    CHECK(result.droppedPressure == 0);
}

TEST_CASE("Non-contiguous weights strand a remainder rather than inventing pressure",
          "[population][composition]")
{
    // Weights {1,3}: two bodies and three pressure reaches {1,1} and stops, because the only
    // upgrade costs 2 and just 1 pressure remains. Seating {3} would fit exactly but is a
    // heavier type than the rule prefers, and overshooting is never allowed.
    PopTypeRegistry registry = LoadPopTypes_("ac_pop_types_gap_tiers.json", R"([
  { "id": "Worker", "name": "Worker", "display_glyph": "W", "is_default": true,
    "can_work_tile": true, "player_assignable": true,
    "psych_to_promote": 2, "promotes_to": "Talent" },
  { "id": "Talent", "name": "Talent", "display_glyph": "A", "can_work_tile": true },
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Worker" },
  { "id": "SuperDrone", "name": "Super Drone", "display_glyph": "S", "can_work_tile": true,
    "drone_weight": 3, "psych_to_promote": 2, "promotes_to": "Drone" }
])");

    CompositionFixture_ calc;
    PopCompositionCalculator composition(calc.config, registry);

    PopCompositionInputs_t inputs;
    inputs.dronePressure = 3;
    inputs.poolSize = 2;
    const PopCompositionResult_t result = composition.Calculate(inputs);

    CHECK(SeatedCount_(result, "Drone") == 2);
    CHECK(SeatedCount_(result, "SuperDrone") == 0);
    CHECK(result.droppedPressure == 1);
}

TEST_CASE("The promotion graph must stay classifiable", "[population][composition][config]")
{
    SECTION("promotes_to must name a known type")
    {
        CHECK_THROWS_WITH(LoadPopTypes_("ac_pop_types_bad_target.json", std::string(k_ChainPrologue)
                              + R"(,
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Nonexistent" }
])"),
                          ContainsSubstring("is not a known pop type"));
    }

    SECTION("psych_to_promote and promotes_to must be set together")
    {
        CHECK_THROWS_WITH(LoadPopTypes_("ac_pop_types_unpaired.json", std::string(k_ChainPrologue)
                              + R"(,
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 1, "promotes_to": "Worker" }
])"),
                          ContainsSubstring("must be set together"));
    }

    SECTION("a cycle is rejected")
    {
        CHECK_THROWS_WITH(LoadPopTypes_("ac_pop_types_cycle.json", R"([
  { "id": "Worker", "name": "Worker", "display_glyph": "W", "is_default": true,
    "can_work_tile": true, "player_assignable": true,
    "psych_to_promote": 2, "promotes_to": "Drone" },
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Worker" }
])"),
                          ContainsSubstring("promotion cycle"));
    }

    SECTION("a branch is rejected, because the classes are undefined on one")
    {
        CHECK_THROWS_WITH(LoadPopTypes_("ac_pop_types_branch.json", std::string(k_ChainPrologue)
                              + R"(,
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Worker" },
  { "id": "OtherDrone", "name": "Other Drone", "display_glyph": "O", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Worker" }
])"),
                          ContainsSubstring("single chain"));
    }

    SECTION("a chain disconnected from is_default is rejected")
    {
        CHECK_THROWS_WITH(LoadPopTypes_("ac_pop_types_island.json", std::string(k_ChainPrologue)
                              + R"(,
  { "id": "Loner", "name": "Loner", "display_glyph": "L", "can_work_tile": true,
    "psych_to_promote": 2, "promotes_to": "Stranger" },
  { "id": "Stranger", "name": "Stranger", "display_glyph": "G", "can_work_tile": true }
])"),
                          ContainsSubstring("single chain"));
    }

    SECTION("drone-class types must declare a positive drone_weight")
    {
        CHECK_THROWS_WITH(LoadPopTypes_("ac_pop_types_no_weight.json", std::string(k_ChainPrologue)
                              + R"(,
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "psych_to_promote": 2, "promotes_to": "Worker" }
])"),
                          ContainsSubstring("positive drone_weight"));
    }

    SECTION("non-drone types must not declare drone_weight")
    {
        CHECK_THROWS_WITH(LoadPopTypes_("ac_pop_types_talent_weight.json", R"([
  { "id": "Worker", "name": "Worker", "display_glyph": "W", "is_default": true,
    "can_work_tile": true, "player_assignable": true,
    "psych_to_promote": 2, "promotes_to": "Talent" },
  { "id": "Talent", "name": "Talent", "display_glyph": "A", "can_work_tile": true,
    "drone_weight": 1 },
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Worker" }
])"),
                          ContainsSubstring("only valid on drone-class"));
    }

    SECTION("drone_weight must be unique across drone-class types")
    {
        CHECK_THROWS_WITH(LoadPopTypes_("ac_pop_types_dup_weight.json", std::string(k_ChainPrologue)
                              + R"(,
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Worker" },
  { "id": "SuperDrone", "name": "Super Drone", "display_glyph": "S", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Drone" }
])"),
                          ContainsSubstring("share drone_weight"));
    }

    SECTION("drone_weight must strictly decrease toward is_default")
    {
        CHECK_THROWS_WITH(LoadPopTypes_("ac_pop_types_inverted_weight.json",
                              std::string(k_ChainPrologue) + R"(,
  { "id": "Drone", "name": "Drone", "display_glyph": "R", "can_work_tile": true,
    "drone_weight": 2, "psych_to_promote": 2, "promotes_to": "Worker" },
  { "id": "SuperDrone", "name": "Super Drone", "display_glyph": "S", "can_work_tile": true,
    "drone_weight": 1, "psych_to_promote": 2, "promotes_to": "Drone" }
])"),
                          ContainsSubstring("strictly decrease"));
    }
}

TEST_CASE("Every conversion path resolves the obsolescence chain", "[population][composition]")
{
    actest::BaseFixture fixture;
    fixture.popTypes().Load(actest::FixturePath("pop_types_obsolescence.json"));
    ac::PopulationManager& pops = fixture.MakeBase(4, 4, /*initialPopulation*/ 1).GetPopulation();
    Pop& rPop = *pops.Pops().begin();

    pops.ConvertTo(rPop, "Technician");
    CHECK(std::string(rPop.GetPopType()) == "Technician");

    fixture.pOwnerFaction->GetResearch().AddDiscoveredTech("advanced_build");
    pops.ConvertTo(rPop, "Technician");
    CHECK(std::string(rPop.GetPopType()) == "Engineer");
}

TEST_CASE("Pop composition config requires type ids and per-source formulas",
          "[population][composition]")
{
    PopCompositionConfigParser parser;

    const PopCompositionConfig_t config =
        parser.ParseConfig(actest::FixturePath("pop_composition.json"));
    CHECK(config.droneTypeId == "Drone");
    CHECK(config.talentTypeId == "Talent");
    CHECK(config.riotThreshold == 1);
    CHECK(config.goldenAgeThreshold == 0);

    CHECK_THROWS_WITH(parser.ParseConfig(actest::FixturePath("pop_composition_missing_types.json")),
                      ContainsSubstring("drone_type"));

    CHECK_THROWS_WITH(
        parser.ParseConfig(actest::FixturePath("pop_composition_missing_thresholds.json")),
        ContainsSubstring("riot_threshold"));
}

TEST_CASE("Each drone source is computed on its own formula", "[population][composition]")
{
    LuaRuntime lua;
    const TempConfigFile compositionConfig("ac_comp_terms.json", R"cfg({
  "bureaucracy_limit_formula": "4",
  "bureaucracy_drone_formula": "max(0, floor((residue + faction_base_count - bureaucracy_limit) / bureaucracy_limit))",
  "size_drone_formula": "max(0, base_size - size_free_drones)",
  "occupation_drone_formula": "assimilation_peak > 0 and 2 or 0",
  "assimilation_drones": 5,
  "assimilation_decay_turns": 10,
  "drone_type": "Drone",
  "talent_type": "Talent",
  "riot_threshold": 1,
  "golden_age_threshold": 0,
  "rebel_selection": {
    "distance_mode": "none",
    "fade_radius": 8,
    "distance_weight_per_tile": 1,
    "base_join_weight": 1,
    "missing_hq_distance": 12
  }
})cfg");

    PopCompositionConfig_t config =
        PopCompositionConfigParser{}.ParseConfig(compositionConfig.Path());
    DroneCalculator calculator(config, lua);

    SizeDroneInputs_t size;
    size.baseSize = 7;
    size.sizeFreeDrones = 4;
    CHECK(calculator.CalculateSizeDrones(size) == 3);

    size.sizeFreeDrones = 9;
    CHECK(calculator.CalculateSizeDrones(size) == 0);

    OccupationDroneInputs_t occupation;
    occupation.assimilationPeak = 0;
    CHECK(calculator.CalculateOccupationDrones(occupation) == 0);
    occupation.assimilationPeak = 3;
    CHECK(calculator.CalculateOccupationDrones(occupation) == 2);

    // One base cannot exceed the limit, so bureaucracy contributes nothing yet.
    BureaucracyDroneInputs_t bureaucracy;
    bureaucracy.bureaucracy = 1.0;
    bureaucracy.mapWidth = 80;
    bureaucracy.mapHeight = 40;
    bureaucracy.baseId = 1;
    bureaucracy.factionBaseCount = 1;
    CHECK(calculator.CalculateBureaucracyDrones(bureaucracy) == 0);
    CHECK(calculator.CalculateBureaucracyLimit(bureaucracy) == 4);
}

TEST_CASE("Completing a drone-reducing building reapplies composition immediately",
          "[population][composition][production]")
{
    actest::FactionFixture fixtures;
    ac::GameSettings settings;
    auto pMap = std::make_unique<ac::WorldMap>(9, 9);
    for (auto& pTile : pMap->GetTiles())
    {
        pTile->SetElevation(100);
    }
    auto pState = std::make_unique<ac::GameState>(
        std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
        *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules,
        actest::k_TestRngSeed);
    ac::Faction& rFaction = pState->AddFaction(std::make_unique<ac::Faction>(
        pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
        pState->GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed));
    ac::BaseManager* pBase = rFaction.CreateBase(
        pState->AllocateBaseId(), "TestBase", pState->GetWorldMap().GetTile(4, 4),
        fixtures.dataContext, pState->GetTileEffects(), pState->GetSecretProjectAvailability());
    REQUIRE(pBase != nullptr);

    pBase->GetBuildingManager().AddBuilding("drone_hall");
    pBase->GetPopulation().RecalculateComposition();
    REQUIRE(pBase->GetPopulation().GetDroneCount() == 2);

    const ac::BuildingConfig_t* pCommons = fixtures.buildings().Find("Recreation_Commons");
    REQUIRE(pCommons != nullptr);
    pBase->GetProduction().SetProduction(pCommons, pBase->GetBaseEffects());
    pBase->GetProduction().SetMineralStockpile(pBase->GetMineralCost());

    const ac::ProductionApplyResult_t applied = pBase->ApplyProduction();
    REQUIRE(applied.kind == ac::ProductionApplyKind_t::Completed);
    REQUIRE(pBase->GetBuildingManager().HasBuilding("Recreation_Commons"));

    pBase->GetPopulation().EnsureCompositionCurrent();
    CHECK(pBase->GetPopulation().GetDroneCount() == 0);
}
TEST_CASE("Committed riot applies disable_production and resource clamps", "[riot][economy]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    PopulationManager& pops = base.GetPopulation();

    const BuildingConfig_t* pCommons = fixture.buildings().Find("Recreation_Commons");
    REQUIRE(pCommons != nullptr);
    base.GetProduction().SetProduction(pCommons, base.GetBaseEffects());
    const int cost = base.GetMineralCost();
    REQUIRE(cost > 0);
    base.GetProduction().SetMineralStockpile(cost);

    pops.ForceRiot(/*turns=*/3);
    pops.CommitMood();
    REQUIRE(pops.IsRioting());
    REQUIRE(pops.GetConsecutiveRiotTurns() == 1);
    REQUIRE(ResolveFlag(base, RuleFlagId_t::DisableProduction));

    // Stockpile freezes: completion skipped, minerals preserved.
    CHECK(base.GetProduction().GetMineralStockpile() == cost);
    CHECK(base.ApplyProduction().kind == ProductionApplyKind_t::InProgress);
    CHECK(base.GetProduction().GetMineralStockpile() == cost);

    faction.GetEconomy().AddEnergy(100);
    CHECK_THROWS_WITH(base.HurryProduction(10), Catch::Matchers::ContainsSubstring("disabled"));

    // Leftover mineral bank is discarded — no BankProduction under disable_production.
    if (!base.GetBuildingManager().HasBuilding("mineral_cache"))
    {
        base.GetBuildingManager().AddBuilding("mineral_cache");
    }
    base.ProduceResources();
    REQUIRE(base.GetResources().GetMineralBank() > 0);
    const int stockpileBefore = base.GetProduction().GetMineralStockpile();
    base.ConvertMinerals();
    CHECK(base.GetProduction().GetMineralStockpile() == stockpileBefore);
    CHECK(base.GetResources().GetMineralBank() == 0);

    CHECK(base.GetLabsProduction() == 0);
    CHECK(base.GetEconProduction() <= std::max(0, base.GetBuildingUpkeep()));
}

TEST_CASE("Riot mood state survives snapshot restore", "[riot][save]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4, /*initialPopulation*/ 3);
    base.GetPopulation().ForceRiot(2);
    base.GetPopulation().CommitMood();
    base.GetPopulation().CommitMood();
    REQUIRE(base.GetPopulation().IsRioting());
    REQUIRE(base.GetPopulation().GetConsecutiveRiotTurns() == 2);
    REQUIRE(base.GetPopulation().CaptureMoodState().forcedRiotTurnsRemaining == 0);

    const BaseSnapshot_t snapshot = base.CaptureSnapshot();
    CHECK(snapshot.mood.bRioting);
    CHECK(snapshot.mood.consecutiveRiotTurns == 2);

    base.GetPopulation().ResetMoodEscalation();
    CHECK_FALSE(base.GetPopulation().IsRioting());
    CHECK(base.GetPopulation().GetConsecutiveRiotTurns() == 0);

    base.GetPopulation().RestoreMoodState(snapshot.mood);
    CHECK(base.GetPopulation().IsRioting());
    CHECK(base.GetPopulation().GetConsecutiveRiotTurns() == 2);
}

TEST_CASE("Ownership transfer restarts the riot escalation ladder", "[riot][transfer]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& receiver = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(owner, 4, 4);

    base.GetPopulation().ForceRiot(/*turns=*/3);
    base.GetPopulation().CommitMood();
    base.GetPopulation().CommitMood();
    base.GetPopulation().CommitMood();
    REQUIRE(base.GetPopulation().IsRioting());
    REQUIRE(base.GetPopulation().GetConsecutiveRiotTurns() == 3);

    const BaseId_t baseId = base.GetBaseId();
    owner.TransferBaseTo(baseId, receiver);

    // A base that rebelled at the top tier must not arrive at its new owner still standing
    // there: the next Mood commit would re-fire Rebel and pass it straight on again.
    BaseManager* pMoved = receiver.FindBase(baseId);
    REQUIRE(pMoved != nullptr);
    CHECK_FALSE(pMoved->GetPopulation().IsRioting());
    CHECK(pMoved->GetPopulation().GetConsecutiveRiotTurns() == 0);
    CHECK(pMoved->GetPopulation().CaptureMoodState().forcedRiotTurnsRemaining == 0);
}
