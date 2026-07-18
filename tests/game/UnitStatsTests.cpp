// Unit current-vs-max stat invariant: a fresh unit starts at its *live* resolved maxima,
// and the current-stat setters clamp to [0, live max] so current can never fall below zero
// or exceed the (effect-adjusted) maximum.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "game/units/MovementConstants.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"

#include <catch2/catch_test_macros.hpp>
#include <unordered_map>
#include <vector>

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

TEST_CASE("ResolveAdditiveStat sums component Adds and ignores percent modifiers", "[unit][stats]")
{
    actest::EffectPool pool;

    UnitComponentConfig_t weapon;
    weapon.id = "laser";
    weapon.type = "weapon";
    weapon.effects = {
        pool.StatMod(StatId_t::Attack, 2.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
        pool.StatMod(StatId_t::Attack, 50.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit),
    };

    UnitComponentConfig_t armor;
    armor.id = "synthmetal";
    armor.type = "armor";
    armor.effects = {
        pool.StatMod(StatId_t::Defense, 2.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
        // Extra attack points from armor/ability still count toward the base rating.
        pool.StatMod(StatId_t::Attack, 1.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
    };

    UnitComponentConfig_t chassis;
    chassis.id = "speeder";
    chassis.type = "chassis";
    chassis.domain = UnitDomain_t::Land;
    chassis.effects = {
        pool.StatMod(StatId_t::Movement, 2.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
    };

    const std::vector<UnitSlotConfig_t> slots = {
        {.id = "weapon", .displayName = "Weapon", .componentType = "weapon", .required = true},
        {.id = "armor", .displayName = "Armor", .componentType = "armor", .required = true},
        {.id = "chassis", .displayName = "Chassis", .componentType = "chassis", .required = true},
    };
    const std::unordered_map<std::string, const UnitComponentConfig_t*> components = {
        {"weapon", &weapon},
        {"armor", &armor},
        {"chassis", &chassis},
    };
    const UnitDesign design(slots, components);

    // Base combat rating: laser 2 + armor's +1 attack, ignoring the +50%.
    CHECK(ResolveAdditiveStat(design, StatId_t::Attack) == 3);
    CHECK(ResolveAdditiveStat(design, StatId_t::Defense) == 2);
    CHECK(ResolveAdditiveStat(design, StatId_t::Movement) == 2);
    // Full resolve still applies percent: (2+1)*1.5 = 4.
    CHECK(ResolveStat(design, StatId_t::Attack) == 4);
}
