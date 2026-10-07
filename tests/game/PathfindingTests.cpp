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
        , orders(moveCosts, steps, fixture.map, *fixture.ctx, pathfinder, fixture.morale(),
                 *fixture.dataContext.terrainOperationRegistry, rng)
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
    Unit& unit = fixture.MakeUnit(faction, 6, 6, {"test_chassis"});
    ExploreAll_(faction, fixture.map);
    const Tile& rDest = fixture.At(9, 9);

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
    Unit& unit = fixture.MakeUnit(faction, 6, 6, {"test_chassis"});
    ExploreAll_(faction, fixture.map);
    const Tile& rDest = fixture.At(9, 9);

    // Direct east corridor is rocky (expensive).
    fixture.At(7, 7).SetRockiness(Rockiness_t::Rocky);
    fixture.At(8, 8).SetRockiness(Rockiness_t::Rocky);

    // Northern road detour is longer in steps but cheaper in fragments.
    for (int x = 2; x <= 5; ++x)
    {
        fixture.At(x + 5, x + 3).AddImprovement(fixture.improvements.Get("Road"));
    }
    fixture.At(9, 9).AddImprovement(fixture.improvements.Get("Road"));

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    REQUIRE_FALSE(path.tiles.empty());

    // Must not walk the direct rocky corridor (3,4) / (4,4).
    for (const Tile* pTile : path.tiles)
    {
        CHECK_FALSE(pTile == &fixture.At(7, 7));
        CHECK_FALSE(pTile == &fixture.At(8, 8));
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
    Unit& unit = fixture.MakeUnit(faction, 6, 6, {"test_chassis"});
    // Water wall is adjacent (vision 1) so it is explored without ExploreAll.
    const Tile& rDest = fixture.At(9, 9);

    // Water wall between start and dest (orthogonal + diagonal cover).
    MakeWater_(fixture.At(8, 6));
    MakeWater_(fixture.At(7, 7));
    MakeWater_(fixture.At(6, 8));

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

    fixture.MakeUnit(enemy, 8, 8, {"test_chassis"});
    // Scout brings the enemy into faction vision without pinning the mover in ZOC.
    fixture.MakeUnit(player, 7, 9, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 6, 6, {"test_chassis"});
    ExploreAll_(player, fixture.map);
    const Tile& rDest = fixture.At(10, 10);

    REQUIRE(IsUnitVisibleTo(player, *fixture.map.GetUnitsOnTile(fixture.At(8, 8)).front(),
                            *fixture.ctx));

    const Path_t path = harness.pathfinder.FindPath(mover, rDest);
    REQUIRE(path.bReachable);
    CHECK(path.tiles.back() == &rDest);
    // Never steps onto the enemy tile.
    for (const Tile* pTile : path.tiles)
    {
        CHECK_FALSE(pTile == &fixture.At(8, 8));
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

    // Mover pinned at the north map edge in ZOC (Y does not wrap): every neighbor that exists is
    // occupied by an enemy or covered by hostile ZOC; dest is behind the enemy.
    fixture.MakeUnit(enemy, 13, 1, {"test_chassis"});
    fixture.MakeUnit(enemy, 10, 0, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 12, 0, {"test_chassis"});
    const Tile& rDest = fixture.At(14, 2);

    REQUIRE(IsUnitVisibleTo(player, *fixture.map.GetUnitsOnTile(fixture.At(13, 1)).front(),
                            *fixture.ctx));
    REQUIRE(IsUnitVisibleTo(player, *fixture.map.GetUnitsOnTile(fixture.At(10, 0)).front(),
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
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

    const Path_t atDest = harness.pathfinder.FindPath(unit, unit.GetTile());
    CHECK(atDest.bReachable);
    CHECK(atDest.tiles.empty());
    CHECK(atDest.totalCostFragments == 0);
    CHECK(harness.pathfinder.NextStep(unit, unit.GetTile()) == nullptr);

    MakeWater_(fixture.At(10, 10));
    ExploreAll_(faction, fixture.map);
    // Known water is a domain dead-end: planner must reject without a land-flood search.
    CHECK_FALSE(harness.steps.CanPlanEnterTerrain(unit, fixture.At(10, 10)));
    const Path_t water = harness.pathfinder.FindPath(unit, fixture.At(10, 10));
    CHECK_FALSE(water.bReachable);
    CHECK(water.tiles.empty());
    CHECK(harness.pathfinder.NextStep(unit, fixture.At(10, 10)) == nullptr);
}

TEST_CASE("UnitOrderExecutor advances along pathfinder until moves exhausted", "[movement][pathfinding][orders]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    PathHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 6, 6, {"test_chassis"});
    ExploreAll_(faction, fixture.map);
    const Tile& rDest = fixture.At(9, 9);
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
    Unit& unit = fixture.MakeUnit(faction, 6, 6, {"test_chassis"});
    ExploreAll_(faction, fixture.map);
    const Tile& rDest = fixture.At(9, 9);

    // Water walls force every route through the fungus gap: empty on one lattice row, friendly
    // on the next. The walls are the two lattice columns between start and dest.
    for (const auto& pTile : fixture.map.GetTiles())
    {
        const int column = (pTile->GetX() + pTile->GetY() - 8) / 2;
        const int row = (pTile->GetY() - pTile->GetX() + 8) / 2;
        if ((column == 3 || column == 4) && row != 3 && row != 4)
        {
            MakeWater_(*pTile);
        }
    }
    fixture.At(7, 7).AddTerrainFeature(fixture.improvements.Get("Fungus"));
    fixture.At(8, 8).AddTerrainFeature(fixture.improvements.Get("Fungus"));
    fixture.At(8, 6).AddTerrainFeature(fixture.improvements.Get("Fungus"));
    fixture.At(9, 7).AddTerrainFeature(fixture.improvements.Get("Fungus"));
    fixture.MakeUnit(faction, 8, 6, {"test_chassis"});
    fixture.MakeUnit(faction, 9, 7, {"test_chassis"});

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    for (const Tile* pTile : path.tiles)
    {
        CHECK_FALSE(pTile == &fixture.At(7, 7));
        CHECK_FALSE(pTile == &fixture.At(8, 8));
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
    Unit& unit = fixture.MakeUnit(faction, 6, 6, {"test_chassis"});
    REQUIRE(unit.GetMovementPoints() == 2);
    ExploreAll_(faction, fixture.map);
    const Tile& rDest = fixture.At(8, 8);

    // Direct: one fungus tile then dest. Planned = 4+1 = 5 (M=2 whole-turn valuation).
    fixture.At(7, 7).AddTerrainFeature(fixture.improvements.Get("Fungus"));
    // Wall off the lattice column beside the fungus; leave one land bridge so the clear route
    // is exactly four steps.
    for (const auto& pTile : fixture.map.GetTiles())
    {
        const int column = (pTile->GetX() + pTile->GetY() - 8) / 2;
        const int row = (pTile->GetY() - pTile->GetX() + 8) / 2;
        if (column == 3 && row != 2 && row != 4)
        {
            MakeWater_(*pTile);
        }
    }

    const auto costs = harness.moveCosts.ForUnit(unit, fixture.map);
    CHECK(costs.PlannedCostFragments(fixture.At(7, 7)) == 4 * k_point);

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    for (const Tile* pTile : path.tiles)
    {
        CHECK_FALSE(pTile == &fixture.At(7, 7));
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

    Unit& cloaked = fixture.MakeUnit(enemy, 7, 7, {"test_chassis", "Cloaking_Device"});
    Unit& mover = fixture.MakeUnit(player, 6, 6, {"test_chassis"});
    ExploreAll_(player, fixture.map);
    const Tile& rDest = fixture.At(9, 9);

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
    Unit& hostile = fixture.MakeUnit(enemy, 8, 8, {"test_chassis"});
    Unit& mover = fixture.MakeUnit(player, 6, 6, {"test_chassis"});
    ExploreAll_(player, fixture.map);
    const Tile& rDest = fixture.At(9, 9);

    REQUIRE_FALSE(IsUnitVisibleTo(player, hostile, *fixture.ctx));
    CHECK(harness.steps.CanPlanStep(mover, fixture.At(7, 7), hostile.GetTile()));

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
    Unit& unit = fixture.MakeUnit(faction, 6, 6, {"test_chassis"});
    const Tile& rDest = fixture.At(9, 9);

    // Water beyond vision-1 is shrouded — any 3-step path must cross column 4.
    MakeWater_(fixture.At(9, 7));
    MakeWater_(fixture.At(8, 8));
    MakeWater_(fixture.At(7, 9));
    REQUIRE_FALSE(faction.GetExploredMap().IsExplored(fixture.At(8, 8)));

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    REQUIRE(path.tiles.size() == 3);
    CHECK(path.totalCostFragments == 3 * k_point);
    bool bThroughShroudedWater = false;
    for (const Tile* pTile : path.tiles)
    {
        if (pTile == &fixture.At(9, 7) || pTile == &fixture.At(8, 8)
            || pTile == &fixture.At(7, 9))
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
    Unit& unit = fixture.MakeUnit(faction, 6, 6, {"test_chassis"});
    const Tile& rDest = fixture.At(9, 9);

    // Shrouded rocky corridor covering every 3-step route through column 4.
    fixture.At(9, 7).SetRockiness(Rockiness_t::Rocky);
    fixture.At(8, 8).SetRockiness(Rockiness_t::Rocky);
    fixture.At(7, 9).SetRockiness(Rockiness_t::Rocky);
    REQUIRE_FALSE(faction.GetExploredMap().IsExplored(fixture.At(8, 8)));

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
    const Tile& rDest = fixture.At(width - 2, 4);

    const Path_t path = harness.pathfinder.FindPath(unit, rDest);
    REQUIRE(path.bReachable);
    REQUIRE(path.tiles.size() == 1);
    CHECK(path.tiles.front() == &rDest);
    CHECK(path.totalCostFragments == k_point);
    CHECK(harness.pathfinder.NextStep(unit, rDest) == &rDest);
}
