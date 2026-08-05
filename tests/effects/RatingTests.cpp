// Tests for two-level social rating resolution: SocialRatingModifier effects are ordinary
// effects from any source; accumulation runs per base AFTER FilterForBase, so a base's
// effective rating = faction-wide modifiers + its own ThisBase-scoped ones.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/IConstructable.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include "game/effects/ActiveEffect.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::Active;
using Catch::Approx;

TEST_CASE("AccumulateSocialRatings: sums per axis across sources", "[effects][rating]")
{
    actest::EffectPool pool;
    const BaseEffects_t baseEffects{{
        Active(pool.RatingMod(SocialRatingId_t::Growth, 2), "policy"),
        Active(pool.RatingMod(SocialRatingId_t::Growth, 1), "building"),
        Active(pool.RatingMod(SocialRatingId_t::Police, -2), "policy"),
        Active(pool.StatMod(StatId_t::Energy, 1.0), "unrelated"),
    }};

    const auto totals = AccumulateSocialRatings(baseEffects.effects);
    CHECK(totals.at(SocialRatingId_t::Growth) == 3);
    CHECK(totals.at(SocialRatingId_t::Police) == -2);
    CHECK(totals.count(SocialRatingId_t::Economy) == 0);
}

TEST_CASE("ExpandSocialRatingEffects: maps accumulated levels through the rating table",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    actest::EffectPool pool;

    SECTION("a defined level appends its gameplay effects")
    {
        BaseEffects_t baseEffects{{
            Active(pool.RatingMod(SocialRatingId_t::Growth, 2), "policy"),
        }};
        ExpandSocialRatingEffects(baseEffects, fixture.socialRatings());

        // Fixture: growth level 2 -> +1 nutrients (AllOwnerBases).
        CHECK(ResolveStatModifiers(FilterByStatId(baseEffects.effects, StatId_t::Nutrients), 0.0).total == Approx(1.0));
        bool foundRatingSource = false;
        for (const ActiveEffect_t& rEffect : baseEffects.effects)
        {
            if (rEffect.sourceId == "se_rating_growth_2")
            {
                foundRatingSource = true;
            }
        }
        CHECK(foundRatingSource);
    }

    SECTION("totals above the highest configured level clamp to that extreme")
    {
        // Fixture growth levels: {2, 3}. Total 5 clamps to 3 -> +3 nutrients.
        BaseEffects_t baseEffects{{
            Active(pool.RatingMod(SocialRatingId_t::Growth, 5), "policy"),
        }};
        ExpandSocialRatingEffects(baseEffects, fixture.socialRatings());
        CHECK(ResolveStatModifiers(FilterByStatId(baseEffects.effects, StatId_t::Nutrients), 0.0).total == Approx(3.0));

        bool foundClampedSource = false;
        for (const ActiveEffect_t& rEffect : baseEffects.effects)
        {
            if (rEffect.sourceId == "se_rating_growth_3")
            {
                foundClampedSource = true;
            }
        }
        CHECK(foundClampedSource);
    }

    SECTION("totals below the lowest configured level clamp to that extreme")
    {
        // Fixture industry levels: {-1, 1, 2}. Total -5 clamps to -1 -> CostMultiplier +10%.
        BaseEffects_t baseEffects{{
            Active(pool.RatingMod(SocialRatingId_t::Industry, -5), "policy"),
        }};
        ExpandSocialRatingEffects(baseEffects, fixture.socialRatings());
        CHECK(ResolveStatModifiers(FilterByStatId(baseEffects.effects, StatId_t::CostMultiplier), 100.0).total
              == Approx(110.0));
    }

    SECTION("in-range missing levels still produce no effects")
    {
        // Fixture industry levels: {-1, 1, 2}. Total 0 is inside [-1, 2] but unlisted.
        BaseEffects_t baseEffects{{
            Active(pool.RatingMod(SocialRatingId_t::Industry, 1), "policy"),
            Active(pool.RatingMod(SocialRatingId_t::Industry, -1), "malus"),
        }};
        ExpandSocialRatingEffects(baseEffects, fixture.socialRatings());
        CHECK(FilterByStatId(baseEffects.effects, StatId_t::CostMultiplier).empty());
    }

    SECTION("modifiers that cancel to zero produce no effects")
    {
        BaseEffects_t baseEffects{{
            Active(pool.RatingMod(SocialRatingId_t::Growth, 2), "policy"),
            Active(pool.RatingMod(SocialRatingId_t::Growth, -2), "malus"),
        }};
        ExpandSocialRatingEffects(baseEffects, fixture.socialRatings());
        CHECK(FilterByStatId(baseEffects.effects, StatId_t::Nutrients).empty());
    }
}

TEST_CASE("Two-level ratings: faction-wide policy rating plus a base-local building rating",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseWithShrine = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& plainBase = fixture.MakeFactionBase(faction, 6, 6);

    // Policy: +2 Growth faction-wide. Shrine: +1 Growth in its base only.
    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("growth_policy"));
    baseWithShrine.GetBuildingManager().AddBuilding("growth_shrine");

    CHECK(baseWithShrine.GetEffectiveSocialRating(SocialRatingId_t::Growth) == 3);
    CHECK(plainBase.GetEffectiveSocialRating(SocialRatingId_t::Growth) == 2);

    // The rating table maps growth level 3 -> +3 nutrients, level 2 -> +1 nutrients.
    // Worked tiles are all barren, so nutrient production isolates the rating effects.
    faction.ProduceBaseResources();
    CHECK(baseWithShrine.GetNutrientProduction() == 3);
    CHECK(plainBase.GetNutrientProduction() == 1);
}

TEST_CASE("Growth rating affects the growth threshold via GrowthRate modifiers",
          "[effects][rating][growth]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseWithShrine = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& plainBase = fixture.MakeFactionBase(faction, 6, 6);

    // No rating: 3 starting pops * 10 nutrients per pop.
    CHECK(plainBase.GetNutrientsRequired() == 30);

    // Policy: +2 Growth faction-wide (fixture level 2 -> +20% growth rate). Shrine: +1
    // Growth in its base only (level 3 -> +30%). The threshold shrinks per base.
    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("growth_policy"));
    baseWithShrine.GetBuildingManager().AddBuilding("growth_shrine");

    CHECK(plainBase.GetNutrientsRequired() == 25);      // 30 / 1.2
    CHECK(baseWithShrine.GetNutrientsRequired() == 23); // 30 / 1.3
}

TEST_CASE("Industry rating affects production cost via CostMultiplier modifiers",
          "[effects][rating][industry][production]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    struct StubItem : IConstructable
    {
        std::string id = "stub";
        std::string name = "Stub";
        const std::string& GetId() const override { return id; }
        const std::string& GetName() const override { return name; }
        int GetBaseCost() const override { return 10; }
    } item;

    base.GetProduction().SetProduction(&item);
    CHECK(base.GetMineralCost() == 100); // Industry 0 → multiplier 1.0

    // Policy: +2 Industry → level 2 → CostMultiplier -20% → 0.8
    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("industry_policy"));
    CHECK(base.GetEffectiveSocialRating(SocialRatingId_t::Industry) == 2);
    CHECK(base.GetMineralCost() == 80);
}

TEST_CASE("Rating modifiers are honored from any source: a building's FactionGlobal rating",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(faction, 6, 6);

    // No policy involved: the building alone raises the faction-wide Growth score.
    baseA.GetBuildingManager().AddBuilding("faction_growth_shrine"); // +2 Growth, FactionGlobal

    CHECK(baseA.GetEffectiveSocialRating(SocialRatingId_t::Growth) == 2);
    CHECK(baseB.GetEffectiveSocialRating(SocialRatingId_t::Growth) == 2);

    faction.ProduceBaseResources();
    CHECK(baseA.GetNutrientProduction() == 1); // growth level 2 -> +1 nutrients
    CHECK(baseB.GetNutrientProduction() == 1);
}

TEST_CASE("Economy rating minerals AddPercent scales worked minerals",
          "[effects][rating][economy]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    // Pack the workable area with Rocky (+2 minerals each). Base center is worked for free;
    // AutoAssignWorkers places the starting workers on the best remaining tiles.
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            fixture.At(4 + dx, 4 + dy).SetRockiness(Rockiness_t::Rocky);
        }
    }
    base.GetWorkerAssignments().AutoAssignWorkers();

    const int worked = base.GetMineralProduction();
    REQUIRE(worked >= 8);

    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("economy_policy"));
    CHECK(base.GetEffectiveSocialRating(SocialRatingId_t::Economy) == 2);

    const int expected = FinalizeResolvedStat(static_cast<double>(worked) * 0.9);
    REQUIRE(expected < worked);
    CHECK(base.GetMineralProduction() == expected);

    faction.ProduceBaseResources();
    CHECK(base.GetResources().ConsumeMinerals() == expected);
}
