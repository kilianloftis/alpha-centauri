// Fog of war: FactionVisibilityMap tracks per-faction explored memory and currently-
// visible tiles, rebuilt from unit Vision and base sight whenever vision sources change.

#include "GameFixtures.h"

#include "game/faction/FactionVisibilityMap.h"
#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("Unit vision reveals a Chebyshev disk including diagonals", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeUnit(faction, 4, 4, {"test_chassis"}); // vision 1

    const FactionVisibilityMap& rVis = faction.GetVisibility();

    CHECK(rVis.IsVisible(4, 4));
    CHECK(rVis.IsExplored(4, 4));
    CHECK(rVis.IsVisible(5, 4));
    CHECK(rVis.IsVisible(4, 5));
    CHECK(rVis.IsVisible(3, 4));
    CHECK(rVis.IsVisible(4, 3));
    // Vision 1 includes diagonals (Chebyshev distance 1).
    CHECK(rVis.IsVisible(5, 5));
    CHECK(rVis.IsVisible(3, 3));
    CHECK(rVis.IsExplored(5, 5));

    // Chebyshev distance 2 is outside vision 1.
    CHECK_FALSE(rVis.IsVisible(6, 4));
    CHECK_FALSE(rVis.IsExplored(6, 4));
    CHECK_FALSE(rVis.IsVisible(6, 6));
}

TEST_CASE("Moving a unit expands explored memory and updates current visibility",
          "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    const FactionVisibilityMap& rVis = faction.GetVisibility();
    REQUIRE(rVis.IsExplored(4, 4));
    REQUIRE(rVis.IsVisible(4, 4));
    REQUIRE_FALSE(rVis.IsExplored(6, 4));

    fixture.MoveUnit(unit, 5, 4);

    // Old tile stays explored (memory) and remains visible while still in the
    // vision-1 square around the new position.
    CHECK(rVis.IsExplored(4, 4));
    CHECK(rVis.IsVisible(4, 4));

    // New frontier is revealed.
    CHECK(rVis.IsVisible(6, 4));
    CHECK(rVis.IsExplored(6, 4));

    fixture.MoveUnit(unit, 6, 4);

    // Origin is now outside the Chebyshev vision-1 square and fogged, but still explored.
    CHECK(rVis.IsExplored(4, 4));
    CHECK_FALSE(rVis.IsVisible(4, 4));
    CHECK(rVis.IsVisible(6, 4));
}

TEST_CASE("Destroying a unit drops current visibility but keeps explored tiles",
          "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});

    const FactionVisibilityMap& rVis = faction.GetVisibility();
    REQUIRE(rVis.IsVisible(4, 4));
    REQUIRE(rVis.IsExplored(5, 4));

    faction.GetUnitManager().DestroyUnit(unit);

    CHECK_FALSE(rVis.IsVisible(4, 4));
    CHECK(rVis.IsExplored(4, 4));
    CHECK(rVis.IsExplored(5, 4));
}

TEST_CASE("A base reveals a Chebyshev radius-2 vision square", "[visibility][fog]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 4, 4);

    const FactionVisibilityMap& rVis = faction.GetVisibility();
    CHECK(rVis.IsVisible(4, 4));
    CHECK(rVis.IsVisible(5, 4));
    CHECK(rVis.IsVisible(6, 4));
    CHECK(rVis.IsVisible(6, 6));
    CHECK_FALSE(rVis.IsVisible(7, 4));
}
