// Fog of war: a faction remembers each tile's terrain and improvements as it last saw them.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/faction/FactionTileMemory.h"
#include "game/faction/VisibilityRules.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

using namespace ac;

namespace
{

bool Remembers_(const Faction& rFaction, const Tile& rTile, const ImprovementConfig_t& rConfig)
{
    const TileOccupants_t occupants = rFaction.GetTileMemory().Occupants(rTile);
    return std::ranges::find(occupants.improvements, &rConfig) != occupants.improvements.end();
}

} // namespace

TEST_CASE("Recording a tile copies its current occupant lists", "[visibility][fog][memory]")
{
    actest::FactionFixture fixture;
    const ImprovementConfig_t& rMine = fixture.improvements.Get("Mine");
    Tile& rTile = fixture.At(8, 8);
    FactionTileMemory memory;
    memory.Reset(fixture.map.GetWidth(), fixture.map.GetHeight());

    CHECK(memory.Occupants(rTile).improvements.empty());

    rTile.AddImprovement(rMine);
    memory.Record(rTile);
    REQUIRE(memory.Occupants(rTile).improvements.size() == 1);
    CHECK(memory.Occupants(rTile).improvements.front() == &rMine);
    CHECK(memory.Occupants(rTile).terrain.size() == rTile.GetTerrainFeatures().size());

    rTile.RemoveImprovement(rMine.id);
    CHECK(memory.Occupants(rTile).improvements.size() == 1);
}

TEST_CASE("The memory revision changes only when a recorded tile changes",
          "[visibility][fog][memory]")
{
    actest::FactionFixture fixture;
    Tile& rTile = fixture.At(8, 8);
    FactionTileMemory memory;
    memory.Reset(fixture.map.GetWidth(), fixture.map.GetHeight());
    memory.Record(rTile);

    const uint64_t revision = memory.GetRevision();
    memory.Record(rTile);
    CHECK(memory.GetRevision() == revision);

    rTile.AddImprovement(fixture.improvements.Get("Mine"));
    memory.Record(rTile);
    CHECK(memory.GetRevision() != revision);
}

TEST_CASE("Using the memory before it is sized is an error", "[visibility][fog][memory]")
{
    actest::WorldFixture world;
    FactionTileMemory memory;
    CHECK_THROWS_AS(memory.Record(world.At(8, 8)), std::logic_error);
    CHECK_THROWS_AS(memory.Occupants(world.At(8, 8)), std::logic_error);
}

TEST_CASE("A tile in sight is recorded as it now looks", "[visibility][fog][memory]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    const ImprovementConfig_t& rMine = fixture.improvements.Get("Mine");
    fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    Tile& rTile = fixture.At(9, 9);
    REQUIRE(faction.GetVisibleMap().IsVisible(rTile));

    rTile.AddImprovement(rMine);
    faction.RebuildVisibility();

    CHECK(Remembers_(faction, rTile, rMine));
}

TEST_CASE("A tile keeps what it showed when it left sight", "[visibility][fog][memory]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    const ImprovementConfig_t& rMine = fixture.improvements.Get("Mine");
    Unit& rUnit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    Tile& rTile = fixture.At(8, 8);

    SECTION("an improvement built while in sight is remembered once the tile is left")
    {
        rTile.AddImprovement(rMine);
        fixture.MoveUnit(rUnit, 12, 12);
        REQUIRE_FALSE(faction.GetVisibleMap().IsVisible(rTile));

        CHECK(Remembers_(faction, rTile, rMine));
    }

    SECTION("an improvement added out of sight is not remembered until the tile is seen again")
    {
        fixture.MoveUnit(rUnit, 12, 12);
        REQUIRE_FALSE(faction.GetVisibleMap().IsVisible(rTile));

        rTile.AddImprovement(rMine);
        fixture.MoveUnit(rUnit, 14, 14);
        CHECK_FALSE(Remembers_(faction, rTile, rMine));

        fixture.MoveUnit(rUnit, 9, 9);
        REQUIRE(faction.GetVisibleMap().IsVisible(rTile));
        CHECK(Remembers_(faction, rTile, rMine));
    }

    SECTION("an improvement removed out of sight stays remembered")
    {
        rTile.AddImprovement(rMine);
        fixture.MoveUnit(rUnit, 12, 12);
        REQUIRE_FALSE(faction.GetVisibleMap().IsVisible(rTile));

        rTile.RemoveImprovement(rMine.id);
        fixture.MoveUnit(rUnit, 14, 14);
        CHECK(Remembers_(faction, rTile, rMine));
    }
}

TEST_CASE("Removing the shroud records the tiles it explores and leaves the rest",
          "[visibility][fog][memory][shroud]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    const ImprovementConfig_t& rMine = fixture.improvements.Get("Mine");
    Unit& rUnit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    Tile& rLeft = fixture.At(8, 8);
    Tile& rFar = fixture.At(8, 0);
    fixture.MoveUnit(rUnit, 12, 12);
    REQUIRE_FALSE(faction.GetVisibleMap().IsVisible(rLeft));
    REQUIRE(faction.GetExploredMap().IsExplored(rLeft));
    REQUIRE_FALSE(faction.GetExploredMap().IsExplored(rFar));

    rLeft.AddImprovement(rMine);
    rFar.AddImprovement(rMine);
    ApplyRemoveShroud(faction);

    CHECK(Remembers_(faction, rFar, rMine));
    CHECK_FALSE(Remembers_(faction, rLeft, rMine));
}
