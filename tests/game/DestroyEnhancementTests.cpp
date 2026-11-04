#include "GameFixtures.h"

#include "game/base/BaseManager.h"
#include "game/combat/AttackRules.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MoveCostRules.h"
#include "game/movement/MovementConstants.h"
#include "game/movement/Pathfinder.h"
#include "game/movement/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitAttackExecutor.h"
#include "game/units/UnitManager.h"
#include "game/units/UnitMoveExecutor.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

struct OrderHarness_
{
    MoveCostRules moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    std::mt19937 rng;
    UnitMoveExecutor moves;
    UnitOrderExecutor orders;
    UnitAttackExecutor attacks;

    OrderHarness_(FactionFixture& fixture, uint32_t seed)
        : moveCosts(fixture.improvements, MovementConstants_t{})
        , steps(fixture.map, fixture.ctx)
        , pathfinder(moveCosts, steps, fixture.map)
        , rng(seed)
        , moves(moveCosts, steps, fixture.map, rng, fixture.state)
        , orders(moves, steps, fixture.map, fixture.ctx, pathfinder,
                 *fixture.dataContext.terrainOperationRegistry, rng, fixture.state)
        , attacks(moveCosts, steps, fixture.map, fixture.ctx, fixture.morale(), rng, fixture.state,
                  orders)
    {
    }
};

void FillLand_(WorldFixture& fixture)
{
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
}

void RebuildTerritory_(FactionFixture& rFixture)
{
    std::vector<const BaseManager*> bases;
    for (const Faction& rFaction : rFixture.state.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            bases.push_back(&rBase);
        }
    }
    rFixture.map.GetTerritory().Rebuild(rFixture.map, bases);
}

} // namespace

TEST_CASE("Combat units and formers may destroy enhancements; probes and pods may not",
          "[destroy_enhancement]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Tile& rTile = fixture.At(8, 8);
    rTile.AddImprovement(fixture.improvements.Get("Farm"));

    Unit& combat = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& former = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_terraformer"});
    Unit& probe = fixture.MakeUnit(player, 8, 8, {"test_chassis", "Probe_Team"});
    Unit& pod = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_colony_pod"});

    CHECK(CanDestroyEnhancement(combat));
    CHECK(CanDestroyEnhancement(former));
    CHECK_FALSE(CanDestroyEnhancement(probe));
    CHECK_FALSE(CanDestroyEnhancement(pod));
}

TEST_CASE("Destroy Enhancement skips base tiles and lists non-Base improvements under the unit",
          "[destroy_enhancement]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();

    Tile& rOpen = fixture.At(8, 8);
    rOpen.AddImprovement(fixture.improvements.Get("Farm"));
    rOpen.AddImprovement(fixture.improvements.Get("Sensor"));
    fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    CHECK(DestroyEnhancementTargets(rOpen) == std::vector<std::string>{"Farm", "Sensor"});
    const std::vector<DestroyEnhancementOption_t> options = DestroyEnhancementOptions(rOpen);
    REQUIRE(options.size() == 2);
    CHECK(options[0].improvementId == "Farm");
    CHECK(options[0].label == fixture.improvements.Get("Farm").name);
    CHECK(options[1].improvementId == "Sensor");
    CHECK(options[1].label == fixture.improvements.Get("Sensor").name);
    CHECK(BombardImprovementTargets(rOpen).empty());

    Tile& rBase = fixture.At(9, 9);
    rBase.AddImprovement(fixture.improvements.Get("Farm"));
    rBase.AddImprovement(fixture.improvements.Get("Base"));
    CHECK(DestroyEnhancementTargets(rBase).empty());
    CHECK(DestroyEnhancementOptions(rBase).empty());
    CHECK(BombardImprovementTargets(rBase) == std::vector<std::string>{"Farm"});
}

TEST_CASE("Destroy Enhancement removes the chosen improvement and spends the rest of the turn",
          "[destroy_enhancement]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Tile& rTile = fixture.At(8, 8);
    rTile.AddImprovement(fixture.improvements.Get("Farm"));
    rTile.AddImprovement(fixture.improvements.Get("Sensor"));
    Unit& unit = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    REQUIRE(unit.GetMoveFragmentsRemaining() > 0);
    CHECK(DestroyEnhancementSuccessChance(unit) == 100);
    CHECK(DestroyEnhancementAttempts(unit) == 1);

    OrderHarness_ orders(fixture, 1);
    const auto refused = orders.attacks.TryDestroyEnhancement(unit, {"Mine"});
    CHECK_FALSE(refused.has_value());
    CHECK(rTile.HasImprovement(ImprovementIds::k_Farm));
    CHECK(unit.GetMoveFragmentsRemaining() > 0);

    const auto result = orders.attacks.TryDestroyEnhancement(unit, {"Farm"});
    REQUIRE(result);
    CHECK(result->destroyedImprovementIds == std::vector<std::string>{"Farm"});
    CHECK_FALSE(result->bUnitDestroyed);
    CHECK_FALSE(rTile.HasImprovement(ImprovementIds::k_Farm));
    CHECK(rTile.HasImprovement(ImprovementIds::k_Sensor));
    CHECK(unit.GetMoveFragmentsRemaining() == 0);
}

TEST_CASE("Air Destroy Enhancement success is 75; enemy AA cuts it, own AA does not",
          "[destroy_enhancement]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    fixture.MakeFactionBase(enemy, 12, 12);
    fixture.state.RebuildTerritory();
    REQUIRE(fixture.map.GetTerritory().GetOwner(9, 9) == enemy.GetFactionId());

    Tile& rTile = fixture.At(8, 8);
    rTile.AddImprovement(fixture.improvements.Get("Farm"));
    Unit& air = fixture.MakeUnit(player, 8, 8, {"test_flight_chassis", "test_weapon"});
    CHECK(DestroyEnhancementSuccessChance(air) == 75);

    Tile& rAaTile = fixture.At(9, 9);
    rAaTile.AddImprovement(fixture.improvements.Get("TestAaEmplacement"));
    CHECK(DestroyEnhancementSuccessChance(air) == 25);

    rAaTile.RemoveImprovement("TestAaEmplacement");
    fixture.MakeFactionBase(player, 8, 8);
    fixture.state.RebuildTerritory();
    REQUIRE(fixture.map.GetTerritory().GetOwner(8, 8) == player.GetFactionId());
    rTile.AddImprovement(fixture.improvements.Get("TestAaEmplacement"));
    CHECK(DestroyEnhancementSuccessChance(air) == 75);

    Unit& land = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    CHECK(DestroyEnhancementSuccessChance(land) == 100);
}

TEST_CASE("SingleUse units survive Destroy Enhancement", "[destroy_enhancement]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Tile& rTile = fixture.At(8, 8);
    rTile.AddImprovement(fixture.improvements.Get("Farm"));
    Unit& missile =
        fixture.MakeUnit(player, 8, 8, {"test_orbital_chassis", "test_single_use_weapon"});
    CHECK(DestroyEnhancementSuccessChance(missile) == 100);

    OrderHarness_ orders(fixture, 1);
    const auto result = orders.attacks.TryDestroyEnhancement(missile, {"Farm"});
    REQUIRE(result);
    CHECK_FALSE(result->bUnitDestroyed);
    CHECK(missile.GetMoveFragmentsRemaining() == 0);
    CHECK_FALSE(rTile.HasImprovement(ImprovementIds::k_Farm));
}

TEST_CASE("Destroy Enhancement can attempt several ids with independent rolls",
          "[destroy_enhancement]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Tile& rTile = fixture.At(8, 8);
    rTile.AddImprovement(fixture.improvements.Get("Farm"));
    rTile.AddImprovement(fixture.improvements.Get("Sensor"));
    Unit& unit =
        fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon", "test_multi_destroy"});
    CHECK(DestroyEnhancementAttempts(unit) == 2);

    OrderHarness_ orders(fixture, 1);
    const auto tooMany =
        orders.attacks.TryDestroyEnhancement(unit, {"Farm", "Sensor", "Farm"});
    CHECK_FALSE(tooMany.has_value());

    const auto result = orders.attacks.TryDestroyEnhancement(unit, {"Farm", "Sensor"});
    REQUIRE(result);
    CHECK(result->destroyedImprovementIds.size() == 2);
    CHECK_FALSE(rTile.HasImprovement(ImprovementIds::k_Farm));
    CHECK_FALSE(rTile.HasImprovement(ImprovementIds::k_Sensor));
    CHECK(unit.GetMoveFragmentsRemaining() == 0);
}

TEST_CASE("Destroy Enhancement is a hostile act only on foreign territory",
          "[destroy_enhancement][diplomacy]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    fixture.MakeFactionBase(player, 8, 8);
    fixture.MakeFactionBase(enemy, 14, 8);
    RebuildTerritory_(fixture);

    Tile& rOwnFarm = fixture.At(9, 9);
    REQUIRE(fixture.map.GetTerritory().GetOwner(rOwnFarm) == player.GetFactionId());
    rOwnFarm.AddImprovement(fixture.improvements.Get("Farm"));
    Unit& onOwn = fixture.MakeUnit(player, 9, 9, {"test_chassis", "test_weapon"});
    CHECK_FALSE(DestroyEnhancementHostileAct(onOwn).has_value());

    Tile& rEnemyFarm = fixture.At(13, 9);
    REQUIRE(fixture.map.GetTerritory().GetOwner(rEnemyFarm) == enemy.GetFactionId());
    rEnemyFarm.AddImprovement(fixture.improvements.Get("Farm"));
    Unit& onEnemy = fixture.MakeUnit(player, 13, 9, {"test_chassis", "test_weapon"});
    const std::optional<HostileAct_t> act = DestroyEnhancementHostileAct(onEnemy);
    REQUIRE(act);
    CHECK(act->aggressor == player.GetFactionId());
    CHECK(act->victim == enemy.GetFactionId());
}
