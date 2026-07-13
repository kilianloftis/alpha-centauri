#include <catch2/catch_test_macros.hpp>
#include "GameFixtures.h"
#include "game/units/MovementRules.h"
#include "game/units/MovementConstants.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/MapUtils.h"
#include "game/map/UnitPositionIndex.h"
#include "game/effects/EffectEnums.h"

#include <algorithm>

using namespace ac;
using namespace actest;

namespace
{

constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;

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
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    REQUIRE(unit.GetMovesRemaining() == 2 * k_point);

    CHECK_FALSE(CanStep(unit, fixture.At(6, 4), fixture.map, fixture.improvements)); // not adjacent
    REQUIRE(TryStep(unit, fixture.At(5, 4), fixture.map, fixture.improvements));
    CHECK(unit.GetTile().GetX() == 5);
    CHECK(unit.GetMovesRemaining() == k_point);
}

TEST_CASE("Land cannot enter water; sea cannot enter land", "[movement][domain]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MakeWater_(fixture.At(5, 4));

    Faction& faction = fixture.MakeFaction();
    Unit& land = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    CHECK_FALSE(CanStep(land, fixture.At(5, 4), fixture.map, fixture.improvements));

    MakeWater_(fixture.At(4, 5));
    MakeWater_(fixture.At(5, 5));
    Unit& sea = fixture.MakeUnit(faction, 4, 5, {"test_sea_chassis"});
    CHECK(sea.GetDomain() == UnitDomain_t::Sea);
    CHECK_FALSE(CanStep(sea, fixture.At(4, 4), fixture.map, fixture.improvements));
    CHECK(CanStep(sea, fixture.At(5, 5), fixture.map, fixture.improvements));
}

TEST_CASE("Air can enter land or water", "[movement][domain]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    MakeWater_(fixture.At(5, 4));
    Faction& faction = fixture.MakeFaction();
    Unit& flyer = fixture.MakeUnit(faction, 4, 4, {"test_flight_chassis"});
    CHECK(flyer.GetDomain() == UnitDomain_t::Air);
    CHECK(CanStep(flyer, fixture.At(5, 4), fixture.map, fixture.improvements));
    CHECK(CanStep(flyer, fixture.At(4, 5), fixture.map, fixture.improvements));
}

TEST_CASE("Enter ZOC allowed; ZOC to ZOC blocked; leave ZOC allowed", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Enemy at (5,4) — ZOC covers (4,4),(4,5),(5,5),(6,4),...
    fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 3, 4, {"test_chassis"});

    // Enter ZOC: (3,4) -> (4,4)
    REQUIRE(TryStep(mover, fixture.At(4, 4), fixture.map, fixture.improvements));
    CHECK(IsTileInHostileZoc(mover, mover.GetTile(), fixture.map));

    // ZOC -> ZOC: (4,4) -> (4,5) both in ZOC of enemy at (5,4)
    CHECK(IsTileInHostileZoc(mover, fixture.At(4, 5), fixture.map));
    CHECK_FALSE(CanStep(mover, fixture.At(4, 5), fixture.map, fixture.improvements));

    // Leave ZOC: (4,4) -> (3,3) — (3,3) is Chebyshev 2 from enemy, outside ZOC
    CHECK_FALSE(IsTileInHostileZoc(mover, fixture.At(3, 3), fixture.map));
    CHECK(CanStep(mover, fixture.At(3, 3), fixture.map, fixture.improvements));
}

TEST_CASE("Attack is adjacent; hostiles never share a tile", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    mover.SetOrder(MoveOrder_t{&fixture.At(5, 4)});

    REQUIRE(IsTileInHostileZoc(mover, mover.GetTile(), fixture.map));
    CHECK_FALSE(CanStep(mover, fixture.At(5, 4), fixture.map, fixture.improvements));
    CHECK_FALSE(TryStep(mover, fixture.At(5, 4), fixture.map, fixture.improvements));

    REQUIRE(TryAttack(mover, fixture.At(5, 4), fixture.map, *fixture.ctx));
    CHECK(mover.GetMovesRemaining() == 0);
    CHECK_FALSE(mover.GetOrder().has_value());
    CHECK(mover.GetTile().GetX() == 4); // still on own tile
}

TEST_CASE("Cannot attack a cloaked hostile until contact-revealed",
          "[movement][visibility][reveal]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& cloaked = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "Cloaking_Device"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    player.RebuildVisibility();

    REQUIRE(player.GetVisibleMap().IsVisible(cloaked.GetTile()));
    REQUIRE_FALSE(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    CHECK_FALSE(TryAttack(mover, fixture.At(5, 4), fixture.map, *fixture.ctx));
    CHECK(mover.GetMovesRemaining() == 2 * k_point);

    // Bumping the occupied tile reveals the cloaked unit.
    CHECK_FALSE(TryStep(mover, fixture.At(5, 4), fixture.map, fixture.improvements));
    CHECK(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    REQUIRE(TryAttack(mover, fixture.At(5, 4), fixture.map, *fixture.ctx));
}

TEST_CASE("ZOC block from a cloaked unit contact-reveals it",
          "[movement][visibility][reveal][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& cloaked = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "Cloaking_Device"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    player.RebuildVisibility();

    REQUIRE_FALSE(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    // ZOC -> ZOC toward (4,5) is blocked by the cloaked projector.
    CHECK_FALSE(TryStep(mover, fixture.At(4, 5), fixture.map, fixture.improvements));
    CHECK(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    CHECK(TryAttack(mover, fixture.At(5, 4), fixture.map, *fixture.ctx));
}

TEST_CASE("Move order into cloaked unit reveals via desired-step bump",
          "[movement][visibility][reveal][orders]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& cloaked = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "Cloaking_Device"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    player.RebuildVisibility();
    mover.SetOrder(MoveOrder_t{&fixture.At(5, 4)});

    REQUIRE_FALSE(IsUnitVisibleTo(player, cloaked, *fixture.ctx));

    UnitOrderExecutor executor;
    executor.Execute(mover, fixture.map, fixture.improvements);

    CHECK(IsUnitVisibleTo(player, cloaked, *fixture.ctx));
    CHECK(mover.GetTile().GetX() == 4); // did not enter the hostile tile
}

TEST_CASE("EvaluateStep attributes occupant and ZOC blockers",
          "[movement][evaluate]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& hostile = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});

    SECTION("occupied tile")
    {
        const StepEvaluation_t eval = EvaluateStep(mover, fixture.At(5, 4), fixture.map, fixture.improvements);
        CHECK(eval.outcome == StepOutcome_t::BlockedByOccupant);
        REQUIRE(eval.blockingUnits.size() == 1);
        CHECK(eval.blockingUnits[0] == &hostile);
    }

    SECTION("ZOC to ZOC")
    {
        const StepEvaluation_t eval = EvaluateStep(mover, fixture.At(4, 5), fixture.map, fixture.improvements);
        CHECK(eval.outcome == StepOutcome_t::BlockedByZoc);
        REQUIRE_FALSE(eval.blockingUnits.empty());
        CHECK(std::find(eval.blockingUnits.begin(), eval.blockingUnits.end(), &hostile)
              != eval.blockingUnits.end());
    }

    SECTION("legal leave-ZOC step")
    {
        const StepEvaluation_t eval = EvaluateStep(mover, fixture.At(3, 3), fixture.map, fixture.improvements);
        CHECK(eval.outcome == StepOutcome_t::Legal);
        CHECK(eval.blockingUnits.empty());
    }

    SECTION("not adjacent / no moves")
    {
        CHECK(EvaluateStep(mover, fixture.At(6, 4), fixture.map, fixture.improvements).outcome
              == StepOutcome_t::NotAdjacent);
        mover.SetMovesRemaining(0);
        CHECK(EvaluateStep(mover, fixture.At(4, 5), fixture.map, fixture.improvements).outcome
              == StepOutcome_t::NoMoves);
    }
}

TEST_CASE("Land ignores sea ZOC; sea ignores land ZOC", "[movement][zoc][domain]")
{
    FactionFixture fixture;
    FillLand_(fixture);
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
        CHECK_FALSE(IsTileInHostileZoc(land, land.GetTile(), fixture.map));
        // Adjacent tiles that would be ZOC-to-ZOC if sea affected land remain legal.
        CHECK(CanStep(land, fixture.At(6, 3), fixture.map, fixture.improvements));
        CHECK(CanStep(land, fixture.At(7, 4), fixture.map, fixture.improvements));
    }

    SECTION("land projector does not affect sea")
    {
        fixture.MakeUnit(enemy, 4, 4, {"test_chassis"});
        Unit& sea = fixture.MakeUnit(player, 4, 5, {"test_sea_chassis"});
        CHECK_FALSE(IsTileInHostileZoc(sea, sea.GetTile(), fixture.map));
        CHECK(CanStep(sea, fixture.At(5, 5), fixture.map, fixture.improvements));
    }
}

TEST_CASE("Air ignores ZOC but exerts on land", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    fixture.MakeUnit(enemy, 5, 4, {"test_flight_chassis"});
    Unit& land = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    CHECK(IsTileInHostileZoc(land, land.GetTile(), fixture.map));
    CHECK_FALSE(CanStep(land, fixture.At(4, 5), fixture.map, fixture.improvements)); // ZOC -> ZOC

    fixture.MakeUnit(enemy, 5, 6, {"test_chassis"});
    Unit& flyer = fixture.MakeUnit(player, 4, 6, {"test_flight_chassis"});
    CHECK_FALSE(IsTileInHostileZoc(flyer, flyer.GetTile(), fixture.map));
    CHECK(CanStep(flyer, fixture.At(4, 5), fixture.map, fixture.improvements)); // would be ZOC->ZOC for land
}

TEST_CASE("IgnoreZoneOfControl flag bypasses ZOC", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& probe = fixture.MakeUnit(player, 4, 4, {"test_chassis", "ignore_zoc"});
    CHECK(probe.GetFlag(RuleFlagId_t::IgnoreZoneOfControl));
    CHECK_FALSE(IsTileInHostileZoc(probe, probe.GetTile(), fixture.map));
    CHECK(CanStep(probe, fixture.At(4, 5), fixture.map, fixture.improvements));
}

TEST_CASE("UnitOrderExecutor advances one greedy step without teleporting", "[movement][orders]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(5, 4);
    unit.SetOrder(MoveOrder_t{&rDest});

    const int distBefore = ChebyshevDistance(unit.GetTile(), rDest);

    UnitOrderExecutor executor;
    executor.Execute(unit, fixture.map, fixture.improvements);

    const int distAfter = ChebyshevDistance(unit.GetTile(), rDest);
    CHECK(distAfter == distBefore - 1);
    CHECK(unit.GetMovesRemaining() == k_point);
    REQUIRE(unit.GetOrder().has_value());
    // Still not at destination after one step.
    CHECK(&unit.GetTile() != &rDest);

    executor.Execute(unit, fixture.map, fixture.improvements);
    CHECK(unit.GetMovesRemaining() == 0);
    REQUIRE(unit.GetOrder().has_value()); // destination not reached; waits for next turn
}

TEST_CASE("ProposeNextStep respects ZOC", "[movement][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Enemy blocks eastward transit: mover at (4,4) in ZOC, dest (6,4).
    // Hostile tile (5,4) is not enterable; (5,3)/(5,5) are ZOC->ZOC.
    fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 4, 4, {"test_chassis"});

    CHECK(ProposeNextStep(mover, fixture.At(6, 4), fixture.map, fixture.improvements) == nullptr);
}

TEST_CASE("TryStep spends tile move-cost fragments", "[movement][move-cost]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    REQUIRE(unit.GetMovesRemaining() == 2 * k_point);

    Tile& rocky = fixture.At(5, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    REQUIRE(TryStep(unit, rocky, fixture.map, fixture.improvements));
    CHECK(unit.GetMovesRemaining() == 0); // 2-point rocky drains a 2-move unit

    Unit& roadUnit = fixture.MakeUnit(faction, 4, 5, {"test_chassis"});
    Tile& road = fixture.At(5, 5);
    road.AddImprovement(fixture.improvements.Get("Road"));
    REQUIRE(TryStep(roadUnit, road, fixture.map, fixture.improvements));
    CHECK(roadUnit.GetMovesRemaining() == 2 * k_point - k_point / 3);

    // Rocky with only one move point left is still enterable (SMAC); remaining zeroes.
    Unit& lastPoint = fixture.MakeUnit(faction, 6, 4, {"test_chassis"});
    lastPoint.SetMovesRemaining(k_point);
    Tile& rocky2 = fixture.At(7, 4);
    rocky2.SetRockiness(Rockiness_t::Rocky);
    REQUIRE(TryStep(lastPoint, rocky2, fixture.map, fixture.improvements));
    CHECK(lastPoint.GetMovesRemaining() == 0);
}
