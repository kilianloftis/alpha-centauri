// Tests for effect filtering: FilterByStatId, FilterBaseLevelByStatId, FilterByStatIdInContext,
// FilterByScope, and condition evaluation (ConditionSatisfied).
//
// FilterForBase requires real BaseManager identities and lives in BaseIntegrationTests.cpp.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <ranges>

using namespace ac;
using actest::Active;

TEST_CASE("FilterByStatId: keeps only StatModifiers targeting the requested stat", "[effects][filter]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Nutrients, 1.0), "nut"),
        Active(pool.StatMod(StatId_t::Minerals, 2.0), "min"),
        Active(pool.RuleFlag(RuleFlagId_t::ForcesPsiCombat), "flag"),
    };

    const std::vector<ActiveEffect_t> matching = actest::Materialize(FilterByStatId(effects, StatId_t::Nutrients));
    REQUIRE(matching.size() == 1);
    CHECK(matching[0].sourceId == "nut");
    CHECK(FilterByStatId(effects, StatId_t::Energy).empty());
}

TEST_CASE("FilterByStatId: includes selector-carrying (per-tile) modifiers", "[effects][filter]")
{
    // Documented: FilterByStatId keeps per-tile modifiers; only FilterBaseLevelByStatId drops them.
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                            actest::ImprovementSelector("Farm")), "farm_booster"),
    };
    CHECK(std::ranges::distance(FilterByStatId(effects, StatId_t::Nutrients)) == 1);
}

TEST_CASE("FilterByStatId: excludes condition-carrying effects from context-free resolution",
          "[effects][filter][condition]")
{
    // A "+25% attack vs Base" style effect must never leak into context-free totals.
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                            std::nullopt, actest::TargetTileHas("Base")), "vs_base"),
        Active(pool.StatMod(StatId_t::Attack, 4.0, ModifierOp_t::Add, EffectScope_t::ThisUnit), "weapon"),
    };

    const std::vector<ActiveEffect_t> matching = actest::Materialize(FilterByStatId(effects, StatId_t::Attack));
    REQUIRE(matching.size() == 1);
    CHECK(matching[0].sourceId == "weapon");
}

TEST_CASE("FilterBaseLevelByStatId: excludes both selector-carrying and condition-carrying modifiers",
          "[effects][filter]")
{
    actest::EffectPool pool;
    const BaseEffects_t baseEffects{{
        Active(pool.StatMod(StatId_t::Nutrients, 2.0), "flat"),
        Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                            actest::ImprovementSelector("Farm")), "per_tile"),
        Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                            std::nullopt, actest::TargetTileHas("River")), "conditional"),
    }};

    const std::vector<ActiveEffect_t> matching = actest::Materialize(FilterBaseLevelByStatId(baseEffects, StatId_t::Nutrients));
    REQUIRE(matching.size() == 1);
    CHECK(matching[0].sourceId == "flat");
}

TEST_CASE("FilterBaseLevelByStatId with context includes satisfied conditions",
          "[effects][filter][condition]")
{
    actest::EffectPool pool;
    const BaseEffects_t baseEffects{{
        Active(pool.StatMod(StatId_t::Energy, -1.0, ModifierOp_t::Add, EffectScope_t::AllOwnerBases,
                            std::nullopt, IsHeadquarters_t{}), "hq_only"),
        Active(pool.StatMod(StatId_t::Energy, 2.0), "flat"),
    }};

    // Without context: HQ-gated modifier stays out of base-level resolution.
    CHECK(std::ranges::distance(FilterBaseLevelByStatId(baseEffects, StatId_t::Energy)) == 1);

    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 2, 2);
    hq.GetBuildingManager().AddBuilding("Headquarters");
    BaseManager& remote = fixture.MakeFactionBase(faction, 6, 6);

    const EffectContext_t hqCtx{.pBase = &hq};
    const auto hqMatching = actest::Materialize(
        FilterBaseLevelByStatId(baseEffects, StatId_t::Energy, &hqCtx));
    REQUIRE(hqMatching.size() == 2);

    const EffectContext_t remoteCtx{.pBase = &remote};
    const auto remoteMatching = actest::Materialize(
        FilterBaseLevelByStatId(baseEffects, StatId_t::Energy, &remoteCtx));
    REQUIRE(remoteMatching.size() == 1);
    CHECK(remoteMatching[0].sourceId == "flat");
}

TEST_CASE("ConditionSatisfied: no condition always applies", "[effects][condition]")
{
    actest::EffectPool pool;
    const EffectConfig_t& config = pool.StatMod(StatId_t::Attack, 1.0);

    CHECK(ConditionSatisfied(config, EffectContext_t{}));
    Tile tile(3, 3);
    CHECK(ConditionSatisfied(config, EffectContext_t{&tile}));
}

TEST_CASE("ConditionSatisfied: TargetTileHas matches terrain features via string id", "[effects][condition]")
{
    actest::EffectPool pool;
    const EffectConfig_t& vsRocky = pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent,
                                                 EffectScope_t::ThisUnit, std::nullopt,
                                                 actest::TargetTileHas("Rocky"));

    Tile tile(0, 0);
    tile.SetRockiness(Rockiness_t::Flat);
    CHECK_FALSE(ConditionSatisfied(vsRocky, EffectContext_t{&tile}));

    tile.SetRockiness(Rockiness_t::Rocky);
    CHECK(ConditionSatisfied(vsRocky, EffectContext_t{&tile}));
}

TEST_CASE("ConditionSatisfied: TargetTileHas matches improvements, including Base", "[effects][condition]")
{
    actest::EffectPool pool;
    const EffectConfig_t& vsBase = pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent,
                                                EffectScope_t::ThisUnit, std::nullopt,
                                                actest::TargetTileHas("Base"));

    ImprovementConfig_t baseImprovement;
    baseImprovement.id = "Base";

    Tile tile(0, 0);
    CHECK_FALSE(ConditionSatisfied(vsBase, EffectContext_t{&tile}));

    tile.AddImprovement(baseImprovement);
    CHECK(ConditionSatisfied(vsBase, EffectContext_t{&tile}));

    tile.RemoveImprovement("Base");
    CHECK_FALSE(ConditionSatisfied(vsBase, EffectContext_t{&tile}));
}

TEST_CASE("ConditionSatisfied: a conditional effect with no target tile in context is not satisfied",
          "[effects][condition]")
{
    actest::EffectPool pool;
    const EffectConfig_t& conditional = pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent,
                                                     EffectScope_t::ThisUnit, std::nullopt,
                                                     actest::TargetTileHas("Forest"));
    CHECK_FALSE(ConditionSatisfied(conditional, EffectContext_t{}));
}

TEST_CASE("FilterByStatIdInContext: unconditional effects plus satisfied conditionals", "[effects][filter][condition]")
{
    actest::EffectPool pool;
    Tile rockyTile(0, 0);
    rockyTile.SetRockiness(Rockiness_t::Rocky);

    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Attack, 4.0, ModifierOp_t::Add, EffectScope_t::ThisUnit), "weapon"),
        Active(pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                            std::nullopt, actest::TargetTileHas("Rocky")), "vs_rocky"),
        Active(pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                            std::nullopt, actest::TargetTileHas("Base")), "vs_base"),
        Active(pool.StatMod(StatId_t::Defense, 2.0, ModifierOp_t::Add, EffectScope_t::ThisUnit), "armor"),
    };

    const EffectContext_t ctx{&rockyTile};
    const std::vector<ActiveEffect_t> matching = actest::Materialize(FilterByStatIdInContext(effects, StatId_t::Attack, ctx));

    REQUIRE(matching.size() == 2);
    CHECK(matching[0].sourceId == "weapon");
    CHECK(matching[1].sourceId == "vs_rocky");

    // Combat math sanity: 4 attack, +25% vs rocky -> 5.
    CHECK(ResolveStatModifiers(matching, 0.0).total == 5.0);
}

TEST_CASE("FilterByStatIdInContext: with an empty context only unconditional effects survive",
          "[effects][filter][condition]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Attack, 4.0, ModifierOp_t::Add, EffectScope_t::ThisUnit), "weapon"),
        Active(pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                            std::nullopt, actest::TargetTileHas("Rocky")), "vs_rocky"),
    };

    const std::vector<ActiveEffect_t> matching =
        actest::Materialize(FilterByStatIdInContext(effects, StatId_t::Attack, EffectContext_t{}));
    REQUIRE(matching.size() == 1);
    CHECK(matching[0].sourceId == "weapon");
}

TEST_CASE("FilterByScope: exact scope match only", "[effects][filter]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Econ, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase), "flat"),
        Active(pool.StatMod(StatId_t::Nutrients, 50.0, ModifierOp_t::AddPercent, EffectScope_t::ThisPop), "mult"),
        Active(pool.StatMod(StatId_t::Energy, 1.0, ModifierOp_t::Add, EffectScope_t::FactionGlobal), "global"),
    };

    const std::vector<ActiveEffect_t> thisPop = actest::Materialize(FilterByScope(effects, EffectScope_t::ThisPop));
    REQUIRE(thisPop.size() == 1);
    CHECK(thisPop[0].sourceId == "mult");

    const std::vector<ActiveEffect_t> thisBase = actest::Materialize(FilterByScope(effects, EffectScope_t::ThisBase));
    REQUIRE(thisBase.size() == 1);
    CHECK(thisBase[0].sourceId == "flat");

    CHECK(FilterByScope(effects, EffectScope_t::WorldGlobal).empty());
}

// Rvalue Filter* overloads are deleted so a temporary vector cannot silently dangle the
// borrowing view (e.g. FilterByStatId(std::vector<ActiveEffect_t>{}, …) is ill-formed).

TEST_CASE("ConditionSatisfied: AttackerIsEmbarked requires pAttacker", "[effects][condition]")
{
    actest::EffectPool pool;
    EffectConfig_t config;
    config.effect = PermissionEffect_t{PermissionId_t::Attack};
    config.scope = EffectScope_t::ThisUnit;
    config.persistence = EffectPersistence_t::Continuous;
    config.condition = AttackerIsEmbarked_t{};
    const EffectConfig_t& rConfig = pool.Add(std::move(config));

    CHECK_FALSE(ConditionSatisfied(rConfig, EffectContext_t{}));
}

TEST_CASE("HasFeature: Water matches IsWater tiles", "[effects][condition][tile]")
{
    Tile water(0, 0);
    water.SetElevation(-100);
    Tile land(1, 0);
    land.SetElevation(100);

    CHECK(water.HasFeature("Water"));
    CHECK(water.IsWater());
    CHECK_FALSE(land.HasFeature("Water"));
    CHECK_FALSE(land.IsWater());
}

TEST_CASE("HasFeature: Ocean vs OceanShelf by depth", "[effects][condition][tile]")
{
    Tile shelf(0, 0);
    shelf.SetElevation(-100);
    Tile ocean(1, 0);
    ocean.SetElevation(k_OceanShelfMinElevation - 1);
    Tile land(2, 0);
    land.SetElevation(100);

    CHECK(shelf.HasFeature("OceanShelf"));
    CHECK_FALSE(shelf.HasFeature("Ocean"));
    CHECK(ocean.HasFeature("Ocean"));
    CHECK_FALSE(ocean.HasFeature("OceanShelf"));
    CHECK_FALSE(land.HasFeature("Ocean"));
    CHECK_FALSE(land.HasFeature("OceanShelf"));
}
