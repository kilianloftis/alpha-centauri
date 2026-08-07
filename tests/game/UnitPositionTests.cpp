// Unit-position state model: UnitPositionIndex is the single owner — units register for
// their lifetime (RAII), movement goes through MoveUnit (which keeps the unit's tile
// pointer and the occupancy lists in sync). Stacking legality lives in MovementRules.

#include "GameFixtures.h"

#include "game/faction/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/units/MovementRules.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

// (The SingleUnitPerTileGuard that used to live here is gone: the stacking rule is per-world
// state on UnitPositionIndex now, so a test sets it on its own fixture's index and cannot leak
// it into the next case.)

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

} // namespace

TEST_CASE("Unit creation and destruction maintain the position index", "[unit][index]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    REQUIRE(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).size() == 1);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).front() == &unit);

    // No manual index bookkeeping: the unit unregisters itself when destroyed.
    faction.GetUnitManager().DestroyUnit(unit);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).empty());
}

TEST_CASE("DestroyUnit emits OnUnitDestroyed before the unit is removed", "[unit][signal]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    Unit* pDestroyed = nullptr;
    faction.GetUnitManager().OnUnitDestroyed.Connect([&pDestroyed](Unit& rUnit) {
        pDestroyed = &rUnit;
    });

    faction.GetUnitManager().DestroyUnit(unit);

    CHECK(pDestroyed == &unit);
}

TEST_CASE("Deferred destruction keeps unit iteration safe", "[unit][lifetime]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    UnitManager& rUnits = faction.GetUnitManager();
    fixture.MakeUnit(faction, 3, 4, {"test_chassis"});
    fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    fixture.MakeUnit(faction, 5, 4, {"test_chassis"});

    std::vector<UnitId_t> visitedIds;
    {
        const auto destructionScope = rUnits.DeferDestruction();
        for (Unit& rUnit : rUnits.Units())
        {
            const Tile& rTile = rUnit.GetTile();
            visitedIds.push_back(rUnit.GetUnitId());

            rUnits.DestroyUnit(rUnit);

            // Logical removal is immediate, while the deferred object remains safe to
            // inspect until the traversal reaches its scope boundary.
            CHECK(fixture.map.GetUnitsOnTile(rTile).empty());
            CHECK(rUnits.IsPendingDestruction(rUnit));
            CHECK(rUnit.GetUnitId() == visitedIds.back());

            // Queries must tolerate the null slots the deferral leaves in the unit list
            // and never offer a destroyed unit.
            CHECK(rUnits.GetNextAvailableUnit() != &rUnit);
        }

        CHECK(rUnits.Units().empty());
        CHECK(rUnits.GetNextAvailableUnit() == nullptr);
        CHECK_FALSE(rUnits.HasUnitsRequiringOrders());
    }

    CHECK(visitedIds.size() == 3);
    CHECK(rUnits.Units().empty());
}

TEST_CASE("MoveUnit emits OnUnitMoved after updating occupancy", "[unit][signal]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    Unit* pMoved = nullptr;
    const Tile* pTileAtEmit = nullptr;
    fixture.map.GetUnitPositions().OnUnitMoved.Connect([&](Unit& rUnit) {
        pMoved = &rUnit;
        pTileAtEmit = &rUnit.GetTile();
    });

    fixture.map.GetUnitPositions().MoveUnit(unit, fixture.At(5, 4));

    CHECK(pMoved == &unit);
    CHECK(pTileAtEmit == &fixture.At(5, 4));

    // Same-tile move is a no-op and must not re-emit.
    pMoved = nullptr;
    fixture.map.GetUnitPositions().MoveUnit(unit, fixture.At(5, 4));
    CHECK(pMoved == nullptr);
}

TEST_CASE("Moving a unit keeps its tile pointer and the index in sync", "[unit][index]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    fixture.map.GetUnitPositions().MoveUnit(unit, fixture.At(5, 4));

    CHECK(&unit.GetTile() == &fixture.At(5, 4));
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).empty());
    REQUIRE(fixture.map.GetUnitsOnTile(fixture.At(5, 4)).size() == 1);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(5, 4)).front() == &unit);
}

TEST_CASE("Units stack without limit by default", "[unit][index]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).size() == 2);
}

TEST_CASE("Single-unit-per-tile rule blocks placement and movement onto occupied tiles",
          "[unit][index]")
{
    FactionFixture fixture;
    fixture.map.GetUnitPositions().SetSingleUnitPerTile(true);
    Faction& faction = fixture.MakeFaction();
    MovementHarness_ move(fixture);

    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    Unit& blocker = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    CHECK_THROWS_AS(fixture.MakeUnit(faction, 4, 4, {"test_chassis"}), std::runtime_error);
    CHECK_FALSE(CanPlaceUnitOnTile(fixture.At(4, 4), fixture.map.GetUnitPositions()));

    Unit& mover = fixture.MakeUnit(faction, 5, 4, {"test_chassis"});
    CHECK_FALSE(move.steps.CanStep(mover, mover.GetTile(), fixture.At(4, 4)));
    CHECK(&mover.GetTile() == &fixture.At(5, 4));
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).size() == 1);

    // Moving onto the tile the unit already occupies is a no-op.
    fixture.map.GetUnitPositions().MoveUnit(mover, fixture.At(5, 4));
    CHECK(&mover.GetTile() == &fixture.At(5, 4));

    // The index refuses the illegal move itself, not just the planner. A caller that skips the
    // step check can no longer overstack behind its back — which is what made the old
    // file-scope flag an incomplete fix.
    CHECK_THROWS_AS(fixture.map.GetUnitPositions().MoveUnit(mover, fixture.At(4, 4)),
                    std::logic_error);
    CHECK(&mover.GetTile() == &fixture.At(5, 4));

    // Destroying the blocker frees the tile.
    faction.GetUnitManager().DestroyUnit(blocker);
    CHECK(CanPlaceUnitOnTile(fixture.At(4, 4), fixture.map.GetUnitPositions()));
    MoveOrder_t stepOrder{&fixture.At(4, 4)};
    REQUIRE(move.orders.TryStep(mover, fixture.At(4, 4), stepOrder).bEntered);
    CHECK(&mover.GetTile() == &fixture.At(4, 4));
}

TEST_CASE("A loaded carrier moves under the stacking rule", "[unit][index][transport]")
{
    // The embarked exemption in MoveUnit is load-bearing: cargo is towed by a recursive
    // MoveUnit onto the tile the carrier just took, so without the exemption every loaded move
    // would throw. Delete `!rUnit.IsEmbarked()` and this is the test that catches it.
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(-10); // all sea, so the transport can move
    }

    // Load first: under the strict rule the passenger could not be *created* beside the
    // carrier, since it is not cargo yet. (That tension is inherent to the rule, not to this
    // change — boarding in the shipped game happens by stepping onto the carrier's tile.)
    Unit& transport = fixture.MakeUnit(faction, 4, 4, {"test_sea_chassis", "test_transport"});
    Unit& cargo = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    cargo.EmbarkInto(transport);
    REQUIRE(cargo.IsEmbarked());

    fixture.map.GetUnitPositions().SetSingleUnitPerTile(true);

    // Two units share (4,4) legally — the passenger is not an independent occupant.
    CHECK_NOTHROW(fixture.map.GetUnitPositions().MoveUnit(transport, fixture.At(5, 4)));
    CHECK(&transport.GetTile() == &fixture.At(5, 4));
    CHECK(&cargo.GetTile() == &fixture.At(5, 4));

    // ...and the tile they left is free again, while the one they took is not.
    CHECK(fixture.map.GetUnitPositions().CanPlaceUnit(fixture.At(4, 4)));
    CHECK_FALSE(fixture.map.GetUnitPositions().CanPlaceUnit(fixture.At(5, 4)));
}

TEST_CASE("EmbarkInto refuses a carrier it cannot legally board", "[unit][transport]")
{
    // These invariants used to be documented as caller duties, so one missed call site could
    // overfill m_cargo (FreeCargoSlots goes negative) or link a passenger to a carrier on
    // another tile that MoveUnit would still tow.
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(-10);
    }

    Unit& transport = fixture.MakeUnit(faction, 4, 4, {"test_sea_chassis", "test_transport"});
    Unit& farAway = fixture.MakeUnit(faction, 7, 7, {"test_chassis"});
    Unit& sameTile = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    // Different tiles.
    CHECK_THROWS_AS(farAway.EmbarkInto(transport), std::invalid_argument);
    CHECK_FALSE(farAway.IsEmbarked());

    // A unit cannot carry itself.
    CHECK_THROWS_AS(transport.EmbarkInto(transport), std::invalid_argument);

    // A plain chassis is not a carrier at all (no cargo capacity).
    CHECK_THROWS_AS(transport.EmbarkInto(sameTile), std::invalid_argument);

    // The legal case still works, and re-embarking the same carrier is a no-op, not a throw.
    CHECK_NOTHROW(sameTile.EmbarkInto(transport));
    CHECK(sameTile.IsEmbarked());
    CHECK_NOTHROW(sameTile.EmbarkInto(transport));
    CHECK(transport.GetCargo().size() == 1);
}

TEST_CASE("The stacking rule is per world, not per process", "[unit][index]")
{
    // It used to be a file-scope global in MovementRules, so two worlds could not disagree and
    // a test that set it leaked into the next case.
    FactionFixture strict;
    strict.map.GetUnitPositions().SetSingleUnitPerTile(true);
    FactionFixture loose;

    CHECK(strict.map.GetUnitPositions().IsSingleUnitPerTile());
    CHECK_FALSE(loose.map.GetUnitPositions().IsSingleUnitPerTile());

    // The loose world still stacks freely while the strict one refuses.
    Faction& rLooseFaction = loose.MakeFaction();
    loose.MakeUnit(rLooseFaction, 4, 4, {"test_chassis"});
    CHECK_NOTHROW(loose.MakeUnit(rLooseFaction, 4, 4, {"test_chassis"}));

    Faction& rStrictFaction = strict.MakeFaction();
    strict.MakeUnit(rStrictFaction, 4, 4, {"test_chassis"});
    CHECK_THROWS(strict.MakeUnit(rStrictFaction, 4, 4, {"test_chassis"}));
}
