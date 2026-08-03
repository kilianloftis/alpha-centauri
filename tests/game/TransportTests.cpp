#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/units/BaseConquestRules.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementRules.h"
#include "game/units/StepEvaluator.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/Pathfinder.h"
#include "game/units/MovementConstants.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>

using namespace ac;
using namespace actest;

namespace
{

void MakeWater_(Tile& rTile)
{
    rTile.SetElevation(-100);
}

void MakeLand_(Tile& rTile)
{
    rTile.SetElevation(100);
}

void FillLand_(WorldFixture& fixture)
{
    for (auto& pTile : fixture.map.GetTiles())
    {
        MakeLand_(*pTile);
    }
}

struct TransportHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    std::mt19937 rng;
    UnitOrderExecutor orders;

    explicit TransportHarness_(WorldFixture& fixture)
        : moveCosts(fixture.improvements)
        , steps(fixture.map, *fixture.ctx)
        , pathfinder(moveCosts, steps, fixture.map)
        , orders(moveCosts, steps, fixture.map, *fixture.ctx, pathfinder, fixture.morale(), rng)
    {
    }
};

} // namespace

TEST_CASE("Transport special applies movement penalties by domain", "[transport][stats]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();

    Unit& land = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_transport"});
    // Chassis movement 2, Transport −1 → 1.
    CHECK(land.GetStat(StatId_t::Movement) == 1);

    MakeWater_(fixture.At(4, 5));
    Unit& sea = fixture.MakeUnit(faction, 4, 5, {"test_sea_chassis", "test_transport"});
    CHECK(sea.GetStat(StatId_t::Movement) == 1);

    Unit& air = fixture.MakeUnit(faction, 5, 4, {"test_fuel_flight_chassis", "test_transport"});
    // Fuel flight chassis movement 4: (4 + −1) * 0.5 → 1.
    CHECK(air.GetStat(StatId_t::Movement) == 1);
}

TEST_CASE("Board requires capacity; full transport rejects; attach via L and step",
          "[transport]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    TransportHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();

    MakeWater_(fixture.At(5, 4));
    Unit& transport = fixture.MakeUnit(faction, 5, 4, {"test_sea_chassis", "test_transport"});
    CHECK(FreeCargoSlots(transport) == 1);

    Unit& land = fixture.MakeUnit(faction, 5, 4, {"test_chassis"});
    REQUIRE(harness.orders.TryAttachToTransport(land));
    CHECK(land.IsEmbarked());
    CHECK(land.GetCarrier() == &transport);
    CHECK(transport.GetCargo().size() == 1);
    CHECK(FreeCargoSlots(transport) == 0);

    Unit& land2 = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    CHECK_FALSE(harness.steps.CanStep(land2, land2.GetTile(), fixture.At(5, 4)));
    CHECK_FALSE(harness.orders.TryAttachToTransport(land2));
}

TEST_CASE("Step onto transport auto-attaches; carrier move keeps cargo", "[transport]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    TransportHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();

    MakeWater_(fixture.At(5, 4));
    MakeWater_(fixture.At(6, 4));
    Unit& transport = fixture.MakeUnit(faction, 5, 4, {"test_sea_chassis", "test_transport"});
    Unit& land = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    MoveOrder_t order;
    REQUIRE(harness.orders.TryStep(land, fixture.At(5, 4), order).bEntered);
    CHECK(land.IsEmbarked());
    CHECK(land.GetCarrier() == &transport);
    CHECK(&land.GetTile() == &fixture.At(5, 4));

    MoveOrder_t seaOrder;
    REQUIRE(harness.orders.TryStep(transport, fixture.At(6, 4), seaOrder).bEntered);
    CHECK(&transport.GetTile() == &fixture.At(6, 4));
    CHECK(land.IsEmbarked());
    CHECK(&land.GetTile() == &fixture.At(6, 4));
}

TEST_CASE("Unload to adjacent land; destroying a carrier over water drowns its cargo",
          "[transport]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    TransportHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();

    MakeWater_(fixture.At(5, 4));
    Unit& transport = fixture.MakeUnit(faction, 5, 4, {"test_sea_chassis", "test_transport"});
    Unit& land = fixture.MakeUnit(faction, 5, 4, {"test_chassis"});
    REQUIRE(harness.orders.TryAttachToTransport(land));

    MoveOrder_t unload;
    REQUIRE(harness.orders.TryStep(land, fixture.At(4, 4), unload).bEntered);
    CHECK_FALSE(land.IsEmbarked());
    CHECK(&land.GetTile() == &fixture.At(4, 4));
    CHECK(transport.GetCargo().empty());

    Unit& land2 = fixture.MakeUnit(faction, 5, 4, {"test_chassis"});
    REQUIRE(harness.orders.TryAttachToTransport(land2));
    size_t unitsBefore = 0;
    for (Unit& rUnit : faction.GetUnitManager().Units())
    {
        (void)rUnit;
        ++unitsBefore;
    }
    faction.GetUnitManager().DestroyUnit(transport);
    size_t unitsAfter = 0;
    for (Unit& rUnit : faction.GetUnitManager().Units())
    {
        (void)rUnit;
        ++unitsAfter;
    }
    // Carrier + cargo removed.
    CHECK(unitsAfter == unitsBefore - 2);
}

TEST_CASE("Entering a base garrisons it rather than boarding a transport there",
          "[transport][base]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    TransportHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();

    BaseManager& rBase = fixture.MakeFactionBase(faction, 4, 4);
    Unit& airTransport =
        fixture.MakeUnit(faction, 4, 4, {"test_flight_chassis", "test_air_transport"});
    Unit& land = fixture.MakeUnit(faction, 3, 4, {"test_chassis"});

    MoveOrder_t order;
    REQUIRE(harness.orders.TryStep(land, fixture.At(4, 4), order).bEntered);
    CHECK_FALSE(land.IsEmbarked());
    CHECK(airTransport.GetCargo().empty());

    // The explicit L order still boards: a base is a legal load site for an air transport.
    REQUIRE(harness.orders.TryAttachToTransport(land));
    REQUIRE(land.IsEmbarked());

    // Cargo parked in a base is still what holds the base.
    CHECK(HasBaseGarrison(rBase, fixture.map));
}

TEST_CASE("A carrier lost in a base sets its cargo down instead of drowning it",
          "[transport][base]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    TransportHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();

    BaseManager& rBase = fixture.MakeFactionBase(faction, 4, 4);
    Unit& airTransport =
        fixture.MakeUnit(faction, 4, 4, {"test_flight_chassis", "test_air_transport"});
    Unit& land = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    REQUIRE(harness.orders.TryAttachToTransport(land));
    REQUIRE(land.IsEmbarked());

    faction.GetUnitManager().DestroyUnit(airTransport);

    // The passenger is set down in the base, and the base is still held.
    CHECK_FALSE(land.IsEmbarked());
    CHECK(&land.GetTile() == &fixture.At(4, 4));
    CHECK(HasBaseGarrison(rBase, fixture.map));

    size_t survivors = 0;
    for (Unit& rUnit : faction.GetUnitManager().Units())
    {
        (void)rUnit;
        ++survivors;
    }
    CHECK(survivors == 1);
}

TEST_CASE("Carrier Deck allows air passengers; without it air cannot board", "[transport]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    TransportHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();

    MakeWater_(fixture.At(5, 4));
    MakeWater_(fixture.At(5, 5));
    Unit& sea = fixture.MakeUnit(faction, 5, 4, {"test_sea_chassis", "test_transport"});
    Unit& air = fixture.MakeUnit(faction, 5, 4, {"test_fuel_flight_chassis"});
    CHECK_FALSE(CanCarryPassenger(sea, air));
    CHECK_FALSE(harness.orders.TryAttachToTransport(air));

    Unit& carrier = fixture.MakeUnit(
        faction, 5, 5, {"test_sea_chassis", "test_transport", "test_carrier_deck"});
    fixture.MoveUnit(air, 5, 5);
    CHECK(CanCarryPassenger(carrier, air));
    REQUIRE(harness.orders.TryAttachToTransport(air));
    CHECK(air.IsEmbarked());
    CHECK(air.GetCarrier() == &carrier);
}

TEST_CASE("Must-land attach refuels; air carrier loads at Base/Airbase or carrier deck",
          "[transport]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    TransportHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();

    MakeWater_(fixture.At(5, 4));
    Unit& carrier = fixture.MakeUnit(
        faction, 5, 4, {"test_sea_chassis", "test_transport", "test_carrier_deck"});
    Unit& air = fixture.MakeUnit(faction, 5, 4, {"test_fuel_flight_chassis"});
    air.SetCurrentFuel(0);
    REQUIRE(harness.orders.TryAutoAttachWhenMustLand(air));
    CHECK(air.IsEmbarked());
    CHECK(air.GetCurrentFuel() == air.GetStat(StatId_t::Fuel));

    Unit& airTransport = fixture.MakeUnit(faction, 4, 4, {"test_flight_chassis", "test_air_transport"});
    Unit& land = fixture.MakeUnit(faction, 3, 4, {"test_chassis"});
    // Bare tile provides no loads_air_transport capability — air carrier cannot load.
    CHECK_FALSE(CanLoadAtTile(airTransport, fixture.At(4, 4), fixture.map));
    fixture.MoveUnit(land, 4, 4);
    CHECK_FALSE(harness.orders.TryAttachToTransport(land));

    fixture.MakeFactionBase(faction, 4, 5);
    fixture.MoveUnit(airTransport, 4, 5);
    fixture.MoveUnit(land, 4, 5);
    CHECK(CanLoadAtTile(airTransport, fixture.At(4, 5), fixture.map));
    REQUIRE(harness.orders.TryAttachToTransport(land));
    CHECK(land.IsEmbarked());

    // Shift+U in-flight unload over land.
    fixture.MoveUnit(airTransport, 3, 3);
    // Cargo rides with MoveUnit.
    CHECK(&land.GetTile() == &fixture.At(3, 3));
    REQUIRE(harness.orders.TryUnloadTransport(airTransport));
    CHECK_FALSE(land.IsEmbarked());

    // A co-located carrier deck supplies loads_air_transport, so it is a valid load site.
    // SMAC ships carrier decks with refuels_air only; test_carrier_deck declares both, which
    // is exactly the one-flag config edit that opts into loading at sea.
    MakeWater_(fixture.At(5, 5));
    fixture.MakeUnit(faction, 5, 5, {"test_sea_chassis", "test_carrier_deck"});
    fixture.MoveUnit(airTransport, 5, 5);
    Unit& land2 = fixture.MakeUnit(faction, 5, 5, {"test_chassis"});
    CHECK(CanLoadAtTile(airTransport, fixture.At(5, 5), fixture.map));
    REQUIRE(harness.orders.TryAttachToTransport(land2));
    CHECK(land2.IsEmbarked());
    CHECK(land2.GetCarrier() == &airTransport);
}

TEST_CASE("Attack on transport tile hits carrier not cargo", "[transport][combat]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    TransportHarness_ harness(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    MakeWater_(fixture.At(5, 4));
    Unit& transport = fixture.MakeUnit(enemy, 5, 4, {"test_sea_chassis", "test_transport"});
    Unit& cargo = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    REQUIRE(ac::TryAttachToTransport(cargo, fixture.map));

    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
    Unit* pTarget = harness.orders.FindVisibleHostileOnTile(attacker, fixture.At(5, 4));
    REQUIRE(pTarget == &transport);
    CHECK(pTarget != &cargo);
}
