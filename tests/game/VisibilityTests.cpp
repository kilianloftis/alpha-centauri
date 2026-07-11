// Fog of war: Faction owns FactionExploredMap (permanent memory) and FactionVisibleMap
// (current vision), rebuilt from unit Vision and base sight whenever sources change.

#include "GameFixtures.h"

#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("Unit vision reveals a Chebyshev disk including diagonals", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeUnit(faction, 4, 4, {"test_chassis"}); // vision 1

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    const FactionExploredMap& rExplored = faction.GetExploredMap();

    CHECK(rVisible.IsVisible(4, 4));
    CHECK(rExplored.IsExplored(4, 4));
    CHECK(rVisible.IsVisible(5, 4));
    CHECK(rVisible.IsVisible(4, 5));
    CHECK(rVisible.IsVisible(3, 4));
    CHECK(rVisible.IsVisible(4, 3));
    // Vision 1 includes diagonals (Chebyshev distance 1).
    CHECK(rVisible.IsVisible(5, 5));
    CHECK(rVisible.IsVisible(3, 3));
    CHECK(rExplored.IsExplored(5, 5));

    // Chebyshev distance 2 is outside vision 1.
    CHECK_FALSE(rVisible.IsVisible(6, 4));
    CHECK_FALSE(rExplored.IsExplored(6, 4));
    CHECK_FALSE(rVisible.IsVisible(6, 6));
}

TEST_CASE("Moving a unit expands explored memory and updates current visibility",
          "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    const FactionExploredMap& rExplored = faction.GetExploredMap();
    REQUIRE(rExplored.IsExplored(4, 4));
    REQUIRE(rVisible.IsVisible(4, 4));
    REQUIRE_FALSE(rExplored.IsExplored(6, 4));

    fixture.MoveUnit(unit, 5, 4);

    // Old tile stays explored (memory) and remains visible while still in the
    // vision-1 square around the new position.
    CHECK(rExplored.IsExplored(4, 4));
    CHECK(rVisible.IsVisible(4, 4));

    // New frontier is revealed.
    CHECK(rVisible.IsVisible(6, 4));
    CHECK(rExplored.IsExplored(6, 4));

    fixture.MoveUnit(unit, 6, 4);

    // Origin is now outside the Chebyshev vision-1 square and fogged, but still explored.
    CHECK(rExplored.IsExplored(4, 4));
    CHECK_FALSE(rVisible.IsVisible(4, 4));
    CHECK(rVisible.IsVisible(6, 4));
}

TEST_CASE("Destroying a unit drops current visibility but keeps explored tiles",
          "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    const FactionExploredMap& rExplored = faction.GetExploredMap();
    REQUIRE(rVisible.IsVisible(4, 4));
    REQUIRE(rExplored.IsExplored(5, 4));

    faction.GetUnitManager().DestroyUnit(unit);

    CHECK_FALSE(rVisible.IsVisible(4, 4));
    CHECK(rExplored.IsExplored(4, 4));
    CHECK(rExplored.IsExplored(5, 4));
}

TEST_CASE("A base reveals a Chebyshev radius-2 vision square", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 4, 4);

    const FactionVisibleMap& rVisible = faction.GetVisibleMap();
    CHECK(rVisible.IsVisible(4, 4));
    CHECK(rVisible.IsVisible(5, 4));
    CHECK(rVisible.IsVisible(6, 4));
    CHECK(rVisible.IsVisible(6, 6));
    CHECK_FALSE(rVisible.IsVisible(7, 4));
}
