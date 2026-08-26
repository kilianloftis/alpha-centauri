// Tests for effect filtering: FilterByStatId, FilterBaseLevelByStatId, FilterByStatIdInContext,
// FilterByScope, and condition evaluation (ConditionSatisfied).
//
// FilterForBase requires real BaseManager identities and lives in BaseIntegrationTests.cpp.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/faction/base/population/PopulationManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <ranges>

using namespace ac;
using actest::Active;
using Catch::Approx;

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
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
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
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& hq = fixture.MakeFactionBase(faction, 2, 2);
    hq.GetBuildingManager().AddBuilding("Headquarters");
    BaseManager& remote = fixture.MakeFactionBase(faction, 6, 6);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{hq, {
        Active(pool.StatMod(StatId_t::Energy, -1.0, ModifierOp_t::Add, EffectScope_t::AllOwnerBases,
                            std::nullopt, IsHeadquarters_t{}), "hq_only"),
        Active(pool.StatMod(StatId_t::Energy, 2.0), "flat"),
    }};

    // Without context: HQ-gated modifier stays out of base-level resolution.
    CHECK(std::ranges::distance(FilterBaseLevelByStatId(baseEffects, StatId_t::Energy)) == 1);

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

// Stockpile yield is resolved by StockpileConversion.cpp against the stockpile config's own
// effects, never against the base pool. If a MineralsConverted modifier ever reached the base
// lane it would be added to that stat every turn with no minerals consumed, so the exclusion
// here is the guard that keeps the two lanes apart. The conversion side is covered by the
// end-to-end yields in StockpileEnergyTests.
TEST_CASE("FilterBaseLevelByStatId excludes MineralsConverted",
          "[effects][filter][amount_source]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    actest::EffectPool pool;
    StatModifierEffect_t converted;
    converted.stat = StatId_t::Energy;
    converted.amount = 0.5;
    converted.op = ModifierOp_t::Add;
    converted.amountSource = StatModifierEffect_t::AmountSource_t::MineralsConverted;
    EffectConfig_t convertedConfig;
    convertedConfig.effect = converted;
    convertedConfig.scope = EffectScope_t::ThisBase;
    convertedConfig.persistence = EffectPersistence_t::Continuous;

    const BaseEffects_t baseEffects{base, {
        Active(pool.StatMod(StatId_t::Energy, 2.0), "flat"),
        Active(pool.Add(std::move(convertedConfig)), "converted"),
    }};

    const std::vector<ActiveEffect_t> baseLevel =
        actest::Materialize(FilterBaseLevelByStatId(baseEffects, StatId_t::Energy));
    REQUIRE(baseLevel.size() == 1);
    CHECK(baseLevel[0].sourceId == "flat");
}

TEST_CASE("FilterBaseLevelByStatId includes BaseSize only when the context carries pBase",
          "[effects][filter][amount_source]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);

    actest::EffectPool pool;
    StatModifierEffect_t baseSize;
    baseSize.stat = StatId_t::Drones;
    baseSize.amount = 0.25;
    baseSize.op = ModifierOp_t::Add;
    baseSize.amountSource = StatModifierEffect_t::AmountSource_t::BaseSize;
    EffectConfig_t baseSizeConfig;
    baseSizeConfig.effect = baseSize;
    baseSizeConfig.scope = EffectScope_t::ThisBase;
    baseSizeConfig.persistence = EffectPersistence_t::Continuous;

    const BaseEffects_t baseEffects{base, {
        Active(pool.StatMod(StatId_t::Drones, -2.0), "commons"),
        Active(pool.Add(std::move(baseSizeConfig)), "university"),
    }};

    // Without a context, BaseSize is dropped even though the bundle knows its base: the
    // caller's pCtx is what ResolveStatModifiers evaluates the amount source against, so
    // admitting on the bundle would hand the resolve a modifier it cannot evaluate.
    // ResolveBaseStat stamps the bundle's subject into the context and gets both.
    const std::vector<ActiveEffect_t> contextFree =
        actest::Materialize(FilterBaseLevelByStatId(baseEffects, StatId_t::Drones));
    REQUIRE(contextFree.size() == 1);
    CHECK(contextFree[0].sourceId == "commons");

    const EffectContext_t ctx{.pBase = &base};
    const std::vector<ActiveEffect_t> withBase =
        actest::Materialize(FilterBaseLevelByStatId(baseEffects, StatId_t::Drones, &ctx));
    REQUIRE(withBase.size() == 2);
}

TEST_CASE("ResolveBaseStat: University BaseSize floors per source then stacks with Commons",
          "[effects][amount_source][drones]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    base.GetPopulation().SetMaxSize(16);
    while (base.GetPopulation().GetSize() < 13)
    {
        base.GetPopulation().AddPop();
    }
    REQUIRE(base.GetPopulation().GetSize() == 13);

    actest::EffectPool pool;
    StatModifierEffect_t baseSize;
    baseSize.stat = StatId_t::Drones;
    baseSize.amount = 0.25;
    baseSize.op = ModifierOp_t::Add;
    baseSize.amountSource = StatModifierEffect_t::AmountSource_t::BaseSize;
    EffectConfig_t baseSizeConfig;
    baseSizeConfig.effect = baseSize;
    baseSizeConfig.scope = EffectScope_t::ThisBase;
    baseSizeConfig.persistence = EffectPersistence_t::Continuous;

    const BaseEffects_t baseEffects{base, {
        Active(pool.StatMod(StatId_t::Drones, -2.0), "commons"),
        Active(pool.Add(std::move(baseSizeConfig)), "university"),
    }};

    // floor(13×0.25)=3, plus Commons −2 → 1 (vanilla per-source floor).
    CHECK(FinalizeResolvedStat(
              ResolveBaseStat(baseEffects, StatId_t::Drones, SeedFor(StatId_t::Drones)))
          == 1);
}

TEST_CASE("ResolveBaseStat: BaseSize evaluates on GrowthRate without a wired call-site ctx",
          "[effects][amount_source][drop_in]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    // Default size is typically 1; floor(1×10)=10 AddPercent-scale amount as RawScaled seed 100.
    actest::EffectPool pool;
    StatModifierEffect_t baseSize;
    baseSize.stat = StatId_t::GrowthRate;
    baseSize.amount = 10.0;
    baseSize.op = ModifierOp_t::Add;
    baseSize.amountSource = StatModifierEffect_t::AmountSource_t::BaseSize;
    EffectConfig_t baseSizeConfig;
    baseSizeConfig.effect = baseSize;
    baseSizeConfig.scope = EffectScope_t::ThisBase;
    baseSizeConfig.persistence = EffectPersistence_t::Continuous;

    const BaseEffects_t baseEffects{base, {
        Active(pool.Add(std::move(baseSizeConfig)), "size_growth"),
    }};

    const int size = base.GetPopulation().GetSize();
    CHECK(ResolveBaseStat(baseEffects, StatId_t::GrowthRate, 100.0)
          == Approx(100.0 + static_cast<double>(size) * 10.0));
}

TEST_CASE("FilterBaseLevelByStatId: BaseSize admission keys on pCtx, not the bundle subject",
          "[effects][filter][amount_source]")
{
    // A hand-rolled filter+resolve pair (FoundBaseRules, AwayFromHomeDrones) passes no context
    // or one context to both. Admitting BaseSize on the bundle's own subject while
    // ResolveStatModifiers evaluates against the context is what made the no-context form
    // throw instead of resolving.
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);

    actest::EffectPool pool;
    StatModifierEffect_t baseSize;
    baseSize.stat = StatId_t::StartingMinerals;
    baseSize.amount = 2.0;
    baseSize.op = ModifierOp_t::Add;
    baseSize.amountSource = StatModifierEffect_t::AmountSource_t::BaseSize;
    EffectConfig_t baseSizeConfig;
    baseSizeConfig.effect = baseSize;
    baseSizeConfig.scope = EffectScope_t::ThisBase;
    baseSizeConfig.persistence = EffectPersistence_t::Continuous;

    const BaseEffects_t baseEffects{base, {
        Active(pool.StatMod(StatId_t::StartingMinerals, 10.0), "pod"),
        Active(pool.Add(std::move(baseSizeConfig)), "per_pop"),
    }};

    // No context: BaseSize is dropped, and resolving the same range must not throw.
    const std::vector<ActiveEffect_t> contextFree =
        actest::Materialize(FilterBaseLevelByStatId(baseEffects, StatId_t::StartingMinerals));
    REQUIRE(contextFree.size() == 1);
    CHECK(ResolveStatModifiers(contextFree, SeedFor(StatId_t::StartingMinerals)).total
          == Approx(10.0));

    // With a context carrying the subject: admitted and scaled by population size.
    const EffectContext_t ctx{.pBase = &base};
    const std::vector<ActiveEffect_t> withBase = actest::Materialize(
        FilterBaseLevelByStatId(baseEffects, StatId_t::StartingMinerals, &ctx));
    REQUIRE(withBase.size() == 2);
    const double size = static_cast<double>(base.GetPopulation().GetSize());
    CHECK(ResolveStatModifiers(withBase, SeedFor(StatId_t::StartingMinerals), &ctx).total
          == Approx(10.0 + std::floor(size * 2.0)));
}

TEST_CASE("AmountSourceValue: missing BaseSize subject throws",
          "[effects][amount_source]")
{
    StatModifierEffect_t mod;
    mod.stat = StatId_t::Drones;
    mod.amount = 0.25;
    mod.op = ModifierOp_t::Add;
    mod.amountSource = StatModifierEffect_t::AmountSource_t::BaseSize;
    CHECK_THROWS_AS(AmountSourceValue(mod, nullptr), std::runtime_error);
    EffectContext_t empty{};
    CHECK_THROWS_AS(AmountSourceValue(mod, &empty), std::runtime_error);
}

TEST_CASE("AmountSourceValue: missing BasesOwned subject throws",
          "[effects][amount_source]")
{
    StatModifierEffect_t mod;
    mod.stat = StatId_t::Attack;
    mod.amount = 1.0;
    mod.op = ModifierOp_t::Add;
    mod.amountSource = StatModifierEffect_t::AmountSource_t::BasesOwned;
    CHECK_THROWS_AS(AmountSourceValue(mod, nullptr), std::runtime_error);
    EffectContext_t empty{};
    CHECK_THROWS_AS(AmountSourceValue(mod, &empty), std::runtime_error);
}

TEST_CASE("BasesOwned: Empire Pulse Attack scales with owned base count",
          "[effects][amount_source][unit]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_empire_pulse"});

    // Flat +1 Attack; BasesOwned contribution is 0 with no bases.
    CHECK(unit.GetStat(StatId_t::Attack) == 1);
    // Design-only resolve has no faction subject, so BasesOwned is dropped.
    CHECK(ResolveStat(unit.GetDesign(), StatId_t::Attack) == 1);

    fixture.MakeFactionBase(faction, 2, 2);
    CHECK(faction.GetBaseCount() == 1);
    CHECK(unit.GetStat(StatId_t::Attack) == 2);

    fixture.MakeFactionBase(faction, 6, 6);
    CHECK(faction.GetBaseCount() == 2);
    CHECK(unit.GetStat(StatId_t::Attack) == 3);
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
