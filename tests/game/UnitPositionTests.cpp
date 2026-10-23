// Unit-position state model: UnitPositionIndex is the single owner — units register for
// their lifetime (RAII), movement goes through MoveUnit (which keeps the unit's tile
// pointer and the occupancy lists in sync). Stacking legality lives in MovementRules.

#include "GameFixtures.h"

#include "game/units/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/movement/MovementRules.h"
#include "game/movement/MoveCostRules.h"
#include "game/movement/Pathfinder.h"
#include "game/movement/StepEvaluator.h"
#include "game/movement/TransportRules.h"
#include "game/movement/TransportMutations.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitAttackExecutor.h"
#include "game/units/UnitMoveExecutor.h"
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
    MoveCostRules moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    std::mt19937 rng;
    UnitMoveExecutor moves;
    UnitOrderExecutor orders;
    UnitAttackExecutor attacks;

    explicit MovementHarness_(WorldFixture& fixture)
        : moveCosts(fixture.improvements, MovementConstants_t{})
        , steps(fixture.map, fixture.ctx)
        , pathfinder(moveCosts, steps, fixture.map)
        , moves(moveCosts, steps, fixture.map, rng, fixture.state)
        , orders(moves, steps, fixture.map, fixture.ctx, pathfinder,
                 *fixture.dataContext.terrainOperationRegistry, rng, fixture.state)
        , attacks(moveCosts, steps, fixture.map, fixture.ctx, fixture.morale(), rng, fixture.state,
                  orders)
    {
    }
};

} // namespace

namespace
{

void FillLand_(WorldFixture& fixture)
{
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
}

void MakeWater_(Tile& rTile)
{
    rTile.SetElevation(-100);
}

} // namespace

TEST_CASE("Unit creation and destruction maintain the position index", "[unit][index]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

    REQUIRE(fixture.map.GetUnitsOnTile(fixture.At(8, 8)).size() == 1);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(8, 8)).front() == &unit);

    // No manual index bookkeeping: the unit unregisters itself when destroyed.
    faction.GetUnitManager().DestroyUnit(unit);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(8, 8)).empty());
}

TEST_CASE("DestroyUnit emits OnUnitDestroyed before the unit is removed", "[unit][signal]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

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
    fixture.MakeUnit(faction, 7, 7, {"test_chassis"});
    fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    fixture.MakeUnit(faction, 9, 9, {"test_chassis"});

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
            CHECK(rUnit.GetUnitId() == visitedIds.back());

            // Queries must tolerate the null slots the deferral leaves in the unit list
            // and never offer a destroyed unit.
            CHECK(rUnits.GetNextAvailableUnit(nullptr) != &rUnit);
        }

        CHECK(rUnits.Units().empty());
        CHECK(rUnits.GetNextAvailableUnit(nullptr) == nullptr);
        CHECK_FALSE(rUnits.HasUnitsRequiringOrders());
    }

    CHECK(visitedIds.size() == 3);
    CHECK(rUnits.Units().empty());
}

TEST_CASE("MoveUnit emits OnUnitMoved after updating occupancy", "[unit][signal]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

    Unit* pMoved = nullptr;
    const Tile* pTileAtEmit = nullptr;
    fixture.map.GetUnitPositions().OnUnitMoved.Connect([&](Unit& rUnit) {
        pMoved = &rUnit;
        pTileAtEmit = &rUnit.GetTile();
    });

    fixture.map.GetUnitPositions().MoveUnit(unit, fixture.At(9, 9));

    CHECK(pMoved == &unit);
    CHECK(pTileAtEmit == &fixture.At(9, 9));

    // Same-tile move is a no-op and must not re-emit.
    pMoved = nullptr;
    fixture.map.GetUnitPositions().MoveUnit(unit, fixture.At(9, 9));
    CHECK(pMoved == nullptr);
}

TEST_CASE("Moving a unit keeps its tile pointer and the index in sync", "[unit][index]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

    fixture.map.GetUnitPositions().MoveUnit(unit, fixture.At(9, 9));

    CHECK(&unit.GetTile() == &fixture.At(9, 9));
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(8, 8)).empty());
    REQUIRE(fixture.map.GetUnitsOnTile(fixture.At(9, 9)).size() == 1);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(9, 9)).front() == &unit);
}

TEST_CASE("GetUnitsOnTile is occupants-only; cargo needs GetCargoOnTile", "[unit][index][harbors]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    MakeWater_(fixture.At(9, 9));
    Unit& carrier = fixture.MakeUnit(
        faction, 9, 9, {"test_sea_chassis", "test_transport", "test_carrier_deck"});
    Unit& jet = fixture.MakeUnit(faction, 9, 9, {"test_fuel_flight_chassis"});
    REQUIRE(TryAttachToTransport(jet));

    REQUIRE(fixture.map.GetUnitsOnTile(fixture.At(9, 9)).size() == 1);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(9, 9)).front() == &carrier);
    REQUIRE(fixture.map.GetCargoOnTile(fixture.At(9, 9)).size() == 1);
    CHECK(fixture.map.GetCargoOnTile(fixture.At(9, 9)).front() == &jet);
    REQUIRE(fixture.map.GetAllUnitsOnTile(fixture.At(9, 9)).size() == 2);
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

    Unit& transport = fixture.MakeUnit(faction, 8, 8, {"test_sea_chassis", "test_transport"});
    Unit& farAway = fixture.MakeUnit(faction, 8, 14, {"test_chassis"});
    Unit& sameTile = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});

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

