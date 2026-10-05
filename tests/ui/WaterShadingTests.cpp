#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/Tile.h"
#include "ui/WaterShading.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <utility>
#include <vector>

using namespace ac;

namespace
{

// Four equal bands from the fixture map's floor up to ocean level, deepest first.
const std::vector<int> k_DepthShades = {30, 20, 10, 0};

// Bottom of band `band` on the fixture map.
int BandFloor_(int band)
{
    const ElevationRulesConfig_t& rRules = actest::TestMapRules();
    const int span = rRules.oceanLevelMeters - rRules.minElevationMeters;
    return rRules.minElevationMeters + span * band / static_cast<int>(k_DepthShades.size());
}

void FillMap_(actest::WorldFixture& rWorld, int elevation)
{
    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            rWorld.map.GetTile(x, y)->SetElevation(elevation);
        }
    }
}

bool SameColor_(const Color_t& a, const Color_t& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

} // namespace

TEST_CASE("A water tile's centre takes the band of its own depth", "[ui][water_shading]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, BandFloor_(3));
    Tile& rTile = *world.map.GetTile(2, 2);

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
        CHECK(ResolveWaterShades(rTile, &world.map, k_DepthShades).center == rCase.shade);
    }
}

TEST_CASE("Without a map every vertex takes the tile's own depth", "[ui][water_shading]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, BandFloor_(3));
    Tile& rTile = *world.map.GetTile(2, 2);
    rTile.SetElevation(BandFloor_(1));

    const DiamondShades_t shades = ResolveWaterShades(rTile, nullptr, k_DepthShades);
    CHECK(shades.center == 20);
    CHECK(shades.west == 20);
    CHECK(shades.north == 20);
    CHECK(shades.east == 20);
    CHECK(shades.south == 20);
}

TEST_CASE("A corner takes the band of the average depth of the tiles that share it",
          "[ui][water_shading]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, BandFloor_(0));
    // The three neighbors that share the N corner with (2, 2).
    for (const auto& [x, y] : {std::pair{1, 2}, std::pair{1, 1}, std::pair{2, 1}})
    {
        world.map.GetTile(x, y)->SetElevation(BandFloor_(3));
    }

    const DiamondShades_t shades =
        ResolveWaterShades(*world.map.GetTile(2, 2), &world.map, k_DepthShades);
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
    actest::WorldFixture world(5, 5);
    FillMap_(world, BandFloor_(0));
    world.map.GetTile(2, 1)->SetElevation(actest::TestMapRules().maxElevationMeters);

    const DiamondShades_t water =
        ResolveWaterShades(*world.map.GetTile(2, 2), &world.map, k_DepthShades);
    // N: the land counts as 4 bands up, not more: (0 + 0 + 0 + 4) / 4 = 1.
    CHECK(water.north == 20);

    const DiamondShades_t land =
        ResolveWaterShades(*world.map.GetTile(2, 1), &world.map, k_DepthShades);
    CHECK(land.center == 0);
}

TEST_CASE("Rows off the map are left out of a corner's average", "[ui][water_shading]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, BandFloor_(0));
    world.map.GetTile(1, 0)->SetElevation(BandFloor_(2));

    const DiamondShades_t shades =
        ResolveWaterShades(*world.map.GetTile(2, 0), &world.map, k_DepthShades);
    // N corner of a top-row tile: only (1, 0) is on the map. (0 + 2) / 2 = 1.
    CHECK(shades.north == 20);
}

TEST_CASE("Corners wrap across the map's x seam", "[ui][water_shading]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, BandFloor_(0));
    world.map.GetTile(4, 2)->SetElevation(BandFloor_(2));
    world.map.GetTile(4, 3)->SetElevation(BandFloor_(2));

    const DiamondShades_t shades =
        ResolveWaterShades(*world.map.GetTile(0, 2), &world.map, k_DepthShades);
    // W corner of (0, 2) is shared with (0, 3), (4, 3) and (4, 2): (0 + 0 + 2 + 2) / 4 = 1.
    CHECK(shades.west == 20);
}

TEST_CASE("An empty shade table is an error", "[ui][water_shading]")
{
    actest::WorldFixture world(5, 5);
    CHECK_THROWS_AS(ResolveWaterShades(*world.map.GetTile(2, 2), &world.map, {}),
                    std::invalid_argument);
    CHECK_THROWS_AS(WaterShadeTint(DiamondShades_t{}, {}), std::invalid_argument);
}

TEST_CASE("Tints follow each vertex's shade, and shades past the table take its last entry",
          "[ui][water_shading]")
{
    const Color_t k_A{250, 250, 250, 255};
    const Color_t k_B{200, 200, 200, 255};
    const Color_t k_C{150, 150, 150, 255};

    const DiamondTint_t tint = WaterShadeTint(DiamondShades_t{0, 1, 2, 3, 9}, {k_A, k_B, k_C});
    CHECK(SameColor_(tint.center, k_A));
    CHECK(SameColor_(tint.west, k_B));
    CHECK(SameColor_(tint.north, k_C));
    CHECK(SameColor_(tint.east, k_C));
    CHECK(SameColor_(tint.south, k_C));
}
