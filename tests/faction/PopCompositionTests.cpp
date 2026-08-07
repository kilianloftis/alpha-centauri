// Pop composition: config-driven drone/talent type ids and centralized role predicates.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/faction/base/population/PopContainer.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/ResearchManager.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

using namespace ac;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("Pop role predicates: drone, talent, plain worker, specialist", "[population][composition]")
{
    actest::PopTypeRegistryOnly reg;

    const Pop worker(reg.popTypes.Get("Worker"));
    CHECK(worker.IsPlainWorker());
    CHECK_FALSE(worker.IsDrone());
    CHECK_FALSE(worker.IsTalent());
    CHECK_FALSE(worker.IsSpecialist());

    const Pop drone(reg.popTypes.Get("Drone"));
    CHECK(drone.IsDrone());
    CHECK(drone.IsWorker());
    CHECK_FALSE(drone.IsPlainWorker());
    CHECK_FALSE(drone.IsTalent());

    const Pop talent(reg.popTypes.Get("Talent"));
    CHECK(talent.IsTalent());
    CHECK(talent.IsWorker());
    CHECK_FALSE(talent.IsPlainWorker());
    CHECK_FALSE(talent.IsDrone());

    const Pop doctor(reg.popTypes.Get("Doctor"));
    CHECK(doctor.IsSpecialist());
    CHECK_FALSE(doctor.IsPlainWorker());
    CHECK_FALSE(doctor.IsWorker());
}

TEST_CASE("ApplyCompositionTargets uses configured type ids and skips existing drones",
          "[population][composition]")
{
    // Reconciliation is driven through PopulationManager now: PopContainer is storage and owns
    // no policy, so the tech gate cannot apply on one conversion path and not another.
    actest::PopRulesFixture reg;
    LuaRuntime lua;
    PopCompositionConfigParser parser;
    const PopCompositionConfig_t config =
        parser.ParseConfig(actest::FixturePath("pop_composition.lua"), lua);
    PopCompositionCalculator calculator(config, lua);
    GrowthConfig_t growth;

    PopulationManager pops(reg.popTypes, *reg.availability, growth, calculator, *reg.research,
                           /*initialSize*/ 0);
    pops.AddPop("Worker");
    pops.AddPop("Worker");
    pops.AddPop("Drone");
    pops.AddPop("Worker");

    PopCompositionResult_t targets;
    targets.targetDrones = 2;
    targets.targetTalents = 1;

    pops.ApplyCompositionTargets(targets, "Drone", "Talent");

    CHECK(pops.GetSize() == 4);
    CHECK(pops.GetDroneCount() == 2);
    CHECK(pops.GetTalentCount() == 1);
    // One plain worker remains; GetWorkerCount includes drones/talents (can-work, non-specialist).
    CHECK(pops.GetWorkerCount() == 4);
    int plainWorkers = 0;
    for (const Pop& rPop : pops.Pops())
    {
        if (rPop.IsPlainWorker())
        {
            ++plainWorkers;
        }
    }
    CHECK(plainWorkers == 1);
}

TEST_CASE("Golden age counts plain workers, not every tile-capable pop",
          "[population][goldenage]")
{
    // GetWorkerCount() is every tile-capable pop, so drones and talents were counted on both
    // sides of GoldenAgeCalculator's documented "talents >= workers + specialists" rule.
    // Counting talents against themselves made the effective condition "every pop a talent".
    actest::PopRulesFixture reg;
    LuaRuntime lua;
    PopCompositionConfigParser parser;
    const PopCompositionConfig_t config =
        parser.ParseConfig(actest::FixturePath("pop_composition.lua"), lua);
    PopCompositionCalculator calculator(config, lua);
    GrowthConfig_t growth;

    PopulationManager pops(reg.popTypes, *reg.availability, growth, calculator, *reg.research,
                           /*initialSize*/ 0);
    // Two talents and one plain worker, no drones: talents(2) >= workers(1) + specialists(0).
    pops.AddPop("Talent");
    pops.AddPop("Talent");
    pops.AddPop("Worker");

    REQUIRE(pops.GetTalentCount() == 2);
    REQUIRE(pops.GetDroneCount() == 0);
    // The distinction the fix rests on: three tile-capable pops, one of them a plain worker.
    REQUIRE(pops.GetWorkerCount() == 3);
    REQUIRE(pops.GetPlainWorkerCount() == 1);

    pops.CheckGoldenAgeEndOfTurn();
    // With GetWorkerCount() the test would be talents(2) >= 3 + 0 — false, no golden age.
    CHECK(pops.IsInGoldenAge());
}

TEST_CASE("Every conversion path resolves the obsolescence chain", "[population][composition]")
{
    // The defect this package's [H] describes: PopContainer held the availability calculator
    // and applied it in ConvertToFallback but not in ConvertTo, so ConvertTo could seat a pop
    // type the fallback path would have refused — the tech gate existed on one path only.
    // Both now go through PopulationManager::ResolveType_.
    PopTypeRegistry registry;
    registry.Load(actest::FixturePath("pop_types_obsolescence.json"));

    TechRegistry techs;
    techs.Load(actest::FixturePath("techs.json"));
    LuaRuntime lua;
    TechCostConfig_t techCostConfig{actest::k_TestTechCostFormula};
    TechCostCalculator techCost(techCostConfig, lua);
    ResearchManager research(techs, techCost, /*pEffectsProvider*/ nullptr);
    PopTypeAvailabilityCalculator availability(registry);

    PopCompositionConfigParser parser;
    const PopCompositionConfig_t config =
        parser.ParseConfig(actest::FixturePath("pop_composition.lua"), lua);
    PopCompositionCalculator calculator(config, lua);
    GrowthConfig_t growth;

    PopulationManager pops(registry, availability, growth, calculator, research,
                           /*initialSize*/ 1);
    Pop& rPop = *pops.Pops().begin();

    // Before the tech, Engineer does not exist yet, so Technician is current.
    pops.ConvertTo(rPop, "Technician");
    CHECK(std::string(rPop.GetPopType()) == "Technician");

    // Discovering the tech makes Engineer obsolete Technician. Asking for a Technician now
    // yields an Engineer — the same resolution ConvertToFallback always performed.
    research.AddDiscoveredTech("advanced_build");
    pops.ConvertTo(rPop, "Technician");
    CHECK(std::string(rPop.GetPopType()) == "Engineer");
}

TEST_CASE("PopCompositionConfigParser requires drone_type and talent_type",
          "[population][composition]")
{
    LuaRuntime lua;
    PopCompositionConfigParser parser;

    const PopCompositionConfig_t config =
        parser.ParseConfig(actest::FixturePath("pop_composition.lua"), lua);
    CHECK(config.droneTypeId == "Drone");
    CHECK(config.talentTypeId == "Talent");

    // Missing type keys must fail at parse time, not at ConvertTo.
    CHECK_THROWS_WITH(
        parser.ParseConfig(actest::FixturePath("pop_composition_missing_types.lua"), lua),
        ContainsSubstring("drone_type"));
}

TEST_CASE("Converting to or from a specialist recalculates composition",
          "[population][composition]")
{
    actest::PopRulesFixture reg;
    LuaRuntime lua;
    PopCompositionConfigParser parser;
    const PopCompositionConfig_t config =
        parser.ParseConfig(actest::FixturePath("pop_composition_psych.lua"), lua);
    PopCompositionCalculator calculator(config, lua);
    GrowthConfig_t growth;

    PopulationManager pops(reg.popTypes, *reg.availability, growth, calculator, *reg.research,
                           /*initialSize*/ 4);
    REQUIRE(pops.GetTalentCount() == 0);

    // Doctor contributes +2 psych → talent_formula floor(2/2) = 1 talent.
    Pop* pWorker = nullptr;
    for (Pop& rPop : pops.Pops())
    {
        if (rPop.IsPlainWorker())
        {
            pWorker = &rPop;
            break;
        }
    }
    REQUIRE(pWorker != nullptr);
    pops.ConvertTo(*pWorker, "Doctor");
    CHECK(pops.GetSpecialistCount() == 1);
    CHECK(pops.GetTalentCount() == 1);

    // Switching to a non-psych specialist drops the talent target immediately.
    pops.ConvertTo(*pWorker, "Technician");
    CHECK(std::string(pWorker->GetPopType()) == "Technician");
    CHECK(pops.GetTalentCount() == 0);

    // Restoring a Doctor brings the talent back.
    pops.ConvertTo(*pWorker, "Doctor");
    CHECK(pops.GetTalentCount() == 1);

    // Demoting the specialist clears psych-driven talents.
    pops.ConvertToDefaultPopType(*pWorker);
    CHECK(pops.GetSpecialistCount() == 0);
    CHECK(pops.GetTalentCount() == 0);
}
