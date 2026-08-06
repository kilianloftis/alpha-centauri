// Pop composition: config-driven drone/talent type ids and centralized role predicates.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/faction/base/population/PopContainer.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
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
    actest::PopRulesFixture reg;
    PopContainer container(reg.popTypes, *reg.availability, *reg.research, /*initialSize*/ 0);
    container.AddPop("Worker");
    container.AddPop("Worker");
    container.AddPop("Drone");
    container.AddPop("Worker");

    PopCompositionResult targets;
    targets.targetDrones = 2;
    targets.targetTalents = 1;

    container.ApplyCompositionTargets(targets, "Worker", "Drone", "Talent");

    CHECK(container.GetSize() == 4);
    CHECK(container.GetDroneCount() == 2);
    CHECK(container.GetTalentCount() == 1);
    // One plain worker remains; GetWorkerCount includes drones/talents (can-work, non-specialist).
    CHECK(container.GetWorkerCount() == 4);
    int plainWorkers = 0;
    for (const Pop& rPop : container.Pops())
    {
        if (rPop.IsPlainWorker())
        {
            ++plainWorkers;
        }
    }
    CHECK(plainWorkers == 1);
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
