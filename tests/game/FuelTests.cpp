#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/effects/EffectEnums.h"
#include "game/units/AttackRules.h"
#include "game/units/FuelRules.h"
#include "game/units/MovementConstants.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>
#include <ranges>

using namespace ac;
using namespace actest;

namespace
{

constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;

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

struct FuelHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    std::mt19937 rng;
    UnitOrderExecutor orders;

    explicit FuelHarness_(WorldFixture& fixture)
        : moveCosts(fixture.improvements)
        , steps(fixture.map, *fixture.ctx)
        , pathfinder(moveCosts, steps, fixture.map)
        , orders(moveCosts, steps, fixture.map, *fixture.ctx, pathfinder, fixture.morale(), rng)
    {
    }
};

size_t CountUnits_(Faction& rFaction)
{
    return static_cast<size_t>(std::ranges::distance(rFaction.GetUnitManager().Units()));
}

void RefreshMoves_(Unit& rUnit)
{
    rUnit.SetMoveFragmentsRemaining(
        rUnit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint);
}

} // namespace

TEST_CASE("Max fuel equals turns_of_fuel times movement", "[fuel]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& jet = fixture.MakeUnit(faction, 4, 4, {"test_fuel_flight_chassis"});

    REQUIRE(jet.GetMovementPoints() == 4);
    REQUIRE(jet.GetStat(StatId_t::TurnsOfFuel) == 2);
    CHECK(jet.GetDesign().UsesFuel());
    CHECK(jet.GetMaxFuel() == 8);
    CHECK(jet.GetCurrentFuel() == 8);
}

TEST_CASE("Away turns burn a full movement of fuel and destroy at zero with 100% damage",
          "[fuel]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& jet = fixture.MakeUnit(faction, 4, 4, {"test_fuel_flight_chassis"});
    REQUIRE(jet.GetCurrentFuel() == 8);

    ProcessFuelAtTurnEnd(jet, fixture.map);
    CHECK(jet.GetCurrentFuel() == 4);

    RefreshMoves_(jet);
    ProcessFuelAtTurnEnd(jet, fixture.map);
    CHECK(CountUnits_(faction) == 0);
}

TEST_CASE("Partial move then end turn burns the same net fuel as a full away turn", "[fuel]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    FuelHarness_ harness(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& jet = fixture.MakeUnit(faction, 4, 4, {"test_fuel_flight_chassis"});
    REQUIRE(jet.GetCurrentFuel() == 8);
    REQUIRE(jet.GetMoveFragmentsRemaining() == 4 * k_point);

    MoveOrder_t stepOrder{&fixture.At(5, 4)};
    REQUIRE(harness.orders.TryStep(jet, fixture.At(5, 4), stepOrder).bEntered);
    CHECK(jet.GetCurrentFuel() == 7);
    CHECK(jet.GetMoveFragmentsRemaining() == 3 * k_point);

    ProcessFuelAtTurnEnd(jet, fixture.map);
    CHECK(jet.GetCurrentFuel() == 4);
}

TEST_CASE("AttackingEndsTurn spends all remaining moves and matching fuel", "[fuel][attack]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    FuelHarness_ harness(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& jet = fixture.MakeUnit(player, 4, 4, {"test_fuel_flight_chassis", "test_weapon"});
    fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "test_weapon"});
    REQUIRE(jet.GetFlag(RuleFlagId_t::AttackingEndsTurn));
    REQUIRE(jet.GetCurrentFuel() == 8);
    REQUIRE(jet.GetMoveFragmentsRemaining() == 4 * k_point);

    REQUIRE(harness.orders.TryAttack(jet, fixture.At(5, 4)).has_value());
    CHECK(jet.GetMoveFragmentsRemaining() == 0);
    CHECK(jet.GetCurrentFuel() == 4);
    CHECK_FALSE(CanDeclareAttack(jet, fixture.At(5, 4), fixture.map, *fixture.ctx));
}

TEST_CASE("End turn on Base, Airbase, or friendly carrier refuels without damage", "[fuel]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();

    SECTION("Base")
    {
        fixture.MakeFactionBase(faction, 4, 4);
        Unit& jet = fixture.MakeUnit(faction, 4, 4, {"test_fuel_flight_chassis"});
        jet.SetCurrentFuel(1);
        ProcessFuelAtTurnEnd(jet, fixture.map);
        CHECK(jet.GetCurrentFuel() == jet.GetMaxFuel());
        CHECK(jet.GetCurrentHp() == jet.GetStat(StatId_t::HitPoints));
    }

    SECTION("Airbase")
    {
        fixture.ctx->AddImprovementWithEffects(fixture.At(4, 4), "Airbase");
        Unit& jet = fixture.MakeUnit(faction, 4, 4, {"test_fuel_flight_chassis"});
        jet.SetCurrentFuel(0);
        ProcessFuelAtTurnEnd(jet, fixture.map);
        CHECK(jet.GetCurrentFuel() == jet.GetMaxFuel());
        CHECK(CountUnits_(faction) == 1);
    }

    SECTION("friendly carrier deck — already embarked")
    {
        MakeWater_(fixture.At(5, 4));
        Unit& carrier = fixture.MakeUnit(
            faction, 5, 4, {"test_sea_chassis", "test_transport", "test_carrier_deck"});
        Unit& jet = fixture.MakeUnit(faction, 5, 4, {"test_fuel_flight_chassis"});
        REQUIRE(TryAttachToTransport(jet, fixture.map));
        jet.SetCurrentFuel(0);
        ProcessFuelAtTurnEnd(jet, fixture.map);
        CHECK(jet.GetCurrentFuel() == jet.GetMaxFuel());
        CHECK(jet.IsEmbarked());
        CHECK(jet.GetCarrier() == &carrier);
    }

    SECTION("friendly carrier deck — auto-lands at turn end")
    {
        MakeWater_(fixture.At(5, 4));
        Unit& carrier = fixture.MakeUnit(
            faction, 5, 4, {"test_sea_chassis", "test_transport", "test_carrier_deck"});
        Unit& jet = fixture.MakeUnit(faction, 5, 4, {"test_fuel_flight_chassis"});
        CHECK_FALSE(jet.IsEmbarked());
        jet.SetCurrentFuel(0);
        ProcessFuelAtTurnEnd(jet, fixture.map);
        CHECK(jet.IsEmbarked());
        CHECK(jet.GetCarrier() == &carrier);
        CHECK(jet.GetCurrentFuel() == jet.GetMaxFuel());
    }
}

TEST_CASE("Carrier deck does not refuel air that could not land (no cargo slot)", "[fuel]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    MakeWater_(fixture.At(5, 4));

    // test_transport cargo_capacity 1: first jet lands, second shares the tile but stays aloft.
    Unit& carrier = fixture.MakeUnit(
        faction, 5, 4, {"test_sea_chassis", "test_transport", "test_carrier_deck"});
    Unit& landed = fixture.MakeUnit(faction, 5, 4, {"test_fuel_flight_chassis"});
    REQUIRE(TryAttachToTransport(landed, fixture.map));
    REQUIRE(landed.IsEmbarked());
    REQUIRE(FreeCargoSlots(carrier) == 0);

    Unit& stranded = fixture.MakeUnit(faction, 5, 4, {"test_fuel_flight_chassis"});
    CHECK_FALSE(stranded.IsEmbarked());
    CHECK_FALSE(TryAttachToTransport(stranded, fixture.map));
    stranded.SetCurrentFuel(0);

    ProcessFuelAtTurnEnd(stranded, fixture.map);
    CHECK(CountUnits_(faction) == 2); // carrier + landed; stranded destroyed at 0 fuel / 100%
    CHECK(landed.IsEmbarked());
}

TEST_CASE("Copter takes 30% damage per away turn at zero fuel and survives", "[fuel]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& copter = fixture.MakeUnit(faction, 4, 4, {"test_copter_chassis"});
    REQUIRE(copter.GetMaxFuel() == 4);
    REQUIRE(copter.GetCurrentFuel() == 4);
    REQUIRE(copter.GetCurrentHp() == 10);

    ProcessFuelAtTurnEnd(copter, fixture.map);
    CHECK(copter.GetCurrentFuel() == 0);
    CHECK(copter.GetCurrentHp() == 7);
    CHECK(CountUnits_(faction) == 1);

    RefreshMoves_(copter);
    ProcessFuelAtTurnEnd(copter, fixture.map);
    CHECK(copter.GetCurrentFuel() == 0);
    CHECK(copter.GetCurrentHp() == 4);
    CHECK(CountUnits_(faction) == 1);
}

TEST_CASE("Chassis without turns_of_fuel ignores turn-end fuel processing", "[fuel]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& grav = fixture.MakeUnit(faction, 4, 4, {"test_flight_chassis"});
    CHECK_FALSE(grav.GetDesign().UsesFuel());
    CHECK(grav.GetMaxFuel() == 0);
    CHECK(grav.GetCurrentFuel() == 0);

    ProcessFuelAtTurnEnd(grav, fixture.map);
    CHECK(grav.GetCurrentFuel() == 0);
    CHECK(grav.GetCurrentHp() == grav.GetStat(StatId_t::HitPoints));
    CHECK(CountUnits_(faction) == 1);
}
