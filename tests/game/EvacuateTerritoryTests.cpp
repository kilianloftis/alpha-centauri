#include "GameFixtures.h"

#include "game/base/BaseManager.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticStatus.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/movement/EvacuateTerritoryMutations.h"
#include "game/movement/EvacuateTerritoryRules.h"
#include "game/movement/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::FactionFixture;

namespace
{

EvacuateTerritoryResult_t Evacuate_(FactionFixture& rFixture, Faction& rGuest,
                                    FactionId_t hostId)
{
    return EvacuateUnitsFromTerritory(rGuest, hostId, rFixture.map);
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
    REQUIRE_FALSE(CanHoldTileWithoutCarrier(ship, fixture.At(7, 7)));
    REQUIRE(CanHoldTileWithoutCarrier(ship, fixture.At(5, 5)));

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 1);
    CHECK(&ship.GetTile() == &fixture.At(5, 5));
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
        FindNearestOwnTerritoryTile(a);
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
    cargo.Transport().EmbarkInto(transport);
    REQUIRE(cargo.Transport().IsEmbarked());

    transport.SetOrder(HoldOrder_t{});
    cargo.SetOrder(HoldOrder_t{});

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 1);
    CHECK(result.unitsLeftInPlace == 0);
    CHECK(fixture.map.GetTerritory().GetOwner(transport.GetTile()) == guest.GetFactionId());
    CHECK(&cargo.GetTile() == &transport.GetTile());
    CHECK(cargo.Transport().IsEmbarked());
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
    const Tile* pDest = FindNearestOwnTerritoryTile(unit);
    REQUIRE(pDest);
    const int expectedDist =
        ChebyshevDistance(rOrigin, *pDest, fixture.map.GetWidth());
    REQUIRE(expectedDist < longDistToBase);

    const EvacuateTerritoryResult_t result = Evacuate_(fixture, guest, host.GetFactionId());
    CHECK(result.unitsMoved == 1);
    CHECK(&unit.GetTile() == pDest);
    CHECK(fixture.map.GetTerritory().GetOwner(unit.GetTile()) == guest.GetFactionId());
}

TEST_CASE("A guest shares a tile with the host in its base or beside its unit", "[evacuate]")
{
    FactionFixture fixture;
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();
    fixture.MakeFactionBase(host, 11, 11);
    fixture.MakeUnit(host, 28, 4, {"test_chassis"});

    const Unit& inBase = fixture.MakeUnit(guest, 11, 11, {"test_chassis"});
    const Unit& besideUnit = fixture.MakeUnit(guest, 28, 4, {"test_chassis"});
    const Unit& onTerritory = fixture.MakeUnit(guest, 10, 10, {"test_chassis"});

    CHECK(SharesTileWith(inBase, host));
    CHECK(SharesTileWith(besideUnit, host));
    CHECK_FALSE(SharesTileWith(onTerritory, host));
    CHECK(MustLeaveTerritory(onTerritory, host.GetFactionId(), fixture.map.GetTerritory()));
    CHECK_FALSE(MustLeaveTerritory(besideUnit, host.GetFactionId(), fixture.map.GetTerritory()));
}

TEST_CASE("MustVacateCurrentTerritory follows MayEnterTerritoryOf", "[evacuate]")
{
    FactionFixture fixture;
    actest::FillLand(fixture.map);
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();
    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);

    DiplomacyLedger& rLedger = fixture.state.GetDiplomacyLedger();
    rLedger.SetStatus(guest.GetFactionId(), host.GetFactionId(), DiplomaticStatus_t::Treaty);

    Unit& covert = fixture.MakeUnit(guest, 10, 10, {"test_chassis", "covert"});
    Unit& open = fixture.MakeUnit(guest, 9, 9, {"test_chassis"});
    CHECK_FALSE(MustVacateCurrentTerritory(covert));
    CHECK(MustVacateCurrentTerritory(open));

    rLedger.SetStatus(guest.GetFactionId(), host.GetFactionId(), DiplomaticStatus_t::Pact);
    CHECK_FALSE(MustVacateCurrentTerritory(open));
}

TEST_CASE("EvacuateUnit relocates one illegal guest", "[evacuate]")
{
    FactionFixture fixture;
    actest::FillLand(fixture.map);
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();
    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);

    fixture.state.GetDiplomacyLedger().SetStatus(
        guest.GetFactionId(), host.GetFactionId(), DiplomaticStatus_t::Treaty);

    Unit& unit = fixture.MakeUnit(guest, 10, 10, {"test_chassis"});
    unit.SetOrder(HoldOrder_t{});
    const EvacuateTerritoryResult_t result = EvacuateUnit(unit, fixture.map);
    CHECK(result.unitsMoved == 1);
    CHECK_FALSE(unit.GetOrder().has_value());
    CHECK(fixture.map.GetTerritory().GetOwner(unit.GetTile()) == guest.GetFactionId());
}

TEST_CASE("ApplyEvacuateOnTerritoryChange is a no-op while the switch is off", "[evacuate]")
{
    FactionFixture fixture;
    actest::FillLand(fixture.map);
    Faction& guest = fixture.MakeFaction();
    Faction& host = fixture.MakeFaction();
    fixture.MakeFactionBase(guest, 5, 5);
    fixture.MakeFactionBase(host, 11, 11);
    fixture.state.GetDiplomacyLedger().SetStatus(
        guest.GetFactionId(), host.GetFactionId(), DiplomaticStatus_t::Treaty);

    Unit& unit = fixture.MakeUnit(guest, 10, 10, {"test_chassis"});
    REQUIRE(MustVacateCurrentTerritory(unit));
    REQUIRE_FALSE(EvacuateUnitsOnTerritoryChange());

    CHECK(ApplyEvacuateOnTerritoryChange(unit, fixture.map).unitsMoved == 0);
    CHECK(ApplyEvacuateOnTerritoryChange(fixture.state).unitsMoved == 0);
    CHECK(&unit.GetTile() == &fixture.At(10, 10));
}
