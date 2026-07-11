// Unit current-vs-max stat invariant: a fresh unit starts at its *live* resolved maxima,
// and the current-stat setters clamp to [0, live max] so current can never fall below zero
// or exceed the (effect-adjusted) maximum.

#include "GameFixtures.h"

#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("A fresh unit starts at its live resolved maxima", "[unit][stats]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    CHECK(unit.GetCurrentHp() == unit.GetHitPoints());
    CHECK(unit.GetCurrentFuel() == unit.GetFuel());
    CHECK(unit.GetMovesRemaining() == unit.GetMovement());
    CHECK(unit.GetXp() == 0);
}

TEST_CASE("Current-stat setters clamp to [0, live max]", "[unit][stats]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    // Overkill damage floors at zero rather than going negative.
    unit.SetCurrentHp(-5);
    CHECK(unit.GetCurrentHp() == 0);

    // Healing past the maximum is capped at the live max.
    unit.SetCurrentHp(unit.GetHitPoints() + 100);
    CHECK(unit.GetCurrentHp() == unit.GetHitPoints());

    unit.SetCurrentFuel(-1);
    CHECK(unit.GetCurrentFuel() == 0);
    unit.SetCurrentFuel(unit.GetFuel() + 100);
    CHECK(unit.GetCurrentFuel() == unit.GetFuel());

    unit.SetMovesRemaining(-3);
    CHECK(unit.GetMovesRemaining() == 0);
    unit.SetMovesRemaining(unit.GetMovement() + 100);
    CHECK(unit.GetMovesRemaining() == unit.GetMovement());

    unit.SetXp(-10);
    CHECK(unit.GetXp() == 0);
}
