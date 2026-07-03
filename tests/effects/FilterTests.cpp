// Tests for effect filtering: FilterByStatId, FilterFlatByStatId, FilterByStatIdInContext,
// FilterByScope, and condition evaluation (ConditionSatisfied).
//
// FilterForBase requires real BaseManager identities and lives in BaseIntegrationTests.cpp.

#include "TestHelpers.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "lib/effects/ActiveEffect.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::Active;

TEST_CASE("FilterByStatId: keeps only StatModifiers targeting the requested stat", "[effects][filter]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Nutrients, 1.0), "nut"),
        Active(pool.StatMod(StatId::Minerals, 2.0), "min"),
        Active(pool.RuleFlag(RuleFlagId::Flight), "flag"),
    };

    const auto matching = FilterByStatId(effects, StatId::Nutrients);
    REQUIRE(matching.size() == 1);
    CHECK(matching[0].sourceId == "nut");
    CHECK(FilterByStatId(effects, StatId::Energy).empty());
}

TEST_CASE("FilterByStatId: includes selector-carrying (per-tile) modifiers", "[effects][filter]")
{
    // Documented: FilterByStatId keeps per-tile modifiers; only FilterFlatByStatId drops them.
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase,
                            actest::ImprovementSelector("Farm")), "farm_booster"),
    };
    CHECK(FilterByStatId(effects, StatId::Nutrients).size() == 1);
}

TEST_CASE("FilterByStatId: excludes condition-carrying effects from context-free resolution",
          "[effects][filter][condition]")
{
    // A "+25% attack vs Base" style effect must never leak into context-free totals.
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent, EffectScope_t::ThisUnit,
                            std::nullopt, actest::TargetTileHas("Base")), "vs_base"),
        Active(pool.StatMod(StatId::Attack, 4.0, ModifierOp::Add, EffectScope_t::ThisUnit), "weapon"),
    };

    const auto matching = FilterByStatId(effects, StatId::Attack);
    REQUIRE(matching.size() == 1);
    CHECK(matching[0].sourceId == "weapon");
}

TEST_CASE("FilterFlatByStatId: excludes both selector-carrying and condition-carrying modifiers",
          "[effects][filter]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Nutrients, 2.0), "flat"),
        Active(pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase,
                            actest::ImprovementSelector("Farm")), "per_tile"),
        Active(pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase,
                            std::nullopt, actest::TargetTileHas("River")), "conditional"),
    };

    const auto matching = FilterFlatByStatId(effects, StatId::Nutrients);
    REQUIRE(matching.size() == 1);
    CHECK(matching[0].sourceId == "flat");
}

TEST_CASE("ConditionSatisfied: no condition always applies", "[effects][condition]")
{
    actest::EffectPool pool;
    const EffectConfig_t& config = pool.StatMod(StatId::Attack, 1.0);

    CHECK(ConditionSatisfied(config, EffectContext_t{}));
    Tile tile(3, 3);
    CHECK(ConditionSatisfied(config, EffectContext_t{&tile}));
}

TEST_CASE("ConditionSatisfied: TargetTileHas matches terrain features via string id", "[effects][condition]")
{
    actest::EffectPool pool;
    const EffectConfig_t& vsRocky = pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent,
                                                 EffectScope_t::ThisUnit, std::nullopt,
                                                 actest::TargetTileHas("Rocky"));

    Tile tile(0, 0);
    tile.SetRockiness(Rockiness::Flat);
    CHECK_FALSE(ConditionSatisfied(vsRocky, EffectContext_t{&tile}));

    tile.SetRockiness(Rockiness::Rocky);
    CHECK(ConditionSatisfied(vsRocky, EffectContext_t{&tile}));
}

TEST_CASE("ConditionSatisfied: TargetTileHas matches improvements, including Base", "[effects][condition]")
{
    actest::EffectPool pool;
    const EffectConfig_t& vsBase = pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent,
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
    const EffectConfig_t& conditional = pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent,
                                                     EffectScope_t::ThisUnit, std::nullopt,
                                                     actest::TargetTileHas("Forest"));
    CHECK_FALSE(ConditionSatisfied(conditional, EffectContext_t{}));
}

TEST_CASE("FilterByStatIdInContext: unconditional effects plus satisfied conditionals", "[effects][filter][condition]")
{
    actest::EffectPool pool;
    Tile rockyTile(0, 0);
    rockyTile.SetRockiness(Rockiness::Rocky);

    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Attack, 4.0, ModifierOp::Add, EffectScope_t::ThisUnit), "weapon"),
        Active(pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent, EffectScope_t::ThisUnit,
                            std::nullopt, actest::TargetTileHas("Rocky")), "vs_rocky"),
        Active(pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent, EffectScope_t::ThisUnit,
                            std::nullopt, actest::TargetTileHas("Base")), "vs_base"),
        Active(pool.StatMod(StatId::Defense, 2.0, ModifierOp::Add, EffectScope_t::ThisUnit), "armor"),
    };

    const EffectContext_t ctx{&rockyTile};
    const auto matching = FilterByStatIdInContext(effects, StatId::Attack, ctx);

    REQUIRE(matching.size() == 2);
    CHECK(matching[0].sourceId == "weapon");
    CHECK(matching[1].sourceId == "vs_rocky");

    // Combat math sanity: 4 attack, +25% vs rocky -> 5.
    CHECK(ResolveStatModifiers(matching).total == 5.0);
}

TEST_CASE("FilterByStatIdInContext: with an empty context only unconditional effects survive",
          "[effects][filter][condition]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Attack, 4.0, ModifierOp::Add, EffectScope_t::ThisUnit), "weapon"),
        Active(pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent, EffectScope_t::ThisUnit,
                            std::nullopt, actest::TargetTileHas("Rocky")), "vs_rocky"),
    };

    const auto matching = FilterByStatIdInContext(effects, StatId::Attack, EffectContext_t{});
    REQUIRE(matching.size() == 1);
    CHECK(matching[0].sourceId == "weapon");
}

TEST_CASE("FilterByScope: exact scope match only", "[effects][filter]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Econ, 1.0, ModifierOp::Add, EffectScope_t::ThisBase), "flat"),
        Active(pool.StatMod(StatId::Nutrients, 50.0, ModifierOp::AddPercent, EffectScope_t::ThisPop), "mult"),
        Active(pool.StatMod(StatId::Energy, 1.0, ModifierOp::Add, EffectScope_t::FactionGlobal), "global"),
    };

    const auto thisPop = FilterByScope(effects, EffectScope_t::ThisPop);
    REQUIRE(thisPop.size() == 1);
    CHECK(thisPop[0].sourceId == "mult");

    const auto thisBase = FilterByScope(effects, EffectScope_t::ThisBase);
    REQUIRE(thisBase.size() == 1);
    CHECK(thisBase[0].sourceId == "flat");

    CHECK(FilterByScope(effects, EffectScope_t::WorldGlobal).empty());
}

TEST_CASE("Filters tolerate null configs", "[effects][filter]")
{
    std::vector<ActiveEffect_t> effects;
    effects.push_back(ActiveEffect_t{nullptr, "broken", nullptr});

    CHECK(FilterByStatId(effects, StatId::Energy).empty());
    CHECK(FilterFlatByStatId(effects, StatId::Energy).empty());
    CHECK(FilterByStatIdInContext(effects, StatId::Energy, EffectContext_t{}).empty());
    CHECK(FilterByScope(effects, EffectScope_t::ThisBase).empty());
}
