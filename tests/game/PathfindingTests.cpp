#include <catch2/catch_test_macros.hpp>
#include "GameFixtures.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"

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

struct PathHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    UnitOrderExecutor orders;

    explicit PathHarness_(WorldFixture& fixture)
        : moveCosts(fixture.improvements)
        , steps(fixture.improvements, fixture.map, *fixture.ctx)
        , pathfinder(moveCosts, steps, fixture.map)
        , orders(moveCosts, steps, fixture.map, *fixture.ctx, pathfinder)
    {
    }
};

} // namespace

TEST_CASE("FindPath open land reaches destination with Chebyshev cost", "[movement][pathfinding]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(5, 4);

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    REQUIRE(path.tiles.size() == 3);
    CHECK(path.tiles.back() == &rDest);
    CHECK(path.totalCostFragments == 3 * k_point);
    CHECK(harness.pathfinder.NextStep(unit, rDest) == path.tiles.front());
    CHECK(ChebyshevDistance(*path.tiles.front(), rDest) == 2);
}

TEST_CASE("FindPath prefers cheaper road corridor over shorter rocky", "[movement][pathfinding]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(5, 4);

    // Direct east corridor is rocky (expensive).
    fixture.At(3, 4).SetRockiness(Rockiness_t::Rocky);
    fixture.At(4, 4).SetRockiness(Rockiness_t::Rocky);

    // Northern road detour is longer in steps but cheaper in fragments.
    for (int x = 2; x <= 5; ++x)
    {
        fixture.At(x, 3).AddImprovement(fixture.improvements.Get("Road"));
    }
    fixture.At(5, 4).AddImprovement(fixture.improvements.Get("Road"));

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    REQUIRE_FALSE(path.tiles.empty());

    // Must not walk the direct rocky corridor (3,4) / (4,4).
    for (const Tile* pTile : path.tiles)
    {
        CHECK_FALSE(pTile == &fixture.At(3, 4));
        CHECK_FALSE(pTile == &fixture.At(4, 4));
    }

    // Direct rocky corridor would cost at least 2+2+1 points; road must beat that.
    CHECK(path.totalCostFragments < (2 + 2 + 1) * k_point);
}

TEST_CASE("FindPath land unit detours around water", "[movement][pathfinding]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(5, 4);

    // Water wall between start and dest (orthogonal + diagonal cover).
    MakeWater_(fixture.At(3, 3));
    MakeWater_(fixture.At(3, 4));
    MakeWater_(fixture.At(3, 5));

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    REQUIRE(path.tiles.size() > 3); // longer than open-land Chebyshev
    CHECK(path.tiles.back() == &rDest);
    for (const Tile* pTile : path.tiles)
    {
        CHECK(pTile->IsLand());
    }
}

TEST_CASE("FindPath routes around enemy ZOC", "[movement][pathfinding][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    fixture.MakeUnit(enemy, 4, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(6, 4);

    const Path_t path = harness.pathfinder.FindPath(mover, rDest);
    REQUIRE(path.bReachable);
    CHECK(path.tiles.back() == &rDest);
    // Never steps onto the enemy tile.
    for (const Tile* pTile : path.tiles)
    {
        CHECK_FALSE(pTile == &fixture.At(4, 4));
    }
}

TEST_CASE("FindPath unreachable when ZOC walls off destination", "[movement][pathfinding][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Mover pinned at map edge in ZOC with no legal exit; dest behind the enemy.
    fixture.MakeUnit(enemy, 1, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 0, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(2, 4);

    const Path_t path = harness.pathfinder.FindPath(mover, rDest);
    CHECK_FALSE(path.bReachable);
    CHECK(path.tiles.empty());
    CHECK(harness.pathfinder.NextStep(mover, rDest) == nullptr);

    // Contact-reveal seam still offers a desired bump toward the destination.
    CHECK(harness.pathfinder.DesiredContactStep(mover, rDest) != nullptr);
}

TEST_CASE("FindPath at destination and unreachable terrain", "[movement][pathfinding]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    const Path_t atDest = harness.pathfinder.FindPath(unit, unit.GetTile());
    CHECK(atDest.bReachable);
    CHECK(atDest.tiles.empty());
    CHECK(atDest.totalCostFragments == 0);
    CHECK(harness.pathfinder.NextStep(unit, unit.GetTile()) == nullptr);

    MakeWater_(fixture.At(6, 4));
    const Path_t water = harness.pathfinder.FindPath(unit, fixture.At(6, 4));
    CHECK_FALSE(water.bReachable);
    CHECK(harness.pathfinder.NextStep(unit, fixture.At(6, 4)) == nullptr);
}

TEST_CASE("UnitOrderExecutor advances along pathfinder NextStep", "[movement][pathfinding][orders]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(5, 4);
    unit.SetOrder(MoveOrder_t{&rDest});

    const Tile* pExpected = harness.pathfinder.NextStep(unit, rDest);
    REQUIRE(pExpected);

    harness.orders.Execute(unit);
    CHECK(&unit.GetTile() == pExpected);
    CHECK(ChebyshevDistance(unit.GetTile(), rDest) == 2);
}

TEST_CASE("FindPath prefers friendly fungus over empty fungus", "[movement][pathfinding][fungus]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(5, 4);

    // Direct east corridor is empty fungus (3 each). Northern corridor has a friend.
    fixture.At(3, 4).SetHasFungus(true);
    fixture.At(4, 4).SetHasFungus(true);

    fixture.At(3, 3).SetHasFungus(true);
    fixture.At(4, 3).SetHasFungus(true);
    fixture.MakeUnit(faction, 3, 3, {"test_chassis"});
    fixture.MakeUnit(faction, 4, 3, {"test_chassis"});

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    for (const Tile* pTile : path.tiles)
    {
        CHECK_FALSE(pTile == &fixture.At(3, 4));
        CHECK_FALSE(pTile == &fixture.At(4, 4));
    }
    // Friendly fungus steps cost 1 each (plus any flat remainder to dest).
    CHECK(path.totalCostFragments < (3 + 3 + 1) * k_point);
}
