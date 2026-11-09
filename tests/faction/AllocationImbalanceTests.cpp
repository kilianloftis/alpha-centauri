// Labs↔econ slider-imbalance penalty after facility modifiers, gated by Efficiency SE.

#include "GameFixtures.h"

#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/economy/EconomyManager.h"
#include "game/economy/EconomyRules.h"
#include "game/economy/Inefficiency.h"
#include "game/faction/Faction.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("AllocationImbalanceFactorForRating reads the Efficiency table",
          "[economy][allocation][imbalance]")
{
    actest::FactionFixture fixture;
    const SocialRatingRegistry& rRatings = fixture.socialRatings();

    CHECK(AllocationImbalanceFactorForRating(rRatings, -4) == 8);
    CHECK(AllocationImbalanceFactorForRating(rRatings, -3) == 7);
    CHECK(AllocationImbalanceFactorForRating(rRatings, -2) == 6);
    CHECK(AllocationImbalanceFactorForRating(rRatings, -1) == 5);
    CHECK(AllocationImbalanceFactorForRating(rRatings, 0) == 4);
    CHECK(AllocationImbalanceFactorForRating(rRatings, 1) == 3);
    CHECK(AllocationImbalanceFactorForRating(rRatings, 2) == 2);
    CHECK(AllocationImbalanceFactorForRating(rRatings, 3) == 1);
    CHECK(AllocationImbalanceFactorForRating(rRatings, 4) == 0);

    CHECK(AllocationImbalanceFactorForRating(rRatings, 5) == 0);
    CHECK(AllocationImbalanceFactorForRating(rRatings, -5) == 8);
}

TEST_CASE("Balanced labs/econ allocation loses nothing to imbalance",
          "[economy][allocation][imbalance]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 8, 4);
    hq.GetBuildingManager().AddBuilding("Headquarters");
    faction.GetEconomy().SetEnergyAllocation({50, 50, 0});

    const EnergySplit_t split = AllocateBaseEnergy(hq, 100);
    CHECK(split.econ == 50);
    CHECK(split.labs == 50);
    CHECK(split.psych == 0);
}

TEST_CASE("Default 40/50/10 at Efficiency 0 loses nothing to imbalance",
          "[economy][allocation][imbalance]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 8, 4);
    hq.GetBuildingManager().AddBuilding("Headquarters");

    const EnergySplit_t split = AllocateBaseEnergy(hq, 100);
    CHECK(split.econ == 40);
    CHECK(split.labs == 50);
    CHECK(split.psych == 10);
}

TEST_CASE("Labs-heavy allocation at Efficiency 0 cuts labs 8% and econ 16%",
          "[economy][allocation][imbalance]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 8, 4);
    hq.GetBuildingManager().AddBuilding("Headquarters");
    faction.GetEconomy().SetEnergyAllocation({40, 60, 0});

    // Imbalance 1, factor 4, penalty 8; labs > econ → labs −8%, econ −16%.
    const EnergySplit_t split = AllocateBaseEnergy(hq, 100);
    CHECK(split.labs == 55);
    CHECK(split.econ == 34);
    CHECK(split.psych == 0);
}

TEST_CASE("Econ-heavy allocation at Efficiency 0 doubles the labs cut",
          "[economy][allocation][imbalance]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 8, 4);
    hq.GetBuildingManager().AddBuilding("Headquarters");
    faction.GetEconomy().SetEnergyAllocation({60, 40, 0});

    // Same penalty 8; labs ≤ econ → labs −16%, econ −8%.
    const EnergySplit_t split = AllocateBaseEnergy(hq, 100);
    CHECK(split.labs == 34);
    CHECK(split.econ == 55);
    CHECK(split.psych == 0);
}

TEST_CASE("Efficiency +4 nulls the slider imbalance penalty",
          "[economy][allocation][imbalance]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 8, 4);
    hq.GetBuildingManager().AddBuilding("Headquarters");
    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("peak_efficiency_policy"));
    faction.GetEconomy().SetEnergyAllocation({0, 100, 0});
    REQUIRE(faction.GetSocialEngineering().GetSocialRating(SocialRatingId_t::Efficiency) == 4);

    const EnergySplit_t split = AllocateBaseEnergy(hq, 100);
    CHECK(split.labs == 100);
    CHECK(split.econ == 0);
    CHECK(split.psych == 0);
}

TEST_CASE("Efficiency -4 and all-labs caps labs at 40% loss and econ at 80%",
          "[economy][allocation][imbalance]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 8, 4);
    hq.GetBuildingManager().AddBuilding("Headquarters");
    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("collapse_efficiency_policy"));
    faction.GetEconomy().SetEnergyAllocation({0, 100, 0});
    REQUIRE(faction.GetSocialEngineering().GetSocialRating(SocialRatingId_t::Efficiency) == -4);

    // Imbalance 5, factor 8, penalty 80 → labs −40%, econ −80%.
    const EnergySplit_t split = AllocateBaseEnergy(hq, 100);
    CHECK(split.labs == 60);
    CHECK(split.econ == 0);
    CHECK(split.psych == 0);
}

TEST_CASE("All-psych allocation has no labs/econ imbalance penalty",
          "[economy][allocation][imbalance]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 8, 4);
    hq.GetBuildingManager().AddBuilding("Headquarters");
    faction.GetEconomy().SetEnergyAllocation({0, 0, 100});

    const EnergySplit_t split = AllocateBaseEnergy(hq, 100);
    CHECK(split.econ == 0);
    CHECK(split.labs == 0);
    CHECK(split.psych == 100);
}
