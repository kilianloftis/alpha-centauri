#include "GameFixtures.h"

#include "game/faction/base/BaseManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/TileLayer.h"
#include "game/map/TileLayerResolver.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>

using namespace ac;

TEST_CASE("Tile layers resolve improvements by config id, not by sprite content id",
          "[map][layers]")
{
    // The probe used TileLayerContent ("farm"/"forest"/"road"), which is the sprite domain;
    // improvements.json declares "Farm"/"Forest"/"Road". Every probe failed, so Vegetation and
    // Road stayed empty and their improvements fell through into the Improvement layer.
    actest::WorldFixture world(5, 5);
    Tile& rTile = *world.map.GetTile(2, 2);
    rTile.SetElevation(500);
    rTile.AddImprovement(world.improvements.Get("Farm"));
    rTile.AddImprovement(world.improvements.Get("Road"));

    const auto layers = ResolveTileLayers(rTile);
    const auto& rVegetation = layers[static_cast<size_t>(TileLayerType_t::Vegetation)];
    const auto& rRoad = layers[static_cast<size_t>(TileLayerType_t::Road)];
    const auto& rImprovement = layers[static_cast<size_t>(TileLayerType_t::Improvement)];

    // Populated, and returning the lowercase sprite ids the renderer keys on.
    REQUIRE(rVegetation.contentId.has_value());
    CHECK(*rVegetation.contentId == TileLayerContent::k_Farm);
    REQUIRE(rRoad.contentId.has_value());
    CHECK(*rRoad.contentId == TileLayerContent::k_Road);

    // ...and neither leaked into the Improvement layer.
    CHECK_FALSE(rImprovement.contentId.has_value());
}

TEST_CASE("Improvement coexistence is enforced in both directions", "[map][improvements]")
{
    actest::WorldFixture world(5, 5);
    Tile& rTile = *world.map.GetTile(2, 2);
    rTile.SetElevation(500);

    const ImprovementConfig_t& rMonolith = world.improvements.Get("Monolith");
    const ImprovementConfig_t& rNutrients = world.improvements.Get("Nutrients");

    // Monolith excludes @resource_bonus; Nutrients (tagged resource_bonus) declares no excludes
    // of its own. Only the Monolith side of the pair is written down.
    REQUIRE(rNutrients.excludes.empty());

    CHECK(CanBuildImprovement(rTile, rMonolith));
    CHECK(CanBuildImprovement(rTile, rNutrients));

    rTile.AddImprovement(rMonolith);

    // Forward direction: Monolith excludes itself.
    CHECK_FALSE(CanBuildImprovement(rTile, rMonolith));
    // Reverse direction: the incumbent's excludes bind the candidate too. This one used to pass.
    CHECK_FALSE(CanBuildImprovement(rTile, rNutrients));
}

TEST_CASE("An intrinsic terrain feature excludes a candidate that does not exclude it back",
          "[map][improvements]")
{
    actest::WorldFixture world(5, 5);
    Tile& rTile = *world.map.GetTile(2, 2);
    rTile.SetElevation(500);

    const ImprovementConfig_t& rMine = world.improvements.Get("Mine");
    REQUIRE(rMine.excludes.empty());
    CHECK(CanBuildImprovement(rTile, rMine));

    // River is a TerrainFeature_t, mirrored into the tile's terrain-feature configs rather
    // than its improvements - the reverse check has to look at both collections.
    rTile.SetHasRiver(true);
    REQUIRE(rTile.HasFeature("River"));
    CHECK_FALSE(CanBuildImprovement(rTile, rMine));
}

TEST_CASE("A feature the caller is clearing does not block the placement", "[map][improvements]")
{
    actest::WorldFixture world(5, 5);
    Tile& rTile = *world.map.GetTile(2, 2);
    rTile.SetElevation(500);
    rTile.SetHasFungus(true);

    const ImprovementConfig_t& rForest = world.improvements.Get("Forest");
    CHECK_FALSE(CanBuildImprovement(rTile, rForest));

    // Forest spread wipes fungus, so it names it - and the tile is not mutated to say so.
    CHECK(CanBuildImprovement(rTile, rForest, ImprovementIds::k_Fungus));
    CHECK(rTile.GetHasFungus());
}

TEST_CASE("WorldMap rejects non-positive dimensions", "[map]")
{
    // A zero-sized map has no valid tile, GetTile always returns null, and every generation
    // stage no-ops on it - an empty world rather than a diagnostic.
    CHECK_THROWS_AS(WorldMap(0, 10), std::invalid_argument);
    CHECK_THROWS_AS(WorldMap(10, 0), std::invalid_argument);
    CHECK_THROWS_AS(WorldMap(-4, -4), std::invalid_argument);
    CHECK_NOTHROW(WorldMap(1, 1));
}

TEST_CASE("TerritoryMap::Rebuild refuses to run against a mismatched grid", "[map][territory]")
{
    actest::WorldFixture world(9, 9);

    SECTION("unsized")
    {
        TerritoryMap territory;
        REQUIRE_FALSE(territory.IsSized());
        // Returning silently left callers reading k_NoFactionOwner as current ownership.
        CHECK_THROWS_AS(territory.Rebuild(world.map, {}), std::logic_error);
    }

    SECTION("sized to different dimensions than the world")
    {
        TerritoryMap territory;
        territory.Reset(4, 4);
        CHECK_THROWS_WITH(territory.Rebuild(world.map, {}),
                          Catch::Matchers::ContainsSubstring("4x4")
                              && Catch::Matchers::ContainsSubstring("9x9"));
    }

    SECTION("matching dimensions rebuild normally")
    {
        TerritoryMap territory;
        territory.Reset(9, 9);
        CHECK_NOTHROW(territory.Rebuild(world.map, {}));
    }
}
