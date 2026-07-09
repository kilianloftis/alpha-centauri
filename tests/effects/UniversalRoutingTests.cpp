// Tests for universal scope routing: an effect declared on ANY config is routed by its
// scope, regardless of which source declared it.
//
// Lanes covered here: faction pool contributions from pops and unit components,
// FactionUnits -> live unit stats, WorldGlobal across factions, and unit-projected
// ThisTile auras.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/GameState.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/units/Unit.h"
#include "lib/effects/ActiveEffect.h"

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
    CHECK(ResolveStatModifiers(FilterByStatId(pool.effects, StatId::Labs), 0.0).total == Approx(1.0));

    // Two Networkers stack.
    base.GetPopulation().AddPop("Networker");
    const auto pool2 = CollectActiveEffects(faction);
    CHECK(ResolveStatModifiers(FilterByStatId(pool2.effects, StatId::Labs), 0.0).total == Approx(2.0));
}

TEST_CASE("Faction pool: a unit component's FactionGlobal effect applies while the unit lives",
          "[effects][routing][unit]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    CHECK(ResolveStatModifiers(FilterByStatId(CollectActiveEffects(faction).effects, StatId::Energy), 0.0).total
          == Approx(0.0));

    // energy_siphon: "+1 energy, FactionGlobal" on a component.
    Unit& unit = fixture.MakeUnit(faction, 0, 0, {"energy_siphon"});
    CHECK(ResolveStatModifiers(FilterByStatId(CollectActiveEffects(faction).effects, StatId::Energy), 0.0).total
          == Approx(1.0));

    // The effect disappears with the unit.
    fixture.map.OnUnitRemoved(unit);
    faction.GetUnitManager().DestroyUnit(unit);
    CHECK(ResolveStatModifiers(FilterByStatId(CollectActiveEffects(faction).effects, StatId::Energy), 0.0).total
          == Approx(0.0));
}

TEST_CASE("FactionUnits lane: a building's FactionUnits stat modifier boosts live unit stats",
          "[effects][routing][unit]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_weapon"}); // 4 attack intrinsic
    CHECK(unit.GetAttack() == 4);

    base.GetBuildingManager().AddBuilding("unit_attack_array"); // +25% attack, FactionUnits
    CHECK(unit.GetAttack() == 5); // 4 * 1.25

    // The design (unit-designer view) keeps showing intrinsic values.
    CHECK(unit.GetDesign().GetAttack() == 4);

    base.GetBuildingManager().DestroyBuilding("unit_attack_array");
    CHECK(unit.GetAttack() == 4);
}

TEST_CASE("FactionUnits lane: a building's FactionUnits rule flag applies to live units",
          "[effects][routing][unit]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    CHECK_FALSE(unit.IsFlight());

    base.GetBuildingManager().AddBuilding("flight_grantor"); // RuleFlag flight, FactionUnits
    CHECK(unit.IsFlight());
    CHECK_FALSE(unit.GetDesign().IsFlight()); // intrinsic design unchanged
}

TEST_CASE("WorldGlobal lane: one faction's WorldGlobal effect reaches other factions' bases",
          "[effects][routing][world]")
{
    actest::FactionFixture fixture;
    GameState state(std::make_unique<WorldMap>(9, 9), fixture.improvements, &fixture.unitComponents);

    Faction& factionA = state.AddFaction(std::make_unique<Faction>(fixture.factionDefinition,
                                               &fixture.buildings(), nullptr, &fixture.socialPolicies(),
                                               &fixture.socialRatings(), nullptr, nullptr));
    Faction& factionB = state.AddFaction(std::make_unique<Faction>(fixture.factionDefinition,
                                               &fixture.buildings(), nullptr, &fixture.socialPolicies(),
                                               &fixture.socialRatings(), nullptr, nullptr));

    BaseManager& baseA = fixture.MakeFactionBase(factionA, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(factionB, 6, 6);

    baseA.GetBuildingManager().AddBuilding("world_beacon"); // +10 energy, WorldGlobal

    SECTION("CollectWorldEffects gathers the other faction's WorldGlobal effects only")
    {
        const auto forB = state.CollectWorldEffects(factionB);
        CHECK(ResolveStatModifiers(FilterByStatId(forB, StatId::Energy), 0.0).total == Approx(10.0));

        // Faction A's own pool already carries its WorldGlobal effects; the world pass
        // excludes them so they are not double-counted.
        CHECK(state.CollectWorldEffects(factionA).empty());
    }

    SECTION("faction B's base production includes faction A's WorldGlobal energy")
    {
        factionB.ProduceBaseResources(state.CollectWorldEffects(factionB));
        // 10 energy split 40/50/10 -> econ 4.
        CHECK(baseB.GetResources().ConsumeEcon() == 4);
    }

    SECTION("faction A's own base gets its own WorldGlobal effect through its own pool")
    {
        factionA.ProduceBaseResources(state.CollectWorldEffects(factionA));
        CHECK(baseA.GetEconProduction() == 4);
    }
}

TEST_CASE("Social policy stat effects flow through the standard pool (no special-casing)",
          "[effects][routing][social]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 2, 2);

    REQUIRE(faction.GetSocialEngineering().SetActivePolicy(SocialCategory::Economics, "wealth_policy"));

    const auto pool = CollectActiveEffects(faction);
    CHECK(ResolveStatModifiers(FilterByStatId(pool.effects, StatId::Energy), 0.0).total == Approx(1.0));
}

TEST_CASE("Unit aura: a sensor-pod unit projects its ThisTile defense bonus within its radius",
          "[effects][routing][aura]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"sensor_pod"}); // +25% defense, radius 2

    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(4, 4)) == Approx(1.25)); // own tile
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(6, 4)) == Approx(1.25)); // distance 2
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(7, 4)) == Approx(1.0));  // distance 3

    SECTION("the aura moves with the unit")
    {
        fixture.MoveUnit(unit, 0, 8);
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(6, 4)) == Approx(1.0));
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(1, 8)) == Approx(1.25));
    }

    SECTION("the aura disappears with the unit")
    {
        fixture.map.OnUnitRemoved(unit);
        faction.GetUnitManager().DestroyUnit(unit);
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(4, 4)) == Approx(1.0));
    }
}

TEST_CASE("Unit aura: ThisUnit effects on a unit's components never leak into tile resolution",
          "[effects][routing][aura]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    fixture.MakeUnit(faction, 4, 4, {"test_weapon", "test_chassis"});

    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(4, 4)) == Approx(1.0));
    const TileResources_t yield = fixture.ctx->ResolveTileYield(fixture.At(4, 4));
    CHECK(yield.nutrients == 0);
    CHECK(yield.minerals == 0);
    CHECK(yield.energy == 0);
}
