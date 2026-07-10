// Unit-position state model: UnitPositionIndex is the single owner — units register for
// their lifetime (RAII), movement goes through TryMoveUnit (which keeps the unit's tile
// pointer and the occupancy lists in sync), and the static single-unit-per-tile stacking
// rule is enforced at placement and movement.

#include "GameFixtures.h"

#include "game/faction/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace ac;

namespace
{

// Restores the default stacking rule even if a test assertion fails mid-case.
struct SingleUnitPerTileGuard
{
    SingleUnitPerTileGuard() { UnitPositionIndex::SetSingleUnitPerTile(true); }
    ~SingleUnitPerTileGuard() { UnitPositionIndex::SetSingleUnitPerTile(false); }
};

} // namespace

TEST_CASE("Unit creation and destruction maintain the position index", "[unit][index]")
{
    actest::FactionFixture fixture;
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
    actest::FactionFixture fixture;
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
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    REQUIRE(fixture.map.GetUnitPositions().TryMoveUnit(unit, fixture.At(5, 4)));

    CHECK(&unit.GetTile() == &fixture.At(5, 4));
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).empty());
    REQUIRE(fixture.map.GetUnitsOnTile(fixture.At(5, 4)).size() == 1);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(5, 4)).front() == &unit);
}

TEST_CASE("Units stack without limit by default", "[unit][index]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).size() == 2);
}

TEST_CASE("Single-unit-per-tile rule blocks placement and movement onto occupied tiles",
          "[unit][index]")
{
    SingleUnitPerTileGuard guard;
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    Unit& blocker = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    CHECK_THROWS_AS(fixture.MakeUnit(faction, 4, 4, {"test_chassis"}), std::runtime_error);

    Unit& mover = fixture.MakeUnit(faction, 5, 4, {"test_chassis"});
    CHECK_FALSE(fixture.map.GetUnitPositions().TryMoveUnit(mover, fixture.At(4, 4)));
    CHECK(&mover.GetTile() == &fixture.At(5, 4));
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).size() == 1);

    // Moving onto the tile the unit already occupies is a successful no-op.
    CHECK(fixture.map.GetUnitPositions().TryMoveUnit(mover, fixture.At(5, 4)));

    // Destroying the blocker frees the tile.
    faction.GetUnitManager().DestroyUnit(blocker);
    CHECK(fixture.map.GetUnitPositions().TryMoveUnit(mover, fixture.At(4, 4)));
    CHECK(&mover.GetTile() == &fixture.At(4, 4));
}
