// Tests for two-level social rating resolution: SocialRatingModifier effects are ordinary
// effects from any source; accumulation runs per base AFTER FilterForBase, so a base's
// effective rating = faction-wide modifiers + its own ThisBase-scoped ones.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/IConstructable.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/Pop.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include "game/effects/ActiveEffect.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::Active;
using Catch::Approx;

TEST_CASE("AccumulateSocialRatings: sums per axis across sources", "[effects][rating]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
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

TEST_CASE("ResolveSocialRatingLevelEffects: maps accumulated levels through the rating table",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    actest::EffectPool pool;

    SECTION("a defined level returns its gameplay effects")
    {
        const BaseEffects_t ratingSource{base, {
            Active(pool.RatingMod(SocialRatingId_t::Growth, 2), "policy"),
        }};
        const std::vector<ActiveEffect_t> levelEffects =
            ResolveSocialRatingLevelEffects(ratingSource, fixture.socialRatings());

        // Fixture: growth level 2 -> +1 nutrients (AllOwnerBases).
        CHECK(ResolveStatModifiers(FilterByStatId(levelEffects, StatId_t::Nutrients), 0.0).total == Approx(1.0));
        bool foundRatingSource = false;
        for (const ActiveEffect_t& rEffect : levelEffects)
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
        const BaseEffects_t ratingSource{base, {
            Active(pool.RatingMod(SocialRatingId_t::Growth, 5), "policy"),
        }};
        const std::vector<ActiveEffect_t> levelEffects =
            ResolveSocialRatingLevelEffects(ratingSource, fixture.socialRatings());
        CHECK(ResolveStatModifiers(FilterByStatId(levelEffects, StatId_t::Nutrients), 0.0).total == Approx(3.0));

        bool foundClampedSource = false;
        for (const ActiveEffect_t& rEffect : levelEffects)
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
        const BaseEffects_t ratingSource{base, {
            Active(pool.RatingMod(SocialRatingId_t::Industry, -5), "policy"),
        }};
        const std::vector<ActiveEffect_t> levelEffects =
            ResolveSocialRatingLevelEffects(ratingSource, fixture.socialRatings());
        CHECK(ResolveStatModifiers(FilterByStatId(levelEffects, StatId_t::CostMultiplier), 100.0).total
              == Approx(110.0));
    }

    SECTION("in-range missing levels still produce no effects for that axis")
    {
        // Fixture industry levels: {-1, 1, 2}. Total 0 is inside [-1, 2] but unlisted.
        const BaseEffects_t ratingSource{base, {
            Active(pool.RatingMod(SocialRatingId_t::Industry, 1), "policy"),
            Active(pool.RatingMod(SocialRatingId_t::Industry, -1), "malus"),
        }};
        const std::vector<ActiveEffect_t> levelEffects =
            ResolveSocialRatingLevelEffects(ratingSource, fixture.socialRatings());
        for (const ActiveEffect_t& rEffect : levelEffects)
        {
            CHECK(rEffect.sourceId.find("se_rating_industry_") == std::string::npos);
        }
    }

    SECTION("modifiers that cancel to zero use the axis level-0 row when configured")
    {
        const BaseEffects_t ratingSource{base, {
            Active(pool.RatingMod(SocialRatingId_t::Growth, 2), "policy"),
            Active(pool.RatingMod(SocialRatingId_t::Growth, -2), "malus"),
        }};
        const std::vector<ActiveEffect_t> levelEffects =
            ResolveSocialRatingLevelEffects(ratingSource, fixture.socialRatings());
        // Growth has no level 0 — cancelled Growth contributes nothing.
        CHECK(ResolveStatModifiers(FilterByStatId(levelEffects, StatId_t::Nutrients), 0.0).total
              == Approx(0.0));
        // Support level 0 still expands (absolute free_unit_support = 2).
        CHECK(ResolveStatModifiers(FilterByStatId(levelEffects, StatId_t::FreeUnitSupport), 0.0)
                  .total
              == Approx(2.0));
    }

    SECTION("untouched axes still expand a configured level-0 row")
    {
        const BaseEffects_t ratingSource{base};
        const std::vector<ActiveEffect_t> levelEffects =
            ResolveSocialRatingLevelEffects(ratingSource, fixture.socialRatings());
        CHECK(ResolveStatModifiers(FilterByStatId(levelEffects, StatId_t::FreeUnitSupport), 0.0)
                  .total
              == Approx(2.0));
        bool foundSupportZero = false;
        for (const ActiveEffect_t& rEffect : levelEffects)
        {
            if (rEffect.sourceId == "se_rating_support_0")
            {
                foundSupportZero = true;
            }
        }
        CHECK(foundSupportZero);
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
        ConstructableKind_t GetConstructableKind() const override
        {
            return ConstructableKind_t::Building;
        }
    } item;

    base.GetProduction().SetProduction(&item, base.GetBaseEffects());
    CHECK(base.GetMineralCost() == 10); // Industry 0 → multiplier 1.0

    // Policy: +2 Industry → level 2 → CostMultiplier -20% → 0.8
    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("industry_policy"));
    CHECK(base.GetEffectiveSocialRating(SocialRatingId_t::Industry) == 2);
    CHECK(base.GetMineralCost() == 8);
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

TEST_CASE("Economy rating +2 adds one energy per worked square",
          "[effects][rating][economy]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    // Flat land so tile energy is stable; workers + free base center are the worked set.
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            fixture.At(4 + dx, 4 + dy).SetRockiness(Rockiness_t::Flat);
        }
    }
    base.GetWorkerAssignments().AutoAssignWorkers();

    int workedTiles = 1; // base center is always worked for free
    for (const Pop& rPop : base.GetPopulation().Pops())
    {
        if (rPop.GetTile() != nullptr)
        {
            ++workedTiles;
        }
    }
    REQUIRE(workedTiles >= 2);

    const int energyBefore = base.GetResources().GetEnergyProduction(base.GetBaseEffects());
    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("economy_policy"));
    CHECK(base.GetEffectiveSocialRating(SocialRatingId_t::Economy) == 2);
    CHECK(base.GetResources().GetEnergyProduction(base.GetBaseEffects())
          == energyBefore + workedTiles);
}

TEST_CASE("Economy rating -1 subtracts energy only at the headquarters base",
          "[effects][rating][economy]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& remote = fixture.MakeFactionBase(faction, 6, 6);
    hq.GetBuildingManager().AddBuilding("Headquarters");

    const int hqBefore = hq.GetResources().GetEnergyProduction(hq.GetBaseEffects());
    const int remoteBefore = remote.GetResources().GetEnergyProduction(remote.GetBaseEffects());

    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("low_economy_policy"));
    CHECK(hq.GetEffectiveSocialRating(SocialRatingId_t::Economy) == -1);
    CHECK(remote.GetEffectiveSocialRating(SocialRatingId_t::Economy) == -1);

    CHECK(hq.GetResources().GetEnergyProduction(hq.GetBaseEffects()) == hqBefore - 1);
    CHECK(remote.GetResources().GetEnergyProduction(remote.GetBaseEffects()) == remoteBefore);
}

TEST_CASE("Economy rating +4 adds per-square energy, flat energy, and commerce bonus",
          "[effects][rating][economy]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    base.GetWorkerAssignments().AutoAssignWorkers();

    int workedTiles = 1;
    for (const Pop& rPop : base.GetPopulation().Pops())
    {
        if (rPop.GetTile() != nullptr)
        {
            ++workedTiles;
        }
    }

    const int energyBefore = base.GetResources().GetEnergyProduction(base.GetBaseEffects());
    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("high_economy_policy"));
    CHECK(base.GetEffectiveSocialRating(SocialRatingId_t::Economy) == 4);

    // Level 4: +1 energy/sq + +2 energy/base.
    CHECK(base.GetResources().GetEnergyProduction(base.GetBaseEffects())
          == energyBefore + workedTiles + 2);

    const int commerceBonus = FinalizeResolvedStat(
        ResolveStatModifiers(
            FilterBaseLevelByStatId(base.GetBaseEffects(), StatId_t::CommerceEnergyBonus),
            SeedFor(StatId_t::CommerceEnergyBonus))
            .total);
    CHECK(commerceBonus == 2);
}

TEST_CASE("Base-level minerals AddPercent scales worked minerals",
          "[effects][rating][minerals]")
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

    base.GetBuildingManager().AddBuilding("minerals_amplifier");
    const int expected = FinalizeResolvedStat(static_cast<double>(worked) * 0.9);
    REQUIRE(expected < worked);
    CHECK(base.GetMineralProduction() == expected);

    faction.ProduceBaseResources();
    CHECK(base.GetResources().ConsumeMinerals() == expected);
}

TEST_CASE("Base-level Labs AddPercent seeds from the energy split, not zero",
          "[effects][rating][labs]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    // Flat +10 energy at HQ → default 50% labs split = 5 (no inefficiency).
    base.GetBuildingManager().AddBuilding("Headquarters");
    base.GetBuildingManager().AddBuilding("world_beacon");
    const int labsBefore = base.GetLabsProduction();
    REQUIRE(labsBefore == 5);

    // +50% Labs must scale the split (5 * 1.5 = 7.5 → 8), not vanish on SeedFor(Labs)==0.
    base.GetBuildingManager().AddBuilding("labs_amplifier");
    const int expected = FinalizeResolvedStat(static_cast<double>(labsBefore) * 1.5);
    CHECK(expected == 8);
    CHECK(base.GetLabsProduction() == expected);

    faction.ProduceBaseResources();
    CHECK(base.GetResources().ConsumeLabs() == expected);
}

TEST_CASE("Research rating AddPercent scales labs from the energy split",
          "[effects][rating][research]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    base.GetBuildingManager().AddBuilding("Headquarters");
    base.GetBuildingManager().AddBuilding("world_beacon");
    const int labsBefore = base.GetLabsProduction();
    REQUIRE(labsBefore == 5);

    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("research_policy"));
    CHECK(base.GetEffectiveSocialRating(SocialRatingId_t::Research) == 2);

    // Level 2: +20% labs → 5 * 1.2 = 6.
    const int expected = FinalizeResolvedStat(static_cast<double>(labsBefore) * 1.2);
    CHECK(expected == 6);
    CHECK(base.GetLabsProduction() == expected);

    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("low_research_policy"));
    CHECK(base.GetEffectiveSocialRating(SocialRatingId_t::Research) == -2);
    // Level -2: -20% labs → 5 * 0.8 = 4.
    CHECK(base.GetLabsProduction()
          == FinalizeResolvedStat(static_cast<double>(labsBefore) * 0.8));
}

TEST_CASE("Faction-lane rating expand ignores ThisBase modifiers across multiple bases",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(faction, 6, 6);

    // Policy +2 Morale (FactionGlobal). Two ThisBase shrines must not inflate FactionUnits.
    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("morale_policy"));
    baseA.GetBuildingManager().AddBuilding("morale_shrine");
    baseB.GetBuildingManager().AddBuilding("morale_shrine");

    const FactionEffects_t& rPool = faction.GetLocalActiveEffects();
    bool foundPolicyLevel = false;
    bool foundInflatedLevel = false;
    for (const ActiveEffect_t& rEffect : rPool.effects)
    {
        if (rEffect.sourceId == "se_rating_morale_2")
        {
            foundPolicyLevel = true;
        }
        if (rEffect.sourceId == "se_rating_morale_4")
        {
            foundInflatedLevel = true;
        }
    }
    CHECK(foundPolicyLevel);
    CHECK_FALSE(foundInflatedLevel);

    // Per-base effective ratings still see their own shrine.
    CHECK(baseA.GetEffectiveSocialRating(SocialRatingId_t::Morale) == 3);
    CHECK(baseB.GetEffectiveSocialRating(SocialRatingId_t::Morale) == 3);
}

TEST_CASE("Faction-lane rating expand observes pop FactionGlobal SocialRatingModifiers",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    // MoraleOfficer declares +1 Morale FactionGlobal. If rating expand ran before pop
    // collection, the expanded FactionUnits level would be absent.
    base.GetPopulation().AddPop("MoraleOfficer");

    const FactionEffects_t& rPool = faction.GetLocalActiveEffects();
    bool foundLevel = false;
    for (const ActiveEffect_t& rEffect : rPool.effects)
    {
        if (rEffect.sourceId == "se_rating_morale_1")
        {
            foundLevel = true;
        }
    }
    CHECK(foundLevel);
}

TEST_CASE("Faction-lane rating expand observes unit-component SocialRatingModifiers",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 2, 2);

    // morale_beacon declares +1 Morale FactionGlobal on a component of a live unit. Unit
    // faction-lane effects are collected after ratings only if the pipeline is misordered.
    fixture.MakeUnit(faction, 3, 3, {"test_chassis", "morale_beacon"});

    bool foundLevel = false;
    for (const ActiveEffect_t& rEffect : faction.GetLocalActiveEffects().effects)
    {
        if (rEffect.sourceId == "se_rating_morale_1")
        {
            foundLevel = true;
        }
    }
    CHECK(foundLevel);
}

TEST_CASE("removed_by_tech gates GrantBuilding before expansion",
          "[effects][rating][grant]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    base.GetBuildingManager().AddBuilding("gated_grantor");
    // FilterBaseLevelByStatId excludes AnyTile MaxClamp from tile_yield_rules — same cut
    // production uses. FilterByStatId would include those clamps and mute the grant Adds.
    const BaseEffects_t local{base, faction.GetLocalActiveEffects().effects};
    REQUIRE(ResolveStatModifiers(FilterBaseLevelByStatId(local, StatId_t::Minerals), 0.0).total
            == Approx(3.0));

    faction.GetResearch().AddDiscoveredTech("gene_splicing");
    const BaseEffects_t afterLift{base, faction.GetLocalActiveEffects().effects};
    CHECK(ResolveStatModifiers(FilterBaseLevelByStatId(afterLift, StatId_t::Minerals), 0.0).total
          == Approx(0.0));
}

TEST_CASE("removed_by_tech gates SocialRatingModifier before faction-lane expand",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    base.GetBuildingManager().AddBuilding("gated_morale_policy_building");
    {
        bool found = false;
        for (const ActiveEffect_t& rEffect : faction.GetLocalActiveEffects().effects)
        {
            if (rEffect.sourceId == "se_rating_morale_2")
            {
                found = true;
            }
        }
        CHECK(found);
    }

    faction.GetResearch().AddDiscoveredTech("gene_splicing");
    for (const ActiveEffect_t& rEffect : faction.GetLocalActiveEffects().effects)
    {
        CHECK(rEffect.sourceId.find("se_rating_morale_") == std::string::npos);
    }
}

TEST_CASE("removed_by_tech gates a rating modifier that arrives through a grant",
          "[effects][rating][grant]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    // The grantor is ungated; the gated +2 Morale modifier only exists after grant
    // expansion. The gate must run between expansion and rating accumulation — the level
    // effects it would produce carry no gate of their own, so a single trailing pass
    // would leave se_rating_morale_2 behind.
    base.GetBuildingManager().AddBuilding("rating_grantor");
    {
        bool found = false;
        for (const ActiveEffect_t& rEffect : faction.GetLocalActiveEffects().effects)
        {
            if (rEffect.sourceId.find("se_rating_morale_") != std::string::npos)
            {
                found = true;
            }
        }
        REQUIRE(found);
    }

    faction.GetResearch().AddDiscoveredTech("gene_splicing");
    for (const ActiveEffect_t& rEffect : faction.GetLocalActiveEffects().effects)
    {
        CHECK(rEffect.sourceId.find("se_rating_morale_") == std::string::npos);
    }
}

TEST_CASE("Two factions with equal revision stamps do not share a local effect pool",
          "[effects][rating]")
{
    actest::FactionFixture fixture;
    actest::EffectPool effectPool;

    // Configs must outlive the Faction references into them (declare before the factions).
    FactionConfig_t defA = fixture.factionDefinition;
    defA.id = "faction_a";
    defA.effects.push_back(
        effectPool.StatMod(StatId_t::Energy, 5.0, ModifierOp_t::Add, EffectScope_t::FactionGlobal));
    FactionConfig_t defB = fixture.factionDefinition;
    defB.id = "faction_b";
    defB.effects.push_back(
        effectPool.StatMod(StatId_t::Energy, 11.0, ModifierOp_t::Add, EffectScope_t::FactionGlobal));

    // Keep factions out of fixture.factions so destruction order is factions → defs.
    auto pFactionA = std::make_unique<Faction>(1, true, defA, fixture.dataContext,
                                               fixture.map, fixture.settings,
                                               actest::k_TestFactionSeed + 1);
    auto pFactionB = std::make_unique<Faction>(2, false, defB, fixture.dataContext,
                                               fixture.map, fixture.settings,
                                               actest::k_TestFactionSeed + 2);
    BaseManager& baseA = fixture.MakeFactionBase(*pFactionA, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(*pFactionB, 4, 4);

    // Base-level filter so tile_yield_rules AnyTile MaxClamp is not part of this assert.
    const BaseEffects_t localA{baseA, pFactionA->GetLocalActiveEffects().effects};
    const BaseEffects_t localB{baseB, pFactionB->GetLocalActiveEffects().effects};
    CHECK(ResolveStatModifiers(FilterBaseLevelByStatId(localA, StatId_t::Energy), 0.0).total
          == Approx(5.0));
    CHECK(ResolveStatModifiers(FilterBaseLevelByStatId(localB, StatId_t::Energy), 0.0).total
          == Approx(11.0));
}
