// Worker-assignment state model: the world-scoped WorkedTileIndex is the single owner of
// the one-worker-per-tile rule (across bases and factions), assignments are RAII
// WorkedTileClaims held by Pops, and the user-assigned flag lives on the claim so it can
// never outlive the assignment.

#include "GameFixtures.h"

#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/map/WorkedTileIndex.h"
#include "game/population/pop-types/Pop.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace ac;

namespace
{

Pop& FirstPop(BaseManager& rBase)
{
    for (Pop& rPop : rBase.GetPopulation().Pops())
    {
        return rPop;
    }
    throw std::logic_error("base has no pops");
}

Pop& LastPop(BaseManager& rBase)
{
    Pop* pLast = nullptr;
    for (Pop& rPop : rBase.GetPopulation().Pops())
    {
        pLast = &rPop;
    }
    if (!pLast)
    {
        throw std::logic_error("base has no pops");
    }
    return *pLast;
}

} // namespace

TEST_CASE("A tile worked by one base cannot be worked by another base", "[worker][index]")
{
    actest::BaseFixture fixture;
    // Workable areas of bases at (2,2) and (4,4) overlap; (3,3) is workable by both.
    BaseManager& baseA = fixture.MakeBase(2, 2);
    BaseManager& baseB = fixture.MakeBase(4, 4);
    Tile& shared = fixture.At(3, 3);

    baseA.GetWorkerAssignments().UnassignAll();
    baseB.GetWorkerAssignments().UnassignAll();
    REQUIRE_FALSE(fixture.map.GetWorkedTiles().IsWorked(shared));

    REQUIRE(baseA.GetWorkerAssignments().AssignWorker(FirstPop(baseA), &shared));

    // Both bases see the tile as taken — including the base that does not own the worker.
    CHECK(baseA.GetWorkerAssignments().IsTileAssigned(&shared));
    CHECK(baseB.GetWorkerAssignments().IsTileAssigned(&shared));
    CHECK_FALSE(baseB.GetWorkerAssignments().AssignWorker(FirstPop(baseB), &shared));

    // Releasing the worker frees the tile for the other base.
    baseA.GetWorkerAssignments().UnassignWorker(FirstPop(baseA));
    CHECK_FALSE(fixture.map.GetWorkedTiles().IsWorked(shared));
    CHECK(baseB.GetWorkerAssignments().AssignWorker(FirstPop(baseB), &shared));
}

TEST_CASE("A base can never work another base's own tile", "[worker][index]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);
    // Free A's auto-assigned workers so B's founding tile cannot already be worked.
    baseA.GetWorkerAssignments().UnassignAll();
    BaseManager& baseB = fixture.MakeBase(3, 3);
    baseB.GetWorkerAssignments().UnassignAll();

    // Each base holds its own tile's claim for its whole life.
    CHECK(fixture.map.GetWorkedTiles().IsWorked(fixture.At(2, 2)));
    CHECK(fixture.map.GetWorkedTiles().IsWorked(fixture.At(3, 3)));

    // A's center is inside B's workable area, but B can neither assign onto it nor see it
    // as free — and vice versa.
    CHECK_FALSE(baseB.GetWorkerAssignments().AssignWorker(FirstPop(baseB), &fixture.At(2, 2)));
    CHECK(baseB.GetWorkerAssignments().IsTileAssigned(&fixture.At(2, 2)));
    CHECK_FALSE(baseA.GetWorkerAssignments().AssignWorker(FirstPop(baseA), &fixture.At(3, 3)));
}

TEST_CASE("A destroyed base releases its own tile", "[worker][index]")
{
    actest::BaseFixture fixture;
    fixture.MakeBase(4, 4);
    REQUIRE(fixture.map.GetWorkedTiles().IsWorked(fixture.At(4, 4)));

    fixture.bases.pop_back();

    CHECK_FALSE(fixture.map.GetWorkedTiles().IsWorked(fixture.At(4, 4)));
}

TEST_CASE("Founding a base displaces the tile's worker onto its base's best free tile",
          "[worker][index]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);
    baseA.GetWorkerAssignments().UnassignAll();

    // The only tile in A's radius with a non-zero yield — where the displaced worker
    // must end up (Wet grants +2 nutrients; every other tile is barren).
    Tile& bestTile = fixture.At(1, 2);
    bestTile.SetBaseMoisture(Moisture::Wet);
    bestTile.SetMoisture(Moisture::Wet);

    Pop& rWorker = FirstPop(baseA);
    Tile& target = fixture.At(4, 3);
    REQUIRE(baseA.GetWorkerAssignments().UserAssignWorker(rWorker, &target));

    fixture.MakeBase(4, 3);

    // The founding base owns the tile; the displaced worker lost the assignment and its
    // user flag, and A's auto-reassignment moved it to the best free tile in A's radius.
    CHECK(fixture.map.GetWorkedTiles().IsWorked(target));
    CHECK_FALSE(rWorker.IsUserAssigned());
    CHECK(rWorker.GetTile() == &bestTile);
}

TEST_CASE("Founding a base on another base's own tile throws", "[worker][index]")
{
    actest::BaseFixture fixture;
    fixture.MakeBase(2, 2);

    CHECK_THROWS_AS(fixture.MakeBase(2, 2), std::runtime_error);
}

TEST_CASE("Destroying a pop releases its worked tile", "[worker][index]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(2, 2);
    Tile& tile = fixture.At(3, 3);

    base.GetWorkerAssignments().UnassignAll();
    // RemovePop destroys the most recently added pop, so assign that one.
    REQUIRE(base.GetWorkerAssignments().AssignWorker(LastPop(base), &tile));
    REQUIRE(fixture.map.GetWorkedTiles().IsWorked(tile));

    base.GetPopulation().RemovePop();

    CHECK_FALSE(fixture.map.GetWorkedTiles().IsWorked(tile));
}

TEST_CASE("Converting a worker to a non-worker type releases its tile", "[worker][index]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(2, 2);
    Tile& tile = fixture.At(3, 3);

    base.GetWorkerAssignments().UnassignAll();
    Pop& rPop = FirstPop(base);
    REQUIRE(base.GetWorkerAssignments().AssignWorker(rPop, &tile));

    // Convert directly through the population subsystem, bypassing BaseManager::ConvertPop's
    // explicit unassign — the claim must still be released.
    base.GetPopulation().ConvertTo(rPop, "Doctor");

    CHECK(rPop.GetTile() == nullptr);
    CHECK_FALSE(fixture.map.GetWorkedTiles().IsWorked(tile));
}

TEST_CASE("User-assigned flag lives and dies with the tile claim", "[worker]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(2, 2);
    Tile& tile = fixture.At(3, 3);

    base.GetWorkerAssignments().UnassignAll();
    Pop& rPop = FirstPop(base);
    REQUIRE(base.GetWorkerAssignments().UserAssignWorker(rPop, &tile));
    CHECK(rPop.IsUserAssigned());

    // UnassignAll only clears auto-assigned pops.
    base.GetWorkerAssignments().UnassignAll();
    CHECK(rPop.GetTile() == &tile);

    base.GetWorkerAssignments().UnassignWorker(rPop);
    CHECK(rPop.GetTile() == nullptr);
    CHECK_FALSE(rPop.IsUserAssigned());
}

TEST_CASE("Assignment is refused outside the workable set and for non-workers", "[worker]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(2, 2);

    base.GetWorkerAssignments().UnassignAll();
    Pop& rPop = FirstPop(base);
    // (8,8) is far outside the 5x5 workable area of a base at (2,2).
    CHECK_FALSE(base.GetWorkerAssignments().AssignWorker(rPop, fixture.map.GetTile(8, 8)));

    // A non-empty claim handed to a pop that cannot work tiles is a hard error.
    Pop doctor(*fixture.popTypes().Find("Doctor"));
    WorkedTileClaim claim = fixture.map.GetWorkedTiles().TryClaim(fixture.At(1, 1), false);
    REQUIRE(claim.GetTile() != nullptr);
    CHECK_THROWS_AS(doctor.SetTileClaim(std::move(claim)), std::logic_error);
}

TEST_CASE("WorkedTileIndex bumps its revision on claim and release", "[worker][index]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(2, 2);
    WorkedTileIndex& rIndex = fixture.map.GetWorkedTiles();

    base.GetWorkerAssignments().UnassignAll();
    const uint64_t before = rIndex.GetRevision();

    Pop& rPop = FirstPop(base);
    REQUIRE(base.GetWorkerAssignments().AssignWorker(rPop, &fixture.At(3, 3)));
    const uint64_t afterClaim = rIndex.GetRevision();
    CHECK(afterClaim > before);

    // A failed claim (tile already worked) must not invalidate caches.
    CHECK_FALSE(base.GetWorkerAssignments().AssignWorker(LastPop(base), &fixture.At(3, 3)));
    CHECK(rIndex.GetRevision() == afterClaim);

    base.GetWorkerAssignments().UnassignWorker(rPop);
    CHECK(rIndex.GetRevision() > afterClaim);
}
