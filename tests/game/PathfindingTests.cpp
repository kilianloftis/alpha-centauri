#include <catch2/catch_test_macros.hpp>
#include "GameFixtures.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <random>

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

// Path cost / terrain tests that are not about fog assume the faction remembers the map.
void ExploreAll_(Faction& rFaction, WorldMap& rMap)
{
    for (const auto& pTile : rMap.GetTiles())
    {
        if (pTile)
        {
            rFaction.GetExploredMap().Mark(*pTile);
        }
    }
}

struct PathHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    std::mt19937 rng;
    UnitOrderExecutor orders;

    explicit PathHarness_(WorldFixture& fixture)
        : moveCosts(fixture.improvements)
        , steps(fixture.map, *fixture.ctx)
        , pathfinder(moveCosts, steps, fixture.map)
        , orders(moveCosts, steps, fixture.map, *fixture.ctx, pathfinder, fixture.morale(), rng)
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
    ExploreAll_(faction, fixture.map);
    const Tile& rDest = fixture.At(5, 4);

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    REQUIRE(path.tiles.size() == 3);
    CHECK(path.tiles.back() == &rDest);
    CHECK(path.totalCostFragments == 3 * k_point);
    CHECK(harness.pathfinder.NextStep(unit, rDest) == path.tiles.front());
    CHECK(ChebyshevDistance(*path.tiles.front(), rDest, fixture.map.GetWidth()) == 2);
}

TEST_CASE("FindPath prefers cheaper road corridor over shorter rocky", "[movement][pathfinding]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    ExploreAll_(faction, fixture.map);
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

TEST_CASE("FindPath land unit detours around known water", "[movement][pathfinding]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    // Water wall is adjacent (vision 1) so it is explored without ExploreAll.
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

TEST_CASE("FindPath routes around visible enemy ZOC", "[movement][pathfinding][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    fixture.MakeUnit(enemy, 4, 4, {"test_chassis"});
    // Scout brings the enemy into faction vision without pinning the mover in ZOC.
    fixture.MakeUnit(player, 4, 5, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 2, 4, {"test_chassis"});
    ExploreAll_(player, fixture.map);
    const Tile& rDest = fixture.At(6, 4);

    REQUIRE(IsUnitVisibleTo(player, *fixture.map.GetUnitsOnTile(fixture.At(4, 4)).front(),
                            *fixture.ctx));

    const Path_t path = harness.pathfinder.FindPath(mover, rDest);
    REQUIRE(path.bReachable);
    CHECK(path.tiles.back() == &rDest);
    // Never steps onto the enemy tile.
    for (const Tile* pTile : path.tiles)
    {
        CHECK_FALSE(pTile == &fixture.At(4, 4));
    }
}

TEST_CASE("FindPath unreachable when visible ZOC walls off destination",
          "[movement][pathfinding][zoc]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Mover pinned at the north map edge in ZOC (Y does not wrap); dest behind the enemy.
    fixture.MakeUnit(enemy, 4, 1, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 4, 0, {"test_chassis"});
    const Tile& rDest = fixture.At(4, 2);

    REQUIRE(IsUnitVisibleTo(player, *fixture.map.GetUnitsOnTile(fixture.At(4, 1)).front(),
                            *fixture.ctx));

    const Path_t path = harness.pathfinder.FindPath(mover, rDest);
    CHECK_FALSE(path.bReachable);
    CHECK(path.tiles.empty());
    CHECK(harness.pathfinder.NextStep(mover, rDest) == nullptr);

    // Contact-reveal seam still offers a desired bump toward the destination.
    CHECK(harness.pathfinder.DesiredContactStep(mover, rDest) != nullptr);
}

TEST_CASE("FindPath at destination and unreachable known terrain", "[movement][pathfinding]")
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
    ExploreAll_(faction, fixture.map);
    // Known water is a domain dead-end: planner must reject without a land-flood search.
    CHECK_FALSE(harness.steps.CanPlanEnterTerrain(unit, fixture.At(6, 4)));
    const Path_t water = harness.pathfinder.FindPath(unit, fixture.At(6, 4));
    CHECK_FALSE(water.bReachable);
    CHECK(water.tiles.empty());
    CHECK(harness.pathfinder.NextStep(unit, fixture.At(6, 4)) == nullptr);
}

TEST_CASE("UnitOrderExecutor advances along pathfinder until moves exhausted", "[movement][pathfinding][orders]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    ExploreAll_(faction, fixture.map);
    const Tile& rDest = fixture.At(5, 4);
    unit.SetOrder(MoveOrder_t{&rDest});
    REQUIRE(unit.GetMoveFragmentsRemaining() == 2 * k_point);

    harness.orders.Execute(unit);

    // 3 tiles away, 2 move points → advances 2 steps.
    CHECK(ChebyshevDistance(unit.GetTile(), rDest, fixture.map.GetWidth()) == 1);
    CHECK(unit.GetMoveFragmentsRemaining() == 0);
    REQUIRE(unit.GetOrder().has_value());
}

TEST_CASE("FindPath prefers friendly fungus over empty fungus", "[movement][pathfinding][fungus]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    ExploreAll_(faction, fixture.map);
    const Tile& rDest = fixture.At(5, 4);

    // Water walls force every route through column 3–4 fungus: empty on y=4, friendly on y=3.
    for (int y = 0; y < fixture.map.GetHeight(); ++y)
    {
        if (y != 3 && y != 4)
        {
            MakeWater_(fixture.At(3, y));
            MakeWater_(fixture.At(4, y));
        }
    }
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
    // Friendly: one allotment each (M=2, cost 1) + dest; empty would be two allotments each.
    CHECK(path.totalCostFragments == (2 + 2 + 1) * k_point);
}

TEST_CASE("FindPath prefers clear detour over cheaper-looking fungus",
          "[movement][pathfinding][fungus]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    REQUIRE(unit.GetMovementPoints() == 2);
    ExploreAll_(faction, fixture.map);
    const Tile& rDest = fixture.At(4, 4);

    // Direct: (3,4) fungus then dest. Planned = 4+1 = 5 (M=2 whole-turn valuation).
    fixture.At(3, 4).SetHasFungus(true);
    // Block short clear diagonals around the fungus; leave (3,2) as a land bridge so the
    // clear route is exactly four steps: (2,4)->(2,3)->(3,2)->(4,3)->(4,4).
    MakeWater_(fixture.At(3, 3));
    MakeWater_(fixture.At(3, 5));
    MakeWater_(fixture.At(3, 1));
    MakeWater_(fixture.At(3, 0));
    for (int y = 6; y < fixture.map.GetHeight(); ++y)
    {
        MakeWater_(fixture.At(3, y));
    }

    const auto costs = harness.moveCosts.ForUnit(unit, fixture.map);
    CHECK(costs.PlannedCostFragments(fixture.At(3, 4)) == 4 * k_point);

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    for (const Tile* pTile : path.tiles)
    {
        CHECK_FALSE(pTile == &fixture.At(3, 4));
    }
    CHECK(path.totalCostFragments == 4 * k_point);
}

TEST_CASE("FindPath ignores cloaked hostiles until contact-revealed",
          "[movement][pathfinding][visibility]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& cloaked = fixture.MakeUnit(enemy, 3, 4, {"test_chassis", "Cloaking_Device"});
    Unit& mover = fixture.MakeUnit(player, 2, 4, {"test_chassis"});
    ExploreAll_(player, fixture.map);
    const Tile& rDest = fixture.At(5, 4);

    REQUIRE(player.GetVisibleMap().IsVisible(cloaked.GetTile()));
    REQUIRE_FALSE(IsUnitVisibleTo(player, cloaked, *fixture.ctx));

    // Planner ignores cloak/ZOC — Chebyshev-cheap path stays reachable (equal-cost
    // diagonals may or may not step on the cloaked tile itself).
    const Path_t path = harness.pathfinder.FindPath(mover, rDest);
    REQUIRE(path.bReachable);
    CHECK(path.tiles.size() == 3);
    CHECK(path.totalCostFragments == 3 * k_point);

    // Objective step still blocks; contact reveal happens on TryStep.
    CHECK_FALSE(harness.steps.CanStep(mover, mover.GetTile(), cloaked.GetTile()));
    CHECK(harness.steps.CanPlanStep(mover, mover.GetTile(), cloaked.GetTile()));
}

TEST_CASE("FindPath ignores fogged hostiles outside vision",
          "[movement][pathfinding][visibility][fog]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Distance 2: fogged to vision-1 mover. Terrain is still known if explored.
    Unit& hostile = fixture.MakeUnit(enemy, 4, 4, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 2, 4, {"test_chassis"});
    ExploreAll_(player, fixture.map);
    const Tile& rDest = fixture.At(5, 4);

    REQUIRE_FALSE(IsUnitVisibleTo(player, hostile, *fixture.ctx));
    CHECK(harness.steps.CanPlanStep(mover, fixture.At(3, 4), hostile.GetTile()));

    // Unknown occupant does not force a detour.
    const Path_t path = harness.pathfinder.FindPath(mover, rDest);
    REQUIRE(path.bReachable);
    CHECK(path.tiles.size() == 3);
    CHECK(path.totalCostFragments == 3 * k_point);
}

TEST_CASE("FindPath treats shrouded water as passable with default cost",
          "[movement][pathfinding][visibility][shroud]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(5, 4);

    // Water beyond vision-1 is shrouded — any 3-step path must cross column 4.
    MakeWater_(fixture.At(4, 3));
    MakeWater_(fixture.At(4, 4));
    MakeWater_(fixture.At(4, 5));
    REQUIRE_FALSE(faction.GetExploredMap().IsExplored(fixture.At(4, 4)));

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    REQUIRE(path.tiles.size() == 3);
    CHECK(path.totalCostFragments == 3 * k_point);
    bool bThroughShroudedWater = false;
    for (const Tile* pTile : path.tiles)
    {
        if (pTile == &fixture.At(4, 3) || pTile == &fixture.At(4, 4)
            || pTile == &fixture.At(4, 5))
        {
            bThroughShroudedWater = true;
        }
    }
    CHECK(bThroughShroudedWater);

    // Once explored, the same water wall forces a longer land detour.
    ExploreAll_(faction, fixture.map);
    const Path_t known = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(known.bReachable);
    CHECK(known.tiles.size() > 3);
    for (const Tile* pTile : known.tiles)
    {
        CHECK(pTile->IsLand());
    }
}

TEST_CASE("FindPath ignores shrouded rockiness for cost",
          "[movement][pathfinding][visibility][shroud]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 2, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(5, 4);

    // Shrouded rocky corridor covering every 3-step route through column 4.
    fixture.At(4, 3).SetRockiness(Rockiness_t::Rocky);
    fixture.At(4, 4).SetRockiness(Rockiness_t::Rocky);
    fixture.At(4, 5).SetRockiness(Rockiness_t::Rocky);
    REQUIRE_FALSE(faction.GetExploredMap().IsExplored(fixture.At(4, 4)));

    const Path_t shrouded = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(shrouded.bReachable);
    CHECK(shrouded.totalCostFragments == 3 * k_point);

    ExploreAll_(faction, fixture.map);
    const Path_t known = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(known.bReachable);
    // Short path pays rocky (+1) on the column-4 step.
    CHECK(known.totalCostFragments == 4 * k_point);
}

TEST_CASE("FindPath takes the one-step wrap across the map seam", "[movement][pathfinding][wrap]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    ExploreAll_(faction, fixture.map);

    const int width = fixture.map.GetWidth();
    Unit& unit = fixture.MakeUnit(faction, 0, 4, {"test_chassis"});
    const Tile& rDest = fixture.At(width - 1, 4);

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    REQUIRE(path.tiles.size() == 1);
    CHECK(path.tiles.front() == &rDest);
    CHECK(path.totalCostFragments == k_point);
    CHECK(harness.pathfinder.NextStep(unit, rDest) == &rDest);
}
