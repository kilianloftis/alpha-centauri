// Psych / energy: per-base production → inefficiency → allocation → category bonuses.
// Econ/labs are collected by the faction; psych stays local for composition later.
// Also covers drones/talents StatModifiers used by composition later.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/population/pop-types/Pop.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("Energy-psych stays at the producing base; effect psych stacks locally",
          "[economy][psych]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    // Large base (size 5) has no local energy; small base (size 3) has +10 energy.
    // Psych 10% of 10 = 1 stays at the small base — it is not redistributed.
    BaseManager& large = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& small = fixture.MakeFactionBase(faction, 6, 6);
    large.GetPopulation().AddPop();
    large.GetPopulation().AddPop();
    REQUIRE(large.GetPopulation().GetSize() == 5);
    REQUIRE(small.GetPopulation().GetSize() == 3);

    small.GetBuildingManager().AddBuilding("energy_tap");

    CHECK(large.GetPsychProduction() == 0);
    CHECK(small.GetPsychProduction() == 1);

    // Doctor on the small base adds local +2 psych on top of the energy-psych share.
    for (Pop& rPop : small.GetPopulation().Pops())
    {
        if (rPop.IsPlainWorker())
        {
            small.ConvertPop(rPop, "Doctor");
            break;
        }
    }
    CHECK(large.GetPsychProduction() == 0);
    CHECK(small.GetPsychProduction() == 3);

    faction.ProduceBaseResources();
    CHECK(large.GetResources().ConsumePsych() == 0);
    CHECK(small.GetResources().ConsumePsych() == 3);
}

TEST_CASE("Default energy split at a base: 40/50/10 of post-inefficiency energy",
          "[economy][psych]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    // +10 energy, inefficiency stub leaves it intact → econ 4, labs 5, psych 1.
    base.GetBuildingManager().AddBuilding("energy_tap");
    CHECK(base.GetEconProduction() == 4);
    CHECK(base.GetLabsProduction() == 5);
    CHECK(base.GetPsychProduction() == 1);

    faction.ProduceBaseResources();
    CHECK(base.GetResources().ConsumeEcon() == 4);
    CHECK(base.GetResources().ConsumeLabs() == 5);
    CHECK(base.GetResources().ConsumePsych() == 1);

    // Faction collection drains the same stockpiles (re-produce after consume).
    faction.ProduceBaseResources();
    CHECK(faction.CollectIncome() == 4);
    // Labs were not consumed by CollectIncome; still in the stockpile from this produce.
    CHECK(faction.CollectResearch() == 5);
}

TEST_CASE("Drone and talent StatModifiers resolve on bases", "[effects][psych][population]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    CHECK(base.GetDroneModifier() == 0);
    CHECK(base.GetTalentModifier() == 0);

    base.GetBuildingManager().AddBuilding("drone_hall");
    CHECK(base.GetDroneModifier() == 2);

    base.GetBuildingManager().AddBuilding("talent_academy");
    CHECK(base.GetTalentModifier() == 1);

    base.GetBuildingManager().AddBuilding("punishment_sphere");
    CHECK(base.GetTalentModifier() == 0);
}
