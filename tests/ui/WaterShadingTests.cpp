#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/Tile.h"
#include "ui/WaterShading.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>
#include <utility>
#include <vector>

using namespace ac;

namespace
{

// Four 1000 m detail steps below ocean level, deepest first; the fixture map's floor is the
// first.
WaterShadingStyle_t Shading_(float detailMeters = 1000.0f)
{
    WaterShadingStyle_t shading;
    shading.depthShades = {30, 20, 10, 0};
    shading.detailMeters = detailMeters;
    shading.deepLandform = "Ocean";
    shading.shelfLandform = "OceanShelf";
    shading.coastShades = "OceanShelf";
    shading.deepFromShade = 20;
    return shading;
}
const WaterShadingStyle_t k_Shading = Shading_();

// Bottom of detail step `band`, counted from the deepest.
int BandFloor_(int band)
{
    const int steps = static_cast<int>(k_Shading.depthShades.size());
    return actest::TestMapRules().oceanLevelMeters
           - (steps - band) * static_cast<int>(k_Shading.detailMeters);
}

void FillMap_(actest::WorldFixture& rWorld, int elevation)
{
    for (const auto& pTile : rWorld.map.GetTiles())
    {
        pTile->SetElevation(elevation);
    }
}

} // namespace

TEST_CASE("A water tile's centre takes the detail step of its own depth", "[ui][water_shading]")
{
    actest::WorldFixture world;
    FillMap_(world, BandFloor_(3));
    Tile& rTile = *world.map.GetTile(8, 4);

    const struct
    {
        int elevation;
        int shade;
    } k_Cases[] = {
        {BandFloor_(0), 30},
        {BandFloor_(1) - 1, 30},
        {BandFloor_(1), 20},
        {BandFloor_(2), 10},
        {BandFloor_(3), 0},
        {actest::TestMapRules().oceanLevelMeters - 1, 0},
    };
    for (const auto& rCase : k_Cases)
    {
        CAPTURE(rCase.elevation);
        rTile.SetElevation(rCase.elevation);
        CHECK(ResolveWaterShades(rTile, world.map, k_Shading).center == rCase.shade);
    }
}

TEST_CASE("A corner takes the band of the average depth of the tiles that share it",
          "[ui][water_shading]")
{
    actest::WorldFixture world;
    FillMap_(world, BandFloor_(0));
    // The three neighbors that share the N corner with (8, 4).
    for (const auto& [x, y] : {std::pair{7, 3}, std::pair{8, 2}, std::pair{9, 3}})
    {
        world.map.GetTile(x, y)->SetElevation(BandFloor_(3));
    }

    const DiamondShades_t shades =
        ResolveWaterShades(*world.map.GetTile(8, 4), world.map, k_Shading);
    // N: (0 + 3 + 3 + 3) / 4 bands up = 2.25 → band 2. W and E each share one raised tile
    // (0.75 → band 0); S shares none.
    CHECK(shades.center == 30);
    CHECK(shades.north == 10);
    CHECK(shades.west == 30);
    CHECK(shades.east == 30);
    CHECK(shades.south == 30);
}

TEST_CASE("Land counts as ocean level in a corner's average", "[ui][water_shading]")
{
    actest::WorldFixture world;
    FillMap_(world, BandFloor_(0));
    world.map.GetTile(9, 3)->SetElevation(actest::TestMapRules().maxElevationMeters);

    const DiamondShades_t water =
        ResolveWaterShades(*world.map.GetTile(8, 4), world.map, k_Shading);
    // N: the land counts as 4 bands up, not more: (0 + 0 + 0 + 4) / 4 = 1.
    CHECK(water.north == 20);

    const DiamondShades_t land =
        ResolveWaterShades(*world.map.GetTile(9, 3), world.map, k_Shading);
    CHECK(land.center == 0);
}

TEST_CASE("Rows off the map are left out of a corner's average", "[ui][water_shading]")
{
    actest::WorldFixture world;
    FillMap_(world, BandFloor_(0));
    world.map.GetTile(7, 1)->SetElevation(BandFloor_(3));

    const DiamondShades_t shades =
        ResolveWaterShades(*world.map.GetTile(8, 0), world.map, k_Shading);
    // W corner of a top-row tile: its NW neighbor is off the map, leaving (8, 0), (6, 0) and
    // (7, 1). (0 + 0 + 3) / 3 = 1.
    CHECK(shades.west == 20);
}

TEST_CASE("Corners wrap across the map's x seam", "[ui][water_shading]")
{
    actest::WorldFixture world;
    FillMap_(world, BandFloor_(0));
    world.map.GetTile(-1, 3)->SetElevation(BandFloor_(2));
    world.map.GetTile(-2, 4)->SetElevation(BandFloor_(2));

    const DiamondShades_t shades =
        ResolveWaterShades(*world.map.GetTile(0, 4), world.map, k_Shading);
    // W corner of (0, 4) is shared with its NW, W and SW neighbors across the seam,
    // (-1, 3), (-2, 4) and (-1, 5): (0 + 2 + 2 + 0) / 4 = 1.
    CHECK(shades.west == 20);
}

TEST_CASE("Depth steps count down from ocean level, and deeper water takes the deepest shade",
          "[ui][water_shading]")
{
    actest::WorldFixture world;
    const WaterShadingStyle_t shading = Shading_(500.0f);
    FillMap_(world, -1);
    Tile& rTile = *world.map.GetTile(8, 4);

    const struct
    {
        int elevation;
        int shade;
    } k_Cases[] = {
        {-1, 0},
        {-501, 10},
        {-1001, 20},
        {-2000, 30},
        {actest::TestMapRules().minElevationMeters, 30},
    };
    for (const auto& rCase : k_Cases)
    {
        CAPTURE(rCase.elevation);
        rTile.SetElevation(rCase.elevation);
        CHECK(ResolveWaterShades(rTile, world.map, shading).center == rCase.shade);
    }
}

TEST_CASE("An empty shade table or a non-positive detail step is an error", "[ui][water_shading]")
{
    actest::WorldFixture world;
    WaterShadingStyle_t empty = k_Shading;
    empty.depthShades.clear();
    CHECK_THROWS_AS(ResolveWaterShades(*world.map.GetTile(8, 4), world.map, empty),
                    std::invalid_argument);
    CHECK_THROWS_AS(ResolveWaterShades(*world.map.GetTile(8, 4), world.map, Shading_(0.0f)),
                    std::invalid_argument);
}

TEST_CASE("Sea art turns deep once any corner reaches the deep shade", "[ui][water_shading]")
{
    CHECK(SeaArtLandform(DiamondShades_t{0, 0, 19, 0, 0}, k_Shading) == "OceanShelf");
    CHECK(SeaArtLandform(DiamondShades_t{0, 0, 20, 0, 0}, k_Shading) == "Ocean");
    CHECK(SeaArtLandform(DiamondShades_t{0, 0, 0, 0, 30}, k_Shading) == "Ocean");
    // The centre alone does not switch it.
    CHECK(SeaArtLandform(DiamondShades_t{30, 10, 10, 10, 10}, k_Shading) == "OceanShelf");
}

TEST_CASE("A shade range with a negative cap is an error", "[ui][water_shading]")
{
    TileShape_t shape;
    CHECK_THROWS_AS(ApplyWaterShades(shape, DiamondShades_t{}, WaterShadeRange_t{0, -1}),
                    std::invalid_argument);
}

TEST_CASE("Each vertex takes its depth shade plus the range's offset, kept within the range",
          "[ui][water_shading]")
{
    TileShape_t shape;

    SECTION("a deep art's offset lightens, stopping at as painted")
    {
        ApplyWaterShades(shape, DiamondShades_t{20, 16, 10, 30, 40}, WaterShadeRange_t{-16, 18});
        CHECK(shape.center.shade == 4.0f);
        CHECK(shape.west.shade == 0.0f);
        CHECK(shape.north.shade == 0.0f);
        CHECK(shape.east.shade == 14.0f);
        CHECK(shape.south.shade == 18.0f);
    }

    SECTION("a shallow art keeps its shade up to the cap")
    {
        ApplyWaterShades(shape, DiamondShades_t{0, 5, 15, 16, 37}, WaterShadeRange_t{0, 15});
        CHECK(shape.center.shade == 0.0f);
        CHECK(shape.west.shade == 5.0f);
        CHECK(shape.north.shade == 15.0f);
        CHECK(shape.east.shade == 15.0f);
        CHECK(shape.south.shade == 15.0f);
    }
}

TEST_CASE("Terrain art references name landforms that have a depth shade",
          "[ui][water_shading][art]")
{
    actest::WorldFixture world;
    WaterShadingStyle_t shading = Shading_();

    SECTION("deep, shelf and coast landforms with a depth shade pass")
    {
        CHECK_NOTHROW(ValidateTerrainArtReferences(shading, world.improvements));
    }

    SECTION("an unknown deep landform is rejected")
    {
        shading.deepLandform = "Trench";
        CHECK_THROWS_WITH(ValidateTerrainArtReferences(shading, world.improvements),
                          Catch::Matchers::ContainsSubstring("deep_landform")
                              && Catch::Matchers::ContainsSubstring("Trench"));
    }

    SECTION("an unknown shelf landform is rejected")
    {
        shading.shelfLandform = "Reef";
        CHECK_THROWS_WITH(ValidateTerrainArtReferences(shading, world.improvements),
                          Catch::Matchers::ContainsSubstring("shelf_landform"));
    }

    SECTION("a coast landform without a depth shade is rejected")
    {
        shading.coastShades = "Water";
        CHECK_THROWS_WITH(ValidateTerrainArtReferences(shading, world.improvements),
                          Catch::Matchers::ContainsSubstring("coast_shades")
                              && Catch::Matchers::ContainsSubstring("depth_shade"));
    }

    SECTION("an occupant that is not a landform is rejected")
    {
        shading.deepLandform = "Moist";
        CHECK_THROWS_WITH(ValidateTerrainArtReferences(shading, world.improvements),
                          Catch::Matchers::ContainsSubstring("deep_landform"));
    }
}
