// Tests for universal scope routing: an effect declared on ANY config is routed by its
// scope, regardless of which source declared it.
//
// Lanes covered here: faction pool contributions from pops and unit components,
// FactionUnits -> live unit stats, WorldGlobal across factions, and unit-projected
// ThisTile auras.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/GameState.h"
#include "game/GameSettings.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/InteractionResolve.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "game/units/MovementRules.h"

#include <variant>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using Catch::Approx;

TEST_CASE("Faction pool: a pop type's FactionGlobal effect is collected faction-wide",
          "[effects][routing][pop]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    // A Networker pop carries "+1 labs, FactionGlobal".
    base.GetPopulation().AddPop("Networker");

    const auto pool = CollectActiveEffects(faction);
    CHECK(ResolveStatModifiers(FilterByStatId(pool.effects, StatId_t::Labs), 0.0).total == Approx(1.0));

    // Two Networkers stack.
    base.GetPopulation().AddPop("Networker");
    const auto pool2 = CollectActiveEffects(faction);
    CHECK(ResolveStatModifiers(FilterByStatId(pool2.effects, StatId_t::Labs), 0.0).total == Approx(2.0));
}

TEST_CASE("Faction pool: a unit component's FactionGlobal effect applies while the unit lives",
          "[effects][routing][unit]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    CHECK(ResolveStatModifiers(FilterByStatId(CollectActiveEffects(faction).effects, StatId_t::Energy), 0.0).total
          == Approx(0.0));

    // energy_siphon: "+1 energy, FactionGlobal" on a component.
    Unit& unit = fixture.MakeUnit(faction, 0, 0, {"energy_siphon"});
    CHECK(ResolveStatModifiers(FilterByStatId(CollectActiveEffects(faction).effects, StatId_t::Energy), 0.0).total
          == Approx(1.0));

    // The effect disappears with the unit.
    faction.GetUnitManager().DestroyUnit(unit);
    CHECK(ResolveStatModifiers(FilterByStatId(CollectActiveEffects(faction).effects, StatId_t::Energy), 0.0).total
          == Approx(0.0));
}

TEST_CASE("FactionUnits lane: a building's FactionUnits stat modifier boosts live unit stats",
          "[effects][routing][unit]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_weapon"}, &base); // 4 attack intrinsic
    CHECK(unit.GetStat(StatId_t::Attack) == 4);

    base.GetBuildingManager().AddBuilding("unit_attack_array"); // +25% attack, FactionUnits
    CHECK(unit.GetStat(StatId_t::Attack) == 5); // 4 * 1.25

    // The design (unit-designer view) keeps showing intrinsic values.
    CHECK(unit.GetDesign().GetStat(StatId_t::Attack) == 4);

    base.GetBuildingManager().DestroyBuilding("unit_attack_array");
    CHECK(unit.GetStat(StatId_t::Attack) == 4);
}

TEST_CASE("FactionUnits lane: a building's FactionUnits InteractionOverride applies to live units",
          "[effects][routing][unit]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& home = fixture.MakeFactionBase(faction, 2, 2);
    fixture.At(5, 4).SetElevation(-100);
    BaseManager& seaBase = fixture.MakeFactionBase(faction, 5, 4);

    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"}, &home);
    InteractionQuery_t enterWater;
    enterWater.grid = InteractionGridId_t::Enter;
    enterWater.actorDomain = UnitDomain_t::Land;
    enterWater.surface = InteractionSurface_t::Water;
    EffectContext_t ctx;
    ctx.targetTile = &seaBase.GetTile();

    CHECK(ResolveInteractionCell(fixture.dataContext.interactionGrids, enterWater, &unit, ctx)
          == InteractionCell_t::Deny);

    home.GetBuildingManager().AddBuilding("amphibious_grantor");
    CHECK(ResolveInteractionCell(fixture.dataContext.interactionGrids, enterWater, &unit, ctx)
          == InteractionCell_t::Allow);

    // Intrinsic design unchanged — grant arrives from the faction pool only.
    bool bDesignHasEnterOverride = false;
    for (const ActiveEffect_t& rEffect : unit.GetDesign().CollectEffects())
    {
        const auto* pOv = rEffect.config
            ? std::get_if<InteractionOverrideEffect_t>(&rEffect.config->effect)
            : nullptr;
        if (pOv && pOv->grid == InteractionGridId_t::Enter)
        {
            bDesignHasEnterOverride = true;
        }
    }
    CHECK_FALSE(bDesignHasEnterOverride);
}

TEST_CASE("ResolveFlag: context-free resolution skips condition-carrying RuleFlags",
          "[effects][flag][condition]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_conditional_psi_flag"});
    CHECK_FALSE(unit.GetFlag(RuleFlagId_t::ForcesPsiCombat));
    CHECK_FALSE(unit.GetDesign().GetFlag(RuleFlagId_t::ForcesPsiCombat));
}

TEST_CASE("on_unit_produced GrantXp: Aerospace Complex only boosts air starting XP",
          "[effects][routing][condition]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    base.GetBuildingManager().AddBuilding("Aerospace_Complex");

    // Explicit producedAt: prototypes are "first one you built", not free fieldings.
    Unit& land = fixture.MakeUnit(faction, 4, 4, {"test_chassis"}, &base, &base);
    Unit& air = fixture.MakeUnit(faction, 5, 4, {"test_flight_chassis"}, &base, &base);

    // Both are the first built of their components, so both are prototypes.
    CHECK(land.GetXp() == 2); // base_intrinsic + prototype GrantXp
    CHECK(air.GetXp() == 4); // base_intrinsic 1 + prototype 1 + Aerospace 2
}

TEST_CASE("on_unit_produced GrantXp applies at the production base only",
          "[effects][routing][producedAt]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& withComplex = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& otherBase = fixture.MakeFactionBase(faction, 6, 6);
    withComplex.GetBuildingManager().AddBuilding("Aerospace_Complex");

    Unit& airBuiltThere =
        fixture.MakeUnit(faction, 3, 3, {"test_flight_chassis"}, &withComplex, &withComplex);
    Unit& airBuiltElsewhere =
        fixture.MakeUnit(faction, 5, 5, {"test_flight_chassis"}, &otherBase, &otherBase);
    Unit& airNoBase = fixture.MakeUnit(faction, 4, 4, {"test_flight_chassis"});
    // Home reassigned away from the production base must not strip train XP already granted.
    Unit& airRehomed = fixture.MakeUnit(faction, 7, 7, {"test_flight_chassis"}, &withComplex,
                                        &withComplex);
    airRehomed.SetHomeBase(&otherBase);

    CHECK(airBuiltThere.GetXp() == 4); // intrinsic + prototype + Aerospace
    CHECK(airBuiltElsewhere.GetXp() == 1); // chassis already fielded; wrong base for Aerospace
    CHECK(airNoBase.GetXp() == 1);
    CHECK(airRehomed.GetHomeBase() == &otherBase);
    CHECK(airRehomed.GetXp() == 3); // Aerospace only; chassis already fielded
}

TEST_CASE("FactionUnits condition HasComponent: only matching designs receive the bonus",
          "[effects][routing][condition]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    Unit& armed = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"}, &base);
    Unit& unarmed = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    CHECK(armed.GetStat(StatId_t::Attack) == 4);
    CHECK(unarmed.GetStat(StatId_t::Attack) == 0);

    base.GetBuildingManager().AddBuilding("component_attack_array"); // +1 attack if HasComponent test_weapon
    CHECK(armed.GetStat(StatId_t::Attack) == 5);
    CHECK(unarmed.GetStat(StatId_t::Attack) == 0);
}

TEST_CASE("WorldGlobal lane: one faction's WorldGlobal effect reaches other factions' bases",
          "[effects][routing][world]")
{
    actest::FactionFixture fixture;
    GameSettings settings;
    GameState state(std::make_unique<WorldMap>(9, 9, actest::TestMapRules()), fixture.improvements, &fixture.unitComponents,
                    settings, *fixture.dataContext.moraleCalculator, fixture.dataContext.tileYieldRules, fixture.dataContext.interactionGrids, actest::k_TestRngSeed);

    Faction& factionA = state.AddFaction(std::make_unique<Faction>(
                                               1, /*bIsPlayerControlled*/ true, fixture.factionDefinition,
                                               fixture.dataContext,
                                               state.GetWorldMap(), fixture.settings,
                                               actest::k_TestFactionSeed));
    Faction& factionB = state.AddFaction(std::make_unique<Faction>(
                                               2, /*bIsPlayerControlled*/ false, fixture.factionDefinition,
                                               fixture.dataContext,
                                               state.GetWorldMap(), fixture.settings,
                                               actest::k_TestFactionSeed));

    BaseManager& baseA = fixture.MakeFactionBase(factionA, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(factionB, 6, 6);

    baseA.GetBuildingManager().AddBuilding("world_beacon"); // +10 energy, WorldGlobal
    // HQ on each base so inefficiency does not mask the WorldGlobal energy under test.
    baseA.GetBuildingManager().AddBuilding("Headquarters");
    baseB.GetBuildingManager().AddBuilding("Headquarters");

    SECTION("CollectWorldExtras gathers the other faction's WorldGlobal effects only")
    {
        const auto forB = state.CollectWorldExtras(factionB);
        CHECK(ResolveStatModifiers(FilterByStatId(forB, StatId_t::Energy), 0.0).total == Approx(10.0));

        // Faction A's own pool already carries its WorldGlobal effects; the world pass
        // excludes them so they are not double-counted.
        CHECK(state.CollectWorldExtras(factionA).empty());
    }

    SECTION("faction B's base production includes faction A's WorldGlobal energy")
    {
        factionB.ProduceBaseResources();
        // 10 energy split 40/50/10 -> econ 4.
        CHECK(baseB.GetResources().ConsumeEcon() == 4);
        // Live getter agrees with the banked path (same composed pool).
        CHECK(baseB.GetEconProduction() == 4);
    }

    SECTION("faction A's own base gets its own WorldGlobal effect through its own pool")
    {
        factionA.ProduceBaseResources();
        CHECK(baseA.GetEconProduction() == 4);
    }

    SECTION("peer WorldGlobal invalidates the observing base memo")
    {
        CHECK(baseB.GetEconProduction() == 4);
        baseA.GetBuildingManager().AddBuilding("world_beacon"); // second +10 energy
        CHECK(baseB.GetEconProduction() == 8);
    }
}

TEST_CASE("WorldGlobal lane: a peer unit-domain modifier changes the other faction's unit",
          "[effects][routing][world][unit]")
{
    actest::FactionFixture fixture;
    GameSettings settings;
    GameState state(std::make_unique<WorldMap>(9, 9, actest::TestMapRules()), fixture.improvements, &fixture.unitComponents,
                    settings, *fixture.dataContext.moraleCalculator, fixture.dataContext.tileYieldRules,
                    fixture.dataContext.interactionGrids, actest::k_TestRngSeed);

    Faction& factionA = state.AddFaction(std::make_unique<Faction>(
        1, true, fixture.factionDefinition, fixture.dataContext, state.GetWorldMap(),
        fixture.settings, actest::k_TestFactionSeed));
    Faction& factionB = state.AddFaction(std::make_unique<Faction>(
        2, false, fixture.factionDefinition, fixture.dataContext, state.GetWorldMap(),
        fixture.settings, actest::k_TestFactionSeed));

    BaseManager& baseA = fixture.MakeFactionBase(factionA, 2, 2);
    Unit& unitB = fixture.MakeUnit(factionB, 6, 6, {"test_weapon"});
    const int attackBefore = unitB.GetStat(StatId_t::Attack);

    baseA.GetBuildingManager().AddBuilding("world_beacon"); // +10 energy, base domain
    CHECK(unitB.GetStat(StatId_t::Attack) == attackBefore);

    baseA.GetBuildingManager().AddBuilding("world_unit_attack"); // +1 attack, unit domain
    CHECK(unitB.GetStat(StatId_t::Attack) == attackBefore + 1);
}

TEST_CASE("WorldGlobal lane: tile defense applies and base yield stats stay on the base",
          "[effects][routing][world][tile]")
{
    actest::FactionFixture fixture;
    GameSettings settings;
    GameState state(std::make_unique<WorldMap>(9, 9, actest::TestMapRules()), fixture.improvements, &fixture.unitComponents,
                    settings, *fixture.dataContext.moraleCalculator, fixture.dataContext.tileYieldRules,
                    fixture.dataContext.interactionGrids, actest::k_TestRngSeed);

    Faction& factionA = state.AddFaction(std::make_unique<Faction>(
        1, true, fixture.factionDefinition, fixture.dataContext, state.GetWorldMap(),
        fixture.settings, actest::k_TestFactionSeed));
    BaseManager& baseA = fixture.MakeFactionBase(factionA, 2, 2);

    Tile& tile = *state.GetWorldMap().GetTile(4, 4);
    const TileResources_t yieldBefore =
        state.GetTileEffects().ResolveTileYield(tile).effective;
    CHECK(state.GetTileEffects().ResolveTileDefenseMultiplier(tile, factionA.GetFactionId())
          == Approx(1.0));

    baseA.GetBuildingManager().AddBuilding("world_beacon");
    baseA.GetBuildingManager().AddBuilding("world_tile_defense");

    const TileResources_t yieldAfter =
        state.GetTileEffects().ResolveTileYield(tile).effective;
    CHECK(yieldAfter.nutrients == yieldBefore.nutrients);
    CHECK(yieldAfter.energy == yieldBefore.energy);
    CHECK(yieldAfter.minerals == yieldBefore.minerals);
    CHECK(state.GetTileEffects().ResolveTileDefenseMultiplier(tile, factionA.GetFactionId())
          == Approx(1.5));
}

TEST_CASE("WorldGlobal lane: a move_cost clamp applies when entering a tile",
          "[effects][routing][world][movement]")
{
    actest::FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    MoveCostCalculator calc(fixture.improvements);
    const auto costs = calc.ForUnit(unit, fixture.map);

    Tile& rocky = fixture.At(5, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    const int rockyCost = 2 * MovementConstants_t::k_moveFragmentsPerPoint;
    CHECK(costs.EntryTerms(rocky).costFragments == rockyCost);

    base.GetBuildingManager().AddBuilding("world_move_clamp");
    CHECK(costs.EntryTerms(rocky).costFragments == 0);
}

TEST_CASE("Social policy stat effects flow through the standard pool (no special-casing)",
          "[effects][routing][social]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 2, 2);

    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("wealth_policy"));

    const auto pool = CollectActiveEffects(faction);
    CHECK(ResolveStatModifiers(FilterByStatId(pool.effects, StatId_t::Energy), 0.0).total == Approx(1.0));
}

TEST_CASE("Unit aura: a sensor-pod unit projects its ThisTile defense bonus within its radius",
          "[effects][routing][aura]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"sensor_pod"}); // +25% defense, radius 2

    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(4, 4), faction.GetFactionId()) == Approx(1.25)); // own tile
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(6, 4), faction.GetFactionId()) == Approx(1.25)); // Chebyshev 2
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(6, 6), faction.GetFactionId()) == Approx(1.25)); // Chebyshev 2 diagonal
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(7, 4), faction.GetFactionId()) == Approx(1.0));  // Chebyshev 3

    SECTION("the aura moves with the unit")
    {
        fixture.MoveUnit(unit, 0, 8);
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(6, 4), faction.GetFactionId()) == Approx(1.0));
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(1, 8), faction.GetFactionId()) == Approx(1.25));
    }

    SECTION("the aura disappears with the unit")
    {
            faction.GetUnitManager().DestroyUnit(unit);
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(4, 4), faction.GetFactionId()) == Approx(1.0));
    }
}

TEST_CASE("Unit aura: ThisUnit effects on a unit's components never leak into tile resolution",
          "[effects][routing][aura]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    fixture.MakeUnit(faction, 4, 4, {"test_weapon", "test_chassis"});

    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(4, 4), faction.GetFactionId()) == Approx(1.0));
    const TileResources_t yield = fixture.ctx->ResolveTileYield(fixture.At(4, 4)).effective;
    CHECK(yield.nutrients == 0);
    CHECK(yield.minerals == 0);
    CHECK(yield.energy == 0);
}

TEST_CASE("Unit aura wraps horizontally across the map seam",
          "[effects][routing][aura][wrap]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();

    fixture.MakeUnit(faction, 0, 4, {"sensor_pod"}); // +25% defense, radius 2

    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(width - 1, 4), faction.GetFactionId())
          == Approx(1.25));
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(width - 2, 4), faction.GetFactionId())
          == Approx(1.25));
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(width - 3, 4), faction.GetFactionId())
          == Approx(1.0));
}
