#include <catch2/catch_test_macros.hpp>
#include "GameFixtures.h"
#include "game/units/MovementRules.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/TransportRules.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/MapUtils.h"
#include "game/map/UnitPositionIndex.h"
#include "game/effects/EffectEnums.h"
#include "game/GameState.h"
#include "game/GameSettings.h"
#include "game/stages/TurnStart.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <memory>
#include <random>
#include <variant>

using namespace ac;
using namespace actest;

namespace
{

constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;

struct MovementHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    std::mt19937 rng;
    UnitOrderExecutor orders;

    explicit MovementHarness_(WorldFixture& fixture)
        : moveCosts(fixture.improvements)
        , steps(fixture.map, *fixture.ctx)
        , pathfinder(moveCosts, steps, fixture.map)
        , orders(moveCosts, steps, fixture.map, *fixture.ctx, pathfinder, fixture.morale(), rng)
    {
    }
};

void MakeLand_(Tile& rTile)
{
    rTile.SetElevation(100);
}

void MakeWater_(Tile& rTile)
{
    rTile.SetElevation(-100);
}

void FillLand_(WorldFixture& fixture)
{
    for (auto& pTile : fixture.map.GetTiles())
    {
        MakeLand_(*pTile);
    }
}

} // namespace

TEST_CASE("Step requires adjacency and spends one move", "[movement]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    REQUIRE(unit.GetMoveFragmentsRemaining() == 2 * k_point);

    CHECK_FALSE(move.steps.CanStep(unit, unit.GetTile(), fixture.At(6, 4))); // not adjacent
    MoveOrder_t stepOrder{&fixture.At(5, 4)};
    REQUIRE(move.orders.TryStep(unit, fixture.At(5, 4), stepOrder).bEntered);
    CHECK(unit.GetTile().GetX() == 5);
    CHECK(unit.GetMoveFragmentsRemaining() == k_point);
}

TEST_CASE("Land cannot enter water; sea cannot enter land", "[movement][domain]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    MakeWater_(fixture.At(5, 4));

    Faction& faction = fixture.MakeFaction();
    Unit& land = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    CHECK_FALSE(move.steps.CanStep(land, land.GetTile(), fixture.At(5, 4)));

    MakeWater_(fixture.At(4, 5));
    MakeWater_(fixture.At(5, 5));
    Unit& sea = fixture.MakeUnit(faction, 4, 5, {"test_sea_chassis"});
    CHECK(sea.GetDomain() == UnitDomain_t::Sea);
    CHECK_FALSE(move.steps.CanStep(sea, sea.GetTile(), fixture.At(4, 4)));
    CHECK(move.steps.CanStep(sea, sea.GetTile(), fixture.At(5, 5)));
}

TEST_CASE("Land may enter friendly sea base or transport; Amphibious is not open ocean",
          "[movement][domain][amphibious]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();

    MakeWater_(fixture.At(5, 4));
    MakeWater_(fixture.At(5, 5));
    MakeWater_(fixture.At(4, 5));

    SECTION("friendly sea base")
    {
        fixture.MakeFactionBase(faction, 5, 4);
        Unit& land = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
        CHECK(move.steps.CanStep(land, land.GetTile(), fixture.At(5, 4)));
    }

    SECTION("friendly transport (sea unit with cargo capacity)")
    {
        fixture.MakeUnit(faction, 5, 4, {"test_sea_chassis", "test_transport"});
        Unit& land = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
        CHECK(move.steps.CanStep(land, land.GetTile(), fixture.At(5, 4)));
    }

    SECTION("non-transport sea unit does not allow water entry")
    {
        fixture.MakeUnit(faction, 5, 4, {"test_sea_chassis"});
        Unit& land = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
        CHECK_FALSE(move.steps.CanStep(land, land.GetTile(), fixture.At(5, 4)));
    }

    SECTION("Amphibious does not grant empty-water movement")
    {
        Unit& amph = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_amphibious"});
        CHECK_FALSE(move.steps.CanStep(amph, amph.GetTile(), fixture.At(5, 4)));
        CHECK_FALSE(CanEnterTileTerrain(amph, fixture.At(5, 4)));
        CHECK_FALSE(CanEnterTile(amph, fixture.At(5, 4), fixture.map));
    }
}

TEST_CASE("Air can enter land or water", "[movement][domain]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    MakeWater_(fixture.At(5, 4));
    Faction& faction = fixture.MakeFaction();
    Unit& flyer = fixture.MakeUnit(faction, 4, 4, {"test_flight_chassis"});
    CHECK(flyer.GetDomain() == UnitDomain_t::Air);
    CHECK(move.steps.CanStep(flyer, flyer.GetTile(), fixture.At(5, 4)));
    CHECK(move.steps.CanStep(flyer, flyer.GetTile(), fixture.At(4, 5)));
}

TEST_CASE("Enter ZOC allowed; ZOC to ZOC blocked; leave ZOC allowed", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Enemy at (5,4) — ZOC covers (4,4),(4,5),(5,5),(6,4),...
    fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 3, 4, {"test_chassis"});

    // Enter ZOC: (3,4) -> (4,4)
    MoveOrder_t enterZoc{&fixture.At(4, 4)};
    REQUIRE(move.orders.TryStep(mover, fixture.At(4, 4), enterZoc).bEntered);
    CHECK(move.steps.IsTileInHostileZoc(mover, mover.GetTile()));

    // ZOC -> ZOC: (4,4) -> (4,5) both in ZOC of enemy at (5,4)
    CHECK(move.steps.IsTileInHostileZoc(mover, fixture.At(4, 5)));
    CHECK_FALSE(move.steps.CanStep(mover, mover.GetTile(), fixture.At(4, 5)));

    // Leave ZOC: (4,4) -> (3,3) — (3,3) is Chebyshev 2 from enemy, outside ZOC
    CHECK_FALSE(move.steps.IsTileInHostileZoc(mover, fixture.At(3, 3)));
    CHECK(move.steps.CanStep(mover, mover.GetTile(), fixture.At(3, 3)));
}

TEST_CASE("ZOC to ZOC allowed onto friendly unit or base", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Enemy at (5,4) — ZOC covers (4,4) and (4,5).
    fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});

    SECTION("friendly unit on destination")
    {
        fixture.MakeUnit(player, 4, 5, {"test_chassis"});
        Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
        REQUIRE(move.steps.IsTileInHostileZoc(mover, mover.GetTile()));
        REQUIRE(move.steps.IsTileInHostileZoc(mover, fixture.At(4, 5)));
        CHECK(move.steps.CanStep(mover, mover.GetTile(), fixture.At(4, 5)));
        CHECK_FALSE(move.steps.IsZocViolation(mover, mover.GetTile(), fixture.At(4, 5)));
    }

    SECTION("friendly base on destination")
    {
        fixture.MakeFactionBase(player, 4, 5);
        Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
        REQUIRE(move.steps.IsTileInHostileZoc(mover, mover.GetTile()));
        REQUIRE(move.steps.IsTileInHostileZoc(mover, fixture.At(4, 5)));
        CHECK(move.steps.CanStep(mover, mover.GetTile(), fixture.At(4, 5)));
        CHECK_FALSE(move.steps.IsZocViolation(mover, mover.GetTile(), fixture.At(4, 5)));
    }

    SECTION("enemy base alone does not exempt")
    {
        fixture.MakeFactionBase(enemy, 4, 5);
        Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
        REQUIRE(move.steps.IsTileInHostileZoc(mover, mover.GetTile()));
        REQUIRE(move.steps.IsTileInHostileZoc(mover, fixture.At(4, 5)));
        CHECK(move.steps.IsZocViolation(mover, mover.GetTile(), fixture.At(4, 5)));
        CHECK_FALSE(move.steps.CanStep(mover, mover.GetTile(), fixture.At(4, 5)));
    }
}

TEST_CASE("Attack is adjacent; hostiles never share a tile", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
    mover.SetOrder(MoveOrder_t{&fixture.At(5, 4)});

    REQUIRE(move.steps.IsTileInHostileZoc(mover, mover.GetTile()));
    CHECK_FALSE(move.steps.CanStep(mover, mover.GetTile(), fixture.At(5, 4)));
    MoveOrder_t bumpHostile{&fixture.At(5, 4)};
    CHECK_FALSE(move.orders.TryStep(mover, fixture.At(5, 4), bumpHostile).bEntered);

    const auto result = move.orders.TryAttack(mover, fixture.At(5, 4));
    REQUIRE(result.has_value());
    CHECK(result->bDefenderDestroyed);
    CHECK_FALSE(result->bAttackerDestroyed);
    CHECK(mover.GetMoveFragmentsRemaining() == 0);
    CHECK_FALSE(mover.GetOrder().has_value());
    CHECK(mover.GetTile().GetX() == 4); // still on own tile
}

TEST_CASE("Cannot attack a cloaked hostile until contact-revealed",
          "[movement][visibility][reveal]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& cloaked = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "Cloaking_Device"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
    player.RebuildVisibility();

    REQUIRE(player.GetVisibleMap().IsVisible(cloaked.GetTile()));
    REQUIRE_FALSE(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    CHECK_FALSE(move.orders.TryAttack(mover, fixture.At(5, 4)));
    CHECK(mover.GetMoveFragmentsRemaining() == 2 * k_point);

    // Bumping the occupied tile reveals the cloaked unit.
    MoveOrder_t bumpCloaked{&fixture.At(5, 4)};
    CHECK_FALSE(move.orders.TryStep(mover, fixture.At(5, 4), bumpCloaked).bEntered);
    CHECK(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    REQUIRE(move.orders.TryAttack(mover, fixture.At(5, 4)));
}

TEST_CASE("ZOC block from a cloaked unit contact-reveals it",
          "[movement][visibility][reveal][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& cloaked = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "Cloaking_Device"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
    player.RebuildVisibility();

    REQUIRE_FALSE(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    // ZOC -> ZOC toward (4,5) is blocked by the cloaked projector.
    MoveOrder_t zocBump{&fixture.At(4, 5)};
    CHECK_FALSE(move.orders.TryStep(mover, fixture.At(4, 5), zocBump).bEntered);
    CHECK(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    CHECK(move.orders.TryAttack(mover, fixture.At(5, 4)));
}

TEST_CASE("Move order into cloaked unit reveals via desired-step bump",
          "[movement][visibility][reveal][orders]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& cloaked = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "Cloaking_Device"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    player.RebuildVisibility();
    mover.SetOrder(MoveOrder_t{&fixture.At(5, 4)});

    REQUIRE_FALSE(IsUnitVisibleTo(player, cloaked, *fixture.ctx));

    move.orders.Execute(mover);

    CHECK(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    CHECK(mover.GetTile().GetX() == 4); // did not enter the hostile tile
    CHECK_FALSE(mover.GetOrder().has_value()); // contact-reveal cancelled the move
}

TEST_CASE("Move order cancels when fog of war reveals a hostile",
          "[movement][visibility][fog][orders]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Vision 1: enemy two tiles east is fogged until the mover steps closer.
    Unit& hostile = fixture.MakeUnit(enemy, 6, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    player.RebuildVisibility();
    mover.SetOrder(MoveOrder_t{&fixture.At(6, 4)});

    REQUIRE_FALSE(IsUnitVisibleTo(player, hostile, *fixture.ctx));
    const int distBefore = ChebyshevDistance(mover.GetTile(), fixture.At(6, 4), fixture.map.GetWidth());

    move.orders.Execute(mover);

    CHECK(ChebyshevDistance(mover.GetTile(), fixture.At(6, 4), fixture.map.GetWidth()) == distBefore - 1);
    CHECK(IsUnitVisibleTo(player, hostile, *fixture.ctx));
    CHECK_FALSE(mover.GetOrder().has_value());
}

TEST_CASE("EvaluateStep attributes occupant and ZOC blockers",
          "[movement][evaluate]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& hostile = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});

    SECTION("occupied tile")
    {
        const StepEvaluation_t eval = move.steps.EvaluateStep(mover, mover.GetTile(), fixture.At(5, 4));
        CHECK(eval.outcome == StepOutcome_t::BlockedByOccupant);
        REQUIRE(eval.blockingUnits.size() == 1);
        CHECK(eval.blockingUnits[0] == &hostile);
    }

    SECTION("ZOC to ZOC")
    {
        const StepEvaluation_t eval = move.steps.EvaluateStep(mover, mover.GetTile(), fixture.At(4, 5));
        CHECK(eval.outcome == StepOutcome_t::BlockedByZoc);
        REQUIRE_FALSE(eval.blockingUnits.empty());
        CHECK(std::find(eval.blockingUnits.begin(), eval.blockingUnits.end(), &hostile)
              != eval.blockingUnits.end());
    }

    SECTION("legal leave-ZOC step")
    {
        const StepEvaluation_t eval = move.steps.EvaluateStep(mover, mover.GetTile(), fixture.At(3, 3));
        CHECK(eval.outcome == StepOutcome_t::Legal);
        CHECK(eval.blockingUnits.empty());
    }

    SECTION("not adjacent")
    {
        CHECK(move.steps.EvaluateStep(mover, mover.GetTile(), fixture.At(6, 4)).outcome
              == StepOutcome_t::NotAdjacent);
    }

    SECTION("TryStep rejects when out of moves")
    {
        mover.SetMoveFragmentsRemaining(0);
        MoveOrder_t order{&fixture.At(4, 5)};
        CHECK_FALSE(move.orders.TryStep(mover, fixture.At(4, 5), order).bEntered);
        CHECK(&mover.GetTile() == &fixture.At(4, 4));
    }
}

TEST_CASE("Land ignores sea ZOC; sea ignores land ZOC", "[movement][zoc][domain]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    MakeWater_(fixture.At(5, 4));
    MakeWater_(fixture.At(5, 5));
    MakeWater_(fixture.At(4, 5));
    MakeWater_(fixture.At(6, 5));

    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    SECTION("sea projector does not affect land")
    {
        fixture.MakeUnit(enemy, 5, 4, {"test_sea_chassis"});
        Unit& land = fixture.MakeUnit(player, 6, 4, {"test_chassis"});
        CHECK_FALSE(move.steps.IsTileInHostileZoc(land, land.GetTile()));
        // Adjacent tiles that would be ZOC-to-ZOC if sea affected land remain legal.
        CHECK(move.steps.CanStep(land, land.GetTile(), fixture.At(6, 3)));
        CHECK(move.steps.CanStep(land, land.GetTile(), fixture.At(7, 4)));
    }

    SECTION("land projector does not affect sea")
    {
        fixture.MakeUnit(enemy, 4, 4, {"test_chassis"});
        Unit& sea = fixture.MakeUnit(player, 4, 5, {"test_sea_chassis"});
        CHECK_FALSE(move.steps.IsTileInHostileZoc(sea, sea.GetTile()));
        CHECK(move.steps.CanStep(sea, sea.GetTile(), fixture.At(5, 5)));
    }
}

TEST_CASE("Air ignores ZOC but exerts on land", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    fixture.MakeUnit(enemy, 5, 4, {"test_flight_chassis"});
    Unit& land = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    CHECK(move.steps.IsTileInHostileZoc(land, land.GetTile()));
    CHECK_FALSE(move.steps.CanStep(land, land.GetTile(), fixture.At(4, 5))); // ZOC -> ZOC

    fixture.MakeUnit(enemy, 5, 6, {"test_chassis"});
    Unit& flyer = fixture.MakeUnit(player, 4, 6, {"test_flight_chassis"});
    CHECK_FALSE(move.steps.IsTileInHostileZoc(flyer, flyer.GetTile()));
    CHECK(move.steps.CanStep(flyer, flyer.GetTile(), fixture.At(4, 5))); // would be ZOC->ZOC for land
}

TEST_CASE("IgnoreZoneOfControl flag bypasses ZOC", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& probe = fixture.MakeUnit(player, 4, 4, {"test_chassis", "ignore_zoc"});
    CHECK(probe.GetFlag(RuleFlagId_t::IgnoreZoneOfControl));
    CHECK_FALSE(move.steps.IsTileInHostileZoc(probe, probe.GetTile()));
    CHECK(move.steps.CanStep(probe, probe.GetTile(), fixture.At(4, 5)));
}

TEST_CASE("UnitOrderExecutor advances until moves run out", "[movement][orders]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(5, 4);
    unit.SetOrder(MoveOrder_t{&rDest});
    REQUIRE(unit.GetMoveFragmentsRemaining() == 2 * k_point); // 2 steps worth

    move.orders.Execute(unit);

    // 3 tiles away, 2 move points → advances 2 steps and runs out of moves.
    CHECK(ChebyshevDistance(unit.GetTile(), rDest, fixture.map.GetWidth()) == 1);
    CHECK(unit.GetMoveFragmentsRemaining() == 0);
    REQUIRE(unit.GetOrder().has_value()); // not at destination yet
}

TEST_CASE("Pathfinder NextStep respects ZOC", "[movement][zoc][pathfinding]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Mover pinned at the north map edge in ZOC (Y does not wrap): no legal exit
    // toward dest behind the enemy.
    fixture.MakeUnit(enemy, 4, 1, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 4, 0, {"test_chassis"});

    CHECK(move.pathfinder.NextStep(mover, fixture.At(4, 2)) == nullptr);
}

TEST_CASE("TryStep spends tile move-cost fragments", "[movement][move-cost]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    REQUIRE(unit.GetMoveFragmentsRemaining() == 2 * k_point);

    Tile& rocky = fixture.At(5, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    MoveOrder_t rockyOrder{&rocky};
    REQUIRE(move.orders.TryStep(unit, rocky, rockyOrder).bEntered);
    CHECK(unit.GetMoveFragmentsRemaining() == 0); // 2-point rocky drains a 2-move unit

    Unit& roadUnit = fixture.MakeUnit(faction, 4, 5, {"test_chassis"});
    Tile& road = fixture.At(5, 5);
    road.AddImprovement(fixture.improvements.Get("Road"));
    MoveOrder_t roadOrder{&road};
    REQUIRE(move.orders.TryStep(roadUnit, road, roadOrder).bEntered);
    CHECK(roadUnit.GetMoveFragmentsRemaining() == 2 * k_point - k_point / 3);

    // Any fragments left suffice even when tile cost is higher; remaining zeroes (clamped).
    Unit& lastFragments = fixture.MakeUnit(faction, 6, 4, {"test_chassis"});
    lastFragments.SetMoveFragmentsRemaining(1);
    Tile& rocky2 = fixture.At(7, 4);
    rocky2.SetRockiness(Rockiness_t::Rocky);
    MoveOrder_t rocky2Order{&rocky2};
    REQUIRE(move.orders.TryStep(lastFragments, rocky2, rocky2Order).bEntered);
    CHECK(lastFragments.GetMoveFragmentsRemaining() == 0);
}

TEST_CASE("TurnStart restores move fragments", "[movement][turn]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    unit.SetMoveFragmentsRemaining(0);

    GameSettings settings;
    GameState state(std::make_unique<WorldMap>(3, 3), fixture.improvements, &fixture.unitComponents,
                    settings, *fixture.dataContext.moraleCalculator);
    state.AddFaction(std::move(fixture.factions[0]));

    TurnStart stage(HookContext{});
    stage.Execute(state);

    CHECK(unit.GetMoveFragmentsRemaining() == 2 * k_point);
}

TEST_CASE("Fungus entry charges across turns until cost is paid", "[movement][fungus]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_slow_chassis"});
    REQUIRE(unit.GetMovementPoints() == 1);

    Tile& fungus = fixture.At(5, 4);
    fungus.SetHasFungus(true);
    unit.SetOrder(MoveOrder_t{&fungus});

    // 1-move chassis: three turns to pay fungus cost 3.
    for (int turn = 0; turn < 2; ++turn)
    {
        unit.SetMoveFragmentsRemaining(k_point);
        move.orders.Execute(unit);
        CHECK(&unit.GetTile() != &fungus);
        CHECK(unit.GetMoveFragmentsRemaining() == 0);
        REQUIRE(unit.GetOrder().has_value());
        const auto& rOrder = std::get<MoveOrder_t>(*unit.GetOrder());
        CHECK(rOrder.pChargeTile == &fungus);
        CHECK(rOrder.chargeFragmentsPaid == (turn + 1) * k_point);
    }

    unit.SetMoveFragmentsRemaining(k_point);
    move.orders.Execute(unit);
    CHECK(&unit.GetTile() == &fungus);
    CHECK(unit.GetMoveFragmentsRemaining() == 0);
    CHECK_FALSE(unit.GetOrder().has_value()); // destination reached
}

TEST_CASE("Friendly on fungus allows immediate entry and ends the turn", "[movement][fungus]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    REQUIRE(unit.GetMoveFragmentsRemaining() == 2 * k_point);

    Tile& fungus = fixture.At(5, 4);
    fungus.SetHasFungus(true);
    fixture.MakeUnit(faction, 5, 4, {"test_chassis"}); // friendly already there

    MoveOrder_t stepOrder{&fungus};
    REQUIRE(move.orders.TryStep(unit, fungus, stepOrder).bEntered);
    CHECK(&unit.GetTile() == &fungus);
    CHECK(unit.GetMoveFragmentsRemaining() == 0);

    // The waiver admits any positive balance — no banking even on the last fragment.
    Unit& lastFragment = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    lastFragment.SetMoveFragmentsRemaining(1);
    MoveOrder_t lastOrder{&fungus};
    REQUIRE(move.orders.TryStep(lastFragment, fungus, lastOrder).bEntered);
    CHECK(&lastFragment.GetTile() == &fungus);
    CHECK(lastFragment.GetMoveFragmentsRemaining() == 0);
}

TEST_CASE("Entering fungus ends the turn even with leftover moves", "[movement][fungus]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    Tile& fungus = fixture.At(5, 4);
    fungus.SetHasFungus(true);
    unit.SetOrder(MoveOrder_t{&fungus});

    // Pay 2 of 4 on turn 1 (M=2 charge opportunity for terrain cost 3).
    REQUIRE(unit.GetMoveFragmentsRemaining() == 2 * k_point);
    move.orders.Execute(unit);
    CHECK(&unit.GetTile() != &fungus);

    // Turn 2: finish the charge — entry must still zero remaining.
    unit.SetMoveFragmentsRemaining(2 * k_point);
    move.orders.Execute(unit);
    CHECK(&unit.GetTile() == &fungus);
    CHECK(unit.GetMoveFragmentsRemaining() == 0);
}

TEST_CASE("Road built on fungus negates the entry rules", "[movement][fungus]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    REQUIRE(unit.GetMoveFragmentsRemaining() == 2 * k_point);

    Tile& roadFungus = fixture.At(5, 4);
    roadFungus.SetHasFungus(true);
    roadFungus.AddImprovement(fixture.improvements.Get("Road"));

    MoveOrder_t stepOrder{&roadFungus};
    REQUIRE(move.orders.TryStep(unit, roadFungus, stepOrder).bEntered);
    CHECK(&unit.GetTile() == &roadFungus);
    CHECK(unit.GetMoveFragmentsRemaining() == 2 * k_point - k_point / 3);
}

TEST_CASE("TreatFungusAsRoad uses road cost without forced end-turn", "[movement][fungus]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "treat_fungus_as_road"});
    REQUIRE(unit.GetFlag(RuleFlagId_t::TreatFungusAsRoad));
    REQUIRE(unit.GetMoveFragmentsRemaining() == 2 * k_point);

    Tile& fungus = fixture.At(5, 4);
    fungus.SetHasFungus(true);
    MoveOrder_t stepOrder{&fungus};
    REQUIRE(move.orders.TryStep(unit, fungus, stepOrder).bEntered);
    CHECK(&unit.GetTile() == &fungus);
    CHECK(unit.GetMoveFragmentsRemaining() == 2 * k_point - k_point / 3);
}

TEST_CASE("Step wraps horizontally across the map seam", "[movement][wrap]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& faction = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();
    Unit& unit = fixture.MakeUnit(faction, 0, 4, {"test_chassis"});

    CHECK(move.steps.CanStep(unit, unit.GetTile(), fixture.At(width - 1, 4)));
    CHECK_FALSE(move.steps.CanStep(unit, unit.GetTile(), fixture.At(width - 2, 4)));

    MoveOrder_t wrapStep{&fixture.At(width - 1, 4)};
    REQUIRE(move.orders.TryStep(unit, fixture.At(width - 1, 4), wrapStep).bEntered);
    CHECK(unit.GetTile().GetX() == width - 1);
}

TEST_CASE("Hostile ZOC wraps horizontally across the map seam", "[movement][zoc][wrap]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();

    fixture.MakeUnit(enemy, 0, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, width - 1, 4, {"test_chassis"});

    REQUIRE(move.steps.IsTileInHostileZoc(mover, mover.GetTile()));
    // ZOC-to-ZOC along the seam is blocked.
    CHECK_FALSE(move.steps.CanStep(mover, mover.GetTile(), fixture.At(width - 1, 5)));
    // Leaving to Chebyshev 2 from the enemy (across the wrap) is allowed.
    CHECK_FALSE(move.steps.IsTileInHostileZoc(mover, fixture.At(width - 2, 3)));
    CHECK(move.steps.CanStep(mover, mover.GetTile(), fixture.At(width - 2, 3)));
}

TEST_CASE("Attack wraps horizontally across the map seam", "[movement][zoc][wrap]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MovementHarness_ move(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();

    fixture.MakeUnit(enemy, width - 1, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 0, 4, {"test_chassis", "test_weapon"});

    CHECK_FALSE(move.orders.TryAttack(mover, fixture.At(width - 2, 4))); // not adjacent
    const auto result = move.orders.TryAttack(mover, fixture.At(width - 1, 4));
    REQUIRE(result.has_value());
    CHECK(mover.GetTile().GetX() == 0);
}
