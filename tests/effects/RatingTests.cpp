// Tests for two-level social rating resolution: SocialRatingModifier effects are ordinary
// effects from any source; accumulation runs per base AFTER FilterForBase, so a base's
// effective rating = faction-wide modifiers + its own ThisBase-scoped ones.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/social-engineering/SocialRatingResolver.h"
#include "lib/effects/ActiveEffect.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::Active;
using Catch::Approx;

TEST_CASE("AccumulateSocialRatings: sums per axis across sources", "[effects][rating]")
{
    actest::EffectPool pool;
    const BaseEffects_t baseEffects{{
        Active(pool.RatingMod(SocialRatingId::Growth, 2), "policy"),
        Active(pool.RatingMod(SocialRatingId::Growth, 1), "building"),
        Active(pool.RatingMod(SocialRatingId::Police, -2), "policy"),
        Active(pool.StatMod(StatId::Energy, 1.0), "unrelated"),
    }};

    const auto totals = AccumulateSocialRatings(baseEffects);
    CHECK(totals.at(SocialRatingId::Growth) == 3);
    CHECK(totals.at(SocialRatingId::Police) == -2);
    CHECK(totals.count(SocialRatingId::Economy) == 0);
}

TEST_CASE("ExpandSocialRatingEffects: maps accumulated levels through the rating table",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    actest::EffectPool pool;

    SECTION("a defined level appends its gameplay effects")
    {
        BaseEffects_t baseEffects{{
            Active(pool.RatingMod(SocialRatingId::Growth, 2), "policy"),
        }};
        ExpandSocialRatingEffects(baseEffects, fixture.socialRatings());

        // Fixture: growth level 2 -> +1 nutrients (AllOwnerBases).
        CHECK(ResolveStatModifiers(FilterByStatId(baseEffects.effects, StatId::Nutrients), 0.0).total == Approx(1.0));
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

    SECTION("an undefined level produces no effects")
    {
        BaseEffects_t baseEffects{{
            Active(pool.RatingMod(SocialRatingId::Growth, 1), "policy"), // no level 1 in fixture
        }};
        ExpandSocialRatingEffects(baseEffects, fixture.socialRatings());
        CHECK(FilterByStatId(baseEffects.effects, StatId::Nutrients).empty());
    }

    SECTION("modifiers that cancel to zero produce no effects")
    {
        BaseEffects_t baseEffects{{
            Active(pool.RatingMod(SocialRatingId::Growth, 2), "policy"),
            Active(pool.RatingMod(SocialRatingId::Growth, -2), "malus"),
        }};
        ExpandSocialRatingEffects(baseEffects, fixture.socialRatings());
        CHECK(FilterByStatId(baseEffects.effects, StatId::Nutrients).empty());
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
    REQUIRE(faction.SetSocialPolicy(SocialCategory::Politics, "growth_policy"));
    baseWithShrine.AddBuilding("growth_shrine");

    const auto pool = CollectActiveEffects(faction);
    CHECK(baseWithShrine.GetEffectiveSocialRating(SocialRatingId::Growth, pool) == 3);
    CHECK(plainBase.GetEffectiveSocialRating(SocialRatingId::Growth, pool) == 2);

    // The rating table maps growth level 3 -> +3 nutrients, level 2 -> +1 nutrients.
    // Worked tiles are all barren, so nutrient production isolates the rating effects.
    faction.ProduceBaseResources({});
    CHECK(baseWithShrine.GetNutrientProduction() == 3);
    CHECK(plainBase.GetNutrientProduction() == 1);
}

TEST_CASE("Rating modifiers are honored from any source: a building's FactionGlobal rating",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(faction, 6, 6);

    // No policy involved: the building alone raises the faction-wide Growth score.
    baseA.AddBuilding("faction_growth_shrine"); // +2 Growth, FactionGlobal

    const auto pool = CollectActiveEffects(faction);
    CHECK(baseA.GetEffectiveSocialRating(SocialRatingId::Growth, pool) == 2);
    CHECK(baseB.GetEffectiveSocialRating(SocialRatingId::Growth, pool) == 2);

    faction.ProduceBaseResources({});
    CHECK(baseA.GetNutrientProduction() == 1); // growth level 2 -> +1 nutrients
    CHECK(baseB.GetNutrientProduction() == 1);
}
