// Unit-position state model: UnitPositionIndex is the single owner — units register for
// their lifetime (RAII), movement goes through MoveUnit (which keeps the unit's tile
// pointer and the occupancy lists in sync). Stacking legality lives in MovementRules.

#include "GameFixtures.h"

#include "game/faction/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/units/MovementRules.h"
#include "game/units/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace ac;
using namespace actest;

namespace
{

// Restores the default stacking rule even if a test assertion fails mid-case.
struct SingleUnitPerTileGuard
{
    SingleUnitPerTileGuard() { SetSingleUnitPerTile(true); }
    ~SingleUnitPerTileGuard() { SetSingleUnitPerTile(false); }
};

struct MovementHarness_
{
    StepEvaluator steps;
    UnitOrderExecutor orders;

    explicit MovementHarness_(WorldFixture& fixture)
        : steps(fixture.improvements, fixture.map, *fixture.ctx)
        , orders(fixture.improvements, fixture.map, *fixture.ctx)
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
    SingleUnitPerTileGuard guard;
    FactionFixture fixture;
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
    CHECK_FALSE(move.steps.CanStep(mover, fixture.At(4, 4)));
    CHECK(&mover.GetTile() == &fixture.At(5, 4));
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).size() == 1);

    // Moving onto the tile the unit already occupies is a no-op.
    fixture.map.GetUnitPositions().MoveUnit(mover, fixture.At(5, 4));
    CHECK(&mover.GetTile() == &fixture.At(5, 4));

    // Destroying the blocker frees the tile.
    faction.GetUnitManager().DestroyUnit(blocker);
    CHECK(CanPlaceUnitOnTile(fixture.At(4, 4), fixture.map.GetUnitPositions()));
    MoveOrder_t stepOrder{&fixture.At(4, 4)};
    REQUIRE(move.orders.TryStep(mover, fixture.At(4, 4), stepOrder));
    CHECK(&mover.GetTile() == &fixture.At(4, 4));
}
