// Unit current-vs-max stat invariant: a fresh unit starts at its *live* resolved maxima,
// and the current-stat setters clamp to [0, live max] so current can never fall below zero
// or exceed the (effect-adjusted) maximum.

#include "GameFixtures.h"

#include "game/units/Unit.h"
#include "game/units/MovementConstants.h"
#include "game/effects/EffectEnums.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("A fresh unit starts at its live resolved maxima", "[unit][stats]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    CHECK(unit.GetCurrentHp() == unit.GetStat(StatId_t::HitPoints));
    CHECK(unit.GetCurrentFuel() == unit.GetStat(StatId_t::Fuel));
    CHECK(unit.GetMovementPoints() == unit.GetStat(StatId_t::Movement));
    CHECK(unit.GetMoveFragmentsRemaining()
          == unit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint);
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
    unit.SetCurrentHp(unit.GetStat(StatId_t::HitPoints) + 100);
    CHECK(unit.GetCurrentHp() == unit.GetStat(StatId_t::HitPoints));

    unit.SetCurrentFuel(-1);
    CHECK(unit.GetCurrentFuel() == 0);
    unit.SetCurrentFuel(unit.GetStat(StatId_t::Fuel) + 100);
    CHECK(unit.GetCurrentFuel() == unit.GetStat(StatId_t::Fuel));

    unit.SetMoveFragmentsRemaining(-3);
    CHECK(unit.GetMoveFragmentsRemaining() == 0);
    const int maxFragments =
        unit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
    unit.SetMoveFragmentsRemaining(maxFragments + 100);
    CHECK(unit.GetMoveFragmentsRemaining() == maxFragments);

    unit.SetXp(-10);
    CHECK(unit.GetXp() == 0);
}
