#include "GameFixtures.h"

#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/units/EvacuateTerritoryEffects.h"
#include "game/units/EvacuateTerritoryRules.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::FactionFixture;

namespace
{

EvacuateTerritoryResult_t Evacuate_(FactionFixture& rFixture, Faction& rGuest,
                                    FactionId_t hostId)
{
    return EvacuateUnitsFromTerritory(rGuest, hostId, rFixture.map,
                                      rFixture.dataContext.interactionGrids);
}

} // namespace

TEST_CASE("Evacuate moves guest unit to nearest own territory without spending moves",
          "[evacuate]")
{
    FactionFixture fixture;
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();

    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);

    REQUIRE(fixture.map.GetTerritory().GetOwner(10, 10) == host.GetFactionId());
    REQUIRE(fixture.map.GetTerritory().GetOwner(5, 5) == guest.GetFactionId());

    Unit& unit = fixture.MakeUnit(guest, 10, 10, {"test_chassis"});
    unit.SetOrder(MoveOrder_t{&fixture.At(11, 11)});
    const int movesBefore = unit.GetMoveFragmentsRemaining();
    REQUIRE(movesBefore > 0);

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 1);
    CHECK(result.unitsLeftInPlace == 0);
    CHECK_FALSE(unit.GetOrder().has_value());
    CHECK(unit.GetMoveFragmentsRemaining() == movesBefore);
    CHECK(fixture.map.GetTerritory().GetOwner(unit.GetTile()) == guest.GetFactionId());
    CHECK(&unit.GetTile() != &fixture.At(10, 10));
}

TEST_CASE("Evacuate leaves a covert unit in the host's territory", "[evacuate][covert]")
{
    FactionFixture fixture;
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();

    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);
    REQUIRE(fixture.map.GetTerritory().GetOwner(10, 10) == host.GetFactionId());

    Unit& covert = fixture.MakeUnit(guest, 10, 10, {"test_chassis", "covert"});
    covert.SetOrder(MoveOrder_t{&fixture.At(11, 11)});

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 0);
    CHECK(&covert.GetTile() == &fixture.At(10, 10));
    CHECK(covert.GetOrder().has_value());
}

TEST_CASE("Evacuate leaves units on own or unowned tiles alone", "[evacuate]")
{
    FactionFixture fixture;
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();

    // A two-row water band isolates the north rows from both land bases so (12, 0) stays
    // unowned.
    for (const auto& pTile : fixture.map.GetTiles())
    {
        if (pTile->GetY() == 2 || pTile->GetY() == 3)
        {
            pTile->SetElevation(-100);
        }
    }

    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);
    REQUIRE_FALSE(fixture.map.GetTerritory().HasOwner(12, 0));

    Unit& onOwn = fixture.MakeUnit(guest, 5, 5, {"test_chassis"});
    onOwn.SetOrder(HoldOrder_t{});

    Unit& onUnowned = fixture.MakeUnit(guest, 12, 0, {"test_chassis"});
    onUnowned.SetOrder(HoldOrder_t{});

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 0);
    CHECK(result.unitsLeftInPlace == 0);
    CHECK(&onOwn.GetTile() == &fixture.At(5, 5));
    CHECK(&onUnowned.GetTile() == &fixture.At(12, 0));
    CHECK(onOwn.GetOrder().has_value());
    CHECK(onUnowned.GetOrder().has_value());
}

TEST_CASE("Evacuate skips unholdable own land for a sea unit", "[evacuate][domain]")
{
    FactionFixture fixture;
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();

    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);

    Unit& ship = fixture.MakeUnit(guest, 10, 10, {"test_sea_chassis"});
    REQUIRE(fixture.map.GetTerritory().GetOwner(10, 10) == host.GetFactionId());

    // Plain guest land nearer than the base must not be chosen; the own base harbors sea.
    REQUIRE(fixture.map.GetTerritory().GetOwner(7, 7) == guest.GetFactionId());
    REQUIRE_FALSE(CanHoldTileWithoutCarrier(ship, fixture.At(7, 7), fixture.map,
                                            fixture.dataContext.interactionGrids));
    REQUIRE(CanHoldTileWithoutCarrier(ship, fixture.At(5, 5), fixture.map,
                                      fixture.dataContext.interactionGrids));

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 1);
    CHECK(&ship.GetTile() == &fixture.At(5, 5));
}

TEST_CASE("Evacuate takes the next tile when the nearest own tile is full", "[evacuate]")
{
    FactionFixture fixture;
    fixture.map.GetUnitPositions().SetSingleUnitPerTile(true);
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();

    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);

    const Tile* pNearest = nullptr;
    {
        Unit& probe = fixture.MakeUnit(guest, 10, 10, {"test_chassis"});
        pNearest = FindNearestOwnTerritoryTile(probe, fixture.map,
                                               fixture.dataContext.interactionGrids);
        REQUIRE(pNearest);
        guest.GetUnitManager().DestroyUnit(probe);
    }

    Unit& blocker = fixture.MakeUnit(guest, pNearest->GetX(), pNearest->GetY(),
                                     {"test_chassis"});
    Unit& mover = fixture.MakeUnit(guest, 10, 10, {"test_chassis"});

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 1);
    CHECK(&mover.GetTile() != pNearest);
    CHECK(&blocker.GetTile() == pNearest);
    CHECK(fixture.map.GetTerritory().GetOwner(mover.GetTile()) == guest.GetFactionId());
}

TEST_CASE("Evacuate moves co-stacked units on one host tile to the same own tile",
          "[evacuate]")
{
    FactionFixture fixture;
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();

    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);
    REQUIRE(fixture.map.GetTerritory().GetOwner(10, 10) == host.GetFactionId());

    Unit& a = fixture.MakeUnit(guest, 10, 10, {"test_chassis"});
    Unit& b = fixture.MakeUnit(guest, 10, 10, {"test_chassis"});
    const Tile* pDest =
        FindNearestOwnTerritoryTile(a, fixture.map, fixture.dataContext.interactionGrids);
    REQUIRE(pDest);

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 2);
    CHECK(&a.GetTile() == pDest);
    CHECK(&b.GetTile() == pDest);
}

TEST_CASE("Evacuate leaves unit in place and clears order when no own territory exists",
          "[evacuate]")
{
    FactionFixture fixture;
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();

    fixture.MakeFactionBase(host, 8, 8);
    REQUIRE(fixture.map.GetTerritory().GetOwner(9, 9) == host.GetFactionId());
    // Guest founded no bases — no own territory anywhere.
    for (const auto& pTile : fixture.map.GetTiles())
    {
        REQUIRE(fixture.map.GetTerritory().GetOwner(*pTile) != guest.GetFactionId());
    }
    Unit& unit = fixture.MakeUnit(guest, 9, 9, {"test_chassis"});
    unit.SetOrder(MoveOrder_t{&fixture.At(8, 8)});

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 0);
    CHECK(result.unitsLeftInPlace == 1);
    CHECK(&unit.GetTile() == &fixture.At(9, 9));
    CHECK_FALSE(unit.GetOrder().has_value());
}

TEST_CASE("Evacuate tows embarked cargo with the carrier and clears both orders",
          "[evacuate][transport]")
{
    FactionFixture fixture;
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();

    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(-100);
    }
    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);

    Unit& transport =
        fixture.MakeUnit(guest, 10, 10, {"test_sea_chassis", "test_transport"});
    Unit& cargo = fixture.MakeUnit(guest, 10, 10, {"test_chassis"});
    cargo.EmbarkInto(transport);
    REQUIRE(cargo.IsEmbarked());

    transport.SetOrder(HoldOrder_t{});
    cargo.SetOrder(HoldOrder_t{});

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 1);
    CHECK(result.unitsLeftInPlace == 0);
    CHECK(fixture.map.GetTerritory().GetOwner(transport.GetTile()) == guest.GetFactionId());
    CHECK(&cargo.GetTile() == &transport.GetTile());
    CHECK(cargo.IsEmbarked());
    CHECK_FALSE(transport.GetOrder().has_value());
    CHECK_FALSE(cargo.GetOrder().has_value());
}

TEST_CASE("Evacuate prefers the wrap-short path to own territory", "[evacuate][wrap]")
{
    FactionFixture fixture(16, actest::k_TestMapHeight);
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();

    fixture.MakeFactionBase(guest, 0, 4);
    fixture.MakeFactionBase(host, 8, 4);

    REQUIRE(fixture.map.GetTerritory().GetOwner(14, 4) == guest.GetFactionId());
    REQUIRE(fixture.map.GetTerritory().GetOwner(10, 4) == host.GetFactionId());

    Unit& unit = fixture.MakeUnit(guest, 10, 4, {"test_chassis"});
    const Tile& rOrigin = fixture.At(10, 4);
    const int longDistToBase =
        ChebyshevDistance(rOrigin, fixture.At(0, 4), fixture.map.GetWidth());
    const Tile* pDest = FindNearestOwnTerritoryTile(unit, fixture.map,
                                                    fixture.dataContext.interactionGrids);
    REQUIRE(pDest);
    const int expectedDist =
        ChebyshevDistance(rOrigin, *pDest, fixture.map.GetWidth());
    REQUIRE(expectedDist < longDistToBase);

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 1);
    CHECK(&unit.GetTile() == pDest);
    CHECK(fixture.map.GetTerritory().GetOwner(unit.GetTile()) == guest.GetFactionId());
}
