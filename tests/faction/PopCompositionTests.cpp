// Pop composition: config-driven drone/talent type ids and centralized role predicates.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/faction/base/population/PopContainer.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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
    actest::PopTypeRegistryOnly reg;
    PopContainer container(&reg.popTypes, nullptr, nullptr, /*initialSize*/ 0);
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
