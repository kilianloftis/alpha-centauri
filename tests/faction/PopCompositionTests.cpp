// Pop composition: config-driven drone/talent type ids and centralized role predicates.

#include "GameFixtures.h"
#include "TempConfigFile.h"
#include "TestHelpers.h"

#include "game/faction/base/population/PopContainer.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/ResearchManager.h"
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
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

using actest::TempConfigFile;

using namespace ac;
using Catch::Matchers::ContainsSubstring;

namespace
{

struct PopCalcFixture_
{
    LuaRuntime lua;
    PopCompositionConfig_t config =
        PopCompositionConfigParser{}.ParseConfig(actest::FixturePath("pop_composition.json"));
    DroneCalculator droneCalculator{config, lua};
    PopCompositionCalculator compositionCalculator{config, lua};
};

} // namespace

TEST_CASE("Pop role predicates: drone, talent, plain worker, specialist", "[population][composition]")
{
    actest::PopTypeRegistryOnly reg;

    const Pop worker(reg.popTypes.Get("Worker"));
    CHECK(worker.IsPlainWorker());
    CHECK_FALSE(worker.IsDrone());
    CHECK_FALSE(worker.IsTalent());
    CHECK_FALSE(worker.IsSpecialist());
    CHECK(worker.GetRiotContribution() == 0);

    const Pop drone(reg.popTypes.Get("Drone"));
    CHECK(drone.IsDrone());
    CHECK(drone.IsWorker());
    CHECK_FALSE(drone.IsPlainWorker());
    CHECK_FALSE(drone.IsTalent());
    CHECK(drone.GetRiotContribution() == 1);

    const Pop superDrone(reg.popTypes.Get("SuperDrone"));
    CHECK(superDrone.IsDrone());
    CHECK(superDrone.GetRiotContribution() == 2);

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

TEST_CASE("Super Drone counts as two drones for riot weight", "[population][riot]")
{
    actest::PopRulesFixture reg;
    PopCalcFixture_ calc;
    GrowthConfig_t growth;

    PopulationManager pops(reg.popTypes, *reg.availability, growth, calc.droneCalculator,
                           calc.compositionCalculator, *reg.research, /*initialSize*/ 0);
    pops.AddPop("SuperDrone");
    pops.AddPop("Drone");

    CHECK(pops.GetDroneCount() == 2);
    CHECK(pops.GetRiotContribution() == 3);
}

TEST_CASE("ApplyCompositionTargets uses configured type ids and skips existing drones",
          "[population][composition]")
{
    actest::PopRulesFixture reg;
    PopCalcFixture_ calc;
    GrowthConfig_t growth;

    PopulationManager pops(reg.popTypes, *reg.availability, growth, calc.droneCalculator,
                           calc.compositionCalculator, *reg.research, /*initialSize*/ 0);
    pops.AddPop("Worker");
    pops.AddPop("Worker");
    pops.AddPop("Drone");
    pops.AddPop("Worker");

    PopCompositionResult_t targets;
    targets.targetDrones = 2;
    targets.targetTalents = 1;

    pops.ApplyCompositionTargets(targets, calc.config.droneTypeId, calc.config.talentTypeId);

    CHECK(pops.GetSize() == 4);
    CHECK(pops.GetDroneCount() == 2);
    CHECK(pops.GetTalentCount() == 1);
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
    actest::PopRulesFixture reg;
    PopCalcFixture_ calc;
    GrowthConfig_t growth;

    PopulationManager pops(reg.popTypes, *reg.availability, growth, calc.droneCalculator,
                           calc.compositionCalculator, *reg.research, /*initialSize*/ 0);
    pops.AddPop("Talent");
    pops.AddPop("Talent");
    pops.AddPop("Worker");

    REQUIRE(pops.GetTalentCount() == 2);
    REQUIRE(pops.GetDroneCount() == 0);
    REQUIRE(pops.GetWorkerCount() == 3);
    REQUIRE(pops.GetPlainWorkerCount() == 1);

    pops.CheckGoldenAgeEndOfTurn();
    CHECK(pops.IsInGoldenAge());
}

TEST_CASE("Every conversion path resolves the obsolescence chain", "[population][composition]")
{
    PopTypeRegistry registry;
    registry.Load(actest::FixturePath("pop_types_obsolescence.json"));

    TechRegistry techs;
    techs.Load(actest::FixturePath("techs.json"));
    PopCalcFixture_ calc;
    TechCostConfig_t techCostConfig{actest::k_TestTechCostFormula};
    TechCostCalculator techCost(techCostConfig, calc.lua);
    ResearchManager research(techs, techCost, /*pEffectsProvider*/ nullptr);
    PopTypeAvailabilityCalculator availability(registry);
    GrowthConfig_t growth;

    PopulationManager pops(registry, availability, growth, calc.droneCalculator,
                           calc.compositionCalculator, research, /*initialSize*/ 1);
    Pop& rPop = *pops.Pops().begin();

    pops.ConvertTo(rPop, "Technician");
    CHECK(std::string(rPop.GetPopType()) == "Technician");

    research.AddDiscoveredTech("advanced_build");
    pops.ConvertTo(rPop, "Technician");
    CHECK(std::string(rPop.GetPopType()) == "Engineer");
}

TEST_CASE("Pop composition config requires type ids and formulas",
          "[population][composition]")
{
    PopCompositionConfigParser parser;

    const PopCompositionConfig_t config =
        parser.ParseConfig(actest::FixturePath("pop_composition.json"));
    CHECK(config.droneTypeId == "Drone");
    CHECK(config.talentTypeId == "Talent");

    CHECK_THROWS_WITH(parser.ParseConfig(actest::FixturePath("pop_composition_missing_types.json")),
                      ContainsSubstring("drone_type"));
}

TEST_CASE("Converting to or from a specialist recalculates composition",
          "[population][composition]")
{
    actest::PopRulesFixture reg;
    LuaRuntime lua;
    PopCompositionConfig_t config =
        PopCompositionConfigParser{}.ParseConfig(actest::FixturePath("pop_composition_psych.json"));
    DroneCalculator droneCalculator{config, lua};
    PopCompositionCalculator compositionCalculator{config, lua};
    GrowthConfig_t growth;

    PopulationManager pops(reg.popTypes, *reg.availability, growth, droneCalculator,
                           compositionCalculator, *reg.research, /*initialSize*/ 4);
    REQUIRE(pops.GetTalentCount() == 0);

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

    pops.ConvertTo(*pWorker, "Technician");
    CHECK(std::string(pWorker->GetPopType()) == "Technician");
    CHECK(pops.GetTalentCount() == 0);

    pops.ConvertTo(*pWorker, "Doctor");
    CHECK(pops.GetTalentCount() == 1);

    pops.ConvertToDefaultPopType(*pWorker);
    CHECK(pops.GetSpecialistCount() == 0);
    CHECK(pops.GetTalentCount() == 0);
}

TEST_CASE("Shrinking a base reconciles composition immediately", "[population][composition]")
{
    actest::PopRulesFixture reg;
    LuaRuntime lua;

    const TempConfigFile compositionConfig("ac_comp_halfdrones.json", R"cfg({
  "bureaucracy_limit_formula": "1",
  "drone_formula": "math.floor(base_size / 2)",
  "drone_type": "Drone",
  "talent_formula": "0",
  "talent_type": "Talent"
})cfg");

    PopCompositionConfig_t composition =
        PopCompositionConfigParser{}.ParseConfig(compositionConfig.Path());
    DroneCalculator droneCalculator{composition, lua};
    PopCompositionCalculator compositionCalculator{composition, lua};
    GrowthConfig_t growth;

    PopulationManager pops(reg.popTypes, *reg.availability, growth, droneCalculator,
                           compositionCalculator, *reg.research, /*initialSize*/ 0);
    for (int i = 0; i < 4; ++i)
    {
        pops.AddPop("Worker");
    }
    pops.RecalculateComposition();
    REQUIRE(pops.GetSize() == 4);
    REQUIRE(pops.GetDroneCount() == 2);

    pops.RemovePop();
    pops.RemovePop();

    CHECK(pops.GetSize() == 2);
    CHECK(pops.GetDroneCount() == 1);
}
