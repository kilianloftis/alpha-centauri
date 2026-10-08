#include "GameFixtures.h"

#include "game/faction/base/BaseManager.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/ImprovementConfigParser.h"

#include <algorithm>
#include <span>
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>

using namespace ac;

TEST_CASE("Fungus in deeper ocean lies dormant until the floor reaches the shelf",
          "[map][fungus]")
{
    actest::WorldFixture world;
    Tile& rTile = *world.map.GetTile(8, 2);
    rTile.SetElevation(actest::TestMapRules().oceanShelfMeters - 1);
    rTile.AddTerrainFeature(world.improvements.Get("Fungus"));
    REQUIRE(rTile.HasFeature("Ocean"));
    const auto activeFungus = [&rTile]() {
        return std::ranges::any_of(rTile.GetTerrainFeatures(), [](const auto* pConfig) {
            return pConfig->id == ImprovementIds::k_Fungus;
        });
    };

    CHECK(rTile.HasTerrainFeature(ImprovementIds::k_Fungus));
    CHECK_FALSE(rTile.HasFeature(ImprovementIds::k_Fungus));
    CHECK_FALSE(activeFungus());

    rTile.SetElevation(actest::TestMapRules().oceanShelfMeters);
    CHECK(rTile.HasFeature(ImprovementIds::k_Fungus));
    CHECK(activeFungus());
}

TEST_CASE("Improvement coexistence is enforced in both directions", "[map][improvements]")
{
    actest::WorldFixture world;
    Tile& rTile = *world.map.GetTile(8, 4);
    rTile.SetElevation(500);

    const ImprovementConfig_t& rMonolith = world.improvements.Get("Monolith");
    const ImprovementConfig_t& rNutrients = world.improvements.Get("Nutrients");

    // Monolith excludes @resource_bonus; Nutrients (tagged resource_bonus) declares no excludes
    // of its own. Only the Monolith side of the pair is written down.
    REQUIRE(rNutrients.excludes.empty());

    CHECK(CanBuildImprovement(rTile, rMonolith));
    CHECK(CanBuildImprovement(rTile, rNutrients));

    rTile.AddTerrainFeature(rMonolith);

    // Forward direction: Monolith excludes itself.
    CHECK_FALSE(CanBuildImprovement(rTile, rMonolith));
    // Reverse direction: the incumbent's excludes bind the candidate too. This one used to pass.
    CHECK_FALSE(CanBuildImprovement(rTile, rNutrients));
}

TEST_CASE("An intrinsic terrain feature excludes a candidate that does not exclude it back",
          "[map][improvements]")
{
    actest::WorldFixture world;
    Tile& rTile = *world.map.GetTile(8, 4);
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
    actest::WorldFixture world;
    Tile& rTile = *world.map.GetTile(8, 4);
    rTile.SetElevation(500);
    rTile.AddTerrainFeature(world.improvements.Get("Fungus"));

    const ImprovementConfig_t& rForest = world.improvements.Get("Forest");
    CHECK_FALSE(CanBuildImprovement(rTile, rForest));

    // Naming an occupant as ignored does not remove it. Forest spread does not use this.
    const std::string fungusLeaving(ImprovementIds::k_Fungus);
    CHECK(CanBuildImprovement(rTile, rForest, std::span(&fungusLeaving, 1)));
    CHECK(rTile.HasFeature("Fungus"));
}

TEST_CASE("WorldMap rejects non-positive dimensions", "[map]")
{
    // A zero-sized map has no valid tile, GetTile always returns null, and every generation
    // stage no-ops on it - an empty world rather than a diagnostic.
    CHECK_THROWS_AS(WorldMap(0, 10, actest::TestMapRules()), std::invalid_argument);
    CHECK_THROWS_AS(WorldMap(10, 0, actest::TestMapRules()), std::invalid_argument);
    CHECK_THROWS_AS(WorldMap(-4, -4, actest::TestMapRules()), std::invalid_argument);
    CHECK_NOTHROW(WorldMap(2, 1, actest::TestMapRules()));
}

TEST_CASE("TerritoryMap::Rebuild refuses to run against a mismatched grid", "[map][territory]")
{
    actest::WorldFixture world;

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
        CHECK_THROWS_WITH(
            territory.Rebuild(world.map, {}),
            Catch::Matchers::ContainsSubstring("4x4")
                && Catch::Matchers::ContainsSubstring(std::to_string(world.map.GetWidth()) + "x"
                                                      + std::to_string(world.map.GetHeight())));
    }

    SECTION("matching dimensions rebuild normally")
    {
        TerritoryMap territory;
        territory.Reset(world.map.GetWidth(), world.map.GetHeight());
        CHECK_NOTHROW(territory.Rebuild(world.map, {}));
    }
}
