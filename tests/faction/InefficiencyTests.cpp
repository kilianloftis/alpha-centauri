// Energy inefficiency: HQ tabletop-diagonal distance × Efficiency SE rating, applied
// before the econ/labs/psych split.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/Inefficiency.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/map/MapUtils.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("TabletopDiagonalDistance is longer + shorter/2 with X wrap",
          "[economy][inefficiency][map]")
{
    actest::WorldFixture world(16, 16);

    // Pure axis: shorter = 0 → distance equals the longer leg.
    CHECK(TabletopDiagonalDistance(world.At(0, 0), world.At(4, 0), 16) == 4);
    CHECK(TabletopDiagonalDistance(world.At(0, 0), world.At(0, 5), 16) == 5);

    // Equal legs: longer + floor(shorter/2) = 3 + 1 = 4 (Chebyshev would be 3).
    CHECK(TabletopDiagonalDistance(world.At(0, 0), world.At(3, 3), 16) == 4);

    // Unequal: dx=4, dy=2 → 4 + 1 = 5.
    CHECK(TabletopDiagonalDistance(world.At(0, 0), world.At(4, 2), 16) == 5);

    // Odd shorter floor: dx=5, dy=1 → 5 + 0 = 5.
    CHECK(TabletopDiagonalDistance(world.At(0, 0), world.At(5, 1), 16) == 5);

    // Horizontal wrap: (0,0) to (15,0) is one step west, not 15 east.
    CHECK(TabletopDiagonalDistance(world.At(0, 0), world.At(15, 0), 16) == 1);
}

TEST_CASE("CalculateInefficiencyLoss uses the configured denominators",
          "[economy][inefficiency]")
{
    constexpr int energy = 64;
    constexpr int distance = 16;

    // Percent lost = Distance / denominator; loss = Energy * Distance / denom.
    CHECK(CalculateInefficiencyLoss(energy, distance, /*denom=*/64) == 16);
    CHECK(CalculateInefficiencyLoss(energy, distance, /*denom=*/56) == 18);
    CHECK(CalculateInefficiencyLoss(energy, distance, /*denom=*/48) == 21);
    CHECK(CalculateInefficiencyLoss(energy, distance, /*denom=*/40) == 25);
    CHECK(CalculateInefficiencyLoss(energy, distance, /*denom=*/32) == 32);
    CHECK(CalculateInefficiencyLoss(energy, distance, /*denom=*/24) == 42);
    CHECK(CalculateInefficiencyLoss(energy, distance, /*denom=*/16) == 64);
    CHECK(CalculateInefficiencyLoss(energy, distance, /*denom=*/8) == 64);  // capped
    CHECK(CalculateInefficiencyLoss(energy, distance, /*denom=*/0) == 64);  // 100%

    CHECK(CalculateInefficiencyLoss(energy, /*distance=*/0, 32) == 0);
    CHECK(CalculateInefficiencyLoss(10, k_DefaultInefficiencyHqDistance, 32) == 5);
}

TEST_CASE("InefficiencyDenominatorForRating reads social_rating_effects.json table",
          "[economy][inefficiency][rating]")
{
    actest::FactionFixture fixture;
    const SocialRatingRegistry& rRatings = fixture.socialRatings();

    CHECK(InefficiencyDenominatorForRating(rRatings, 4) == 64);
    CHECK(InefficiencyDenominatorForRating(rRatings, 3) == 56);
    CHECK(InefficiencyDenominatorForRating(rRatings, 2) == 48);
    CHECK(InefficiencyDenominatorForRating(rRatings, 1) == 40);
    CHECK(InefficiencyDenominatorForRating(rRatings, 0) == 32);
    CHECK(InefficiencyDenominatorForRating(rRatings, -1) == 24);
    CHECK(InefficiencyDenominatorForRating(rRatings, -2) == 16);
    CHECK(InefficiencyDenominatorForRating(rRatings, -3) == 8);
    CHECK(InefficiencyDenominatorForRating(rRatings, -4) == 0);

    // Out-of-range totals clamp to the extreme configured levels.
    CHECK(InefficiencyDenominatorForRating(rRatings, 5) == 64);
    CHECK(InefficiencyDenominatorForRating(rRatings, -5) == 0);
}

TEST_CASE("HQ base loses no energy to inefficiency regardless of Efficiency rating",
          "[economy][inefficiency]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& remote = fixture.MakeFactionBase(faction, 6, 2);

    hq.GetBuildingManager().AddBuilding("Headquarters");
    hq.GetBuildingManager().AddBuilding("energy_tap");
    remote.GetBuildingManager().AddBuilding("energy_tap");

    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("collapse_efficiency_policy"));
    REQUIRE(hq.GetEffectiveSocialRating(SocialRatingId_t::Efficiency) == -4);
    REQUIRE(remote.GetEffectiveSocialRating(SocialRatingId_t::Efficiency) == -4);

    // +10 energy: HQ keeps all of it; remote at -4 loses 100%.
    CHECK(hq.GetEconProduction() + hq.GetLabsProduction() + hq.GetPsychProduction() == 10);
    CHECK(remote.GetEconProduction() + remote.GetLabsProduction() + remote.GetPsychProduction()
          == 0);
}

TEST_CASE("No headquarters uses Distance 16 for inefficiency", "[economy][inefficiency]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    base.GetBuildingManager().AddBuilding("energy_tap");

    REQUIRE(faction.GetHeadquarters() == nullptr);
    // Efficiency 0, Distance 16 → loss = 10*16/32 = 5 → 5 allocatable → econ 3, labs 2, psych 0.
    CHECK(base.GetEconProduction() == 3);
    CHECK(base.GetLabsProduction() == 2);
    CHECK(base.GetPsychProduction() == 0);
}

TEST_CASE("Efficiency SE rating changes remote-base inefficiency loss",
          "[economy][inefficiency][rating]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    // Fixture map is 9 wide; stay inside the non-wrapping half so distance is literal dx.
    BaseManager& hq = fixture.MakeFactionBase(faction, 2, 2);
    // Tabletop distance: dx=4, dy=0 → 4.
    BaseManager& remote = fixture.MakeFactionBase(faction, 6, 2);

    hq.GetBuildingManager().AddBuilding("Headquarters");
    remote.GetBuildingManager().AddBuilding("energy_tap");

    REQUIRE(TabletopDiagonalDistance(hq.GetTile(), remote.GetTile(), fixture.map.GetWidth())
            == 4);

    // Efficiency 0: loss = 10*4/32 = 1 → 9 allocatable → econ 5, labs 4, psych 0.
    CHECK(remote.GetEffectiveSocialRating(SocialRatingId_t::Efficiency) == 0);
    CHECK(remote.GetEconProduction() == 5);
    CHECK(remote.GetLabsProduction() == 4);
    CHECK(remote.GetPsychProduction() == 0);

    // Efficiency +2: denom 48, loss = 10*4/48 = 0 → 10 → econ 4, labs 5, psych 1.
    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("efficiency_policy"));
    REQUIRE(remote.GetEffectiveSocialRating(SocialRatingId_t::Efficiency) == 2);
    CHECK(remote.GetEconProduction() == 4);
    CHECK(remote.GetLabsProduction() == 5);
    CHECK(remote.GetPsychProduction() == 1);

    // Efficiency -2: denom 16, loss = 10*4/16 = 2 → 8 → econ 4, labs 4, psych 0.
    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("low_efficiency_policy"));
    REQUIRE(remote.GetEffectiveSocialRating(SocialRatingId_t::Efficiency) == -2);
    CHECK(remote.GetEconProduction() == 4);
    CHECK(remote.GetLabsProduction() == 4);
    CHECK(remote.GetPsychProduction() == 0);
}
