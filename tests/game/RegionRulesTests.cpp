#include "TestHelpers.h"

#include "game/map/RegionCache.h"
#include "game/map/RegionRules.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace ac;

namespace
{

constexpr int k_Width = 16;
constexpr int k_Height = 7;

void Flood_(WorldMap& rMap)
{
    for (const auto& pTile : rMap.GetTiles())
    {
        pTile->SetElevation(-100);
    }
}

void Raise_(WorldMap& rMap, int x, int y)
{
    rMap.GetTile(x, y)->SetElevation(100);
}

int RegionAt_(const std::vector<int>& rRegions, const WorldMap& rMap, int x, int y)
{
    return rRegions.at(rMap.GetTileIndex(x, y));
}

} // namespace

TEST_CASE("Separate islands are separate regions and the sea around them is one",
          "[map][regions]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules(), Rainfall_t::Average);
    Flood_(map);
    Raise_(map, 4, 2);
    Raise_(map, 5, 3);
    Raise_(map, 10, 2);

    const std::vector<int> regions = AssignRegions(map);
    const int islandA = RegionAt_(regions, map, 4, 2);
    const int islandB = RegionAt_(regions, map, 10, 2);
    const int sea = RegionAt_(regions, map, 8, 2);
    CHECK(RegionAt_(regions, map, 5, 3) == islandA);
    CHECK(islandA != islandB);
    CHECK(RegionAt_(regions, map, 8, 4) == sea);
    CHECK(RegionAt_(regions, map, 14, 4) == sea);
    CHECK(sea != islandA);
    CHECK(sea != islandB);
}

TEST_CASE("Land touching only at a corner is one region", "[map][regions]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules(), Rainfall_t::Average);
    Flood_(map);
    Raise_(map, 4, 2);
    Raise_(map, 6, 2);

    const std::vector<int> regions = AssignRegions(map);
    CHECK(RegionAt_(regions, map, 4, 2) == RegionAt_(regions, map, 6, 2));
}

TEST_CASE("Each pole takes one region per surface", "[map][regions]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules(), Rainfall_t::Average);
    map.GetTile(4, 0)->SetElevation(-100);
    map.GetTile(8, 0)->SetElevation(-100);
    map.GetTile(4, 6)->SetElevation(-100);

    const std::vector<int> regions = AssignRegions(map);
    const int northLand = RegionAt_(regions, map, 0, 0);
    const int northWater = RegionAt_(regions, map, 4, 0);
    const int southLand = RegionAt_(regions, map, 2, 6);
    const int southWater = RegionAt_(regions, map, 4, 6);
    CHECK(RegionAt_(regions, map, 6, 0) == northLand);
    CHECK(RegionAt_(regions, map, 8, 0) == northWater);
    CHECK(RegionAt_(regions, map, 10, 6) == southLand);
    CHECK(northLand != northWater);
    CHECK(northLand != southLand);
    CHECK(northWater != southWater);
    CHECK(southLand != southWater);
    CHECK(RegionAt_(regions, map, 1, 1) != northLand);
}

TEST_CASE("Land joined only through the first row stays two regions", "[map][regions]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules(), Rainfall_t::Average);
    Flood_(map);
    for (int x = 2; x <= 8; x += 2)
    {
        Raise_(map, x, 0);
    }
    Raise_(map, 3, 1);
    Raise_(map, 7, 1);

    const std::vector<int> regions = AssignRegions(map);
    CHECK(RegionAt_(regions, map, 3, 1) != RegionAt_(regions, map, 7, 1));
}

TEST_CASE("The region cache follows a tile that turns from water to land", "[map][regions]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules(), Rainfall_t::Average);
    Flood_(map);
    Raise_(map, 4, 2);
    Raise_(map, 8, 2);
    const RegionCache& rRegions = map.GetRegions();
    const Tile& rWest = *map.GetTile(4, 2);
    const Tile& rEast = *map.GetTile(8, 2);
    REQUIRE(rRegions.RegionOf(rWest) != rRegions.RegionOf(rEast));

    Raise_(map, 6, 2);

    CHECK(rRegions.RegionOf(rWest) == rRegions.RegionOf(rEast));
}

TEST_CASE("RegionCache TileCountInRegion matches flood sizes for two ponds", "[map][regions]")
{
    WorldMap map(k_Width, k_Height, actest::TestMapRules(), Rainfall_t::Average);
    for (const auto& pTile : map.GetTiles())
    {
        pTile->SetElevation(100);
    }
    // Two inland ponds separated by land (8-neighbour flood). Even-parity cells only.
    map.GetTile(4, 2)->SetElevation(-100);
    map.GetTile(5, 3)->SetElevation(-100);
    map.GetTile(6, 2)->SetElevation(-100);

    map.GetTile(10, 2)->SetElevation(-100);
    map.GetTile(11, 3)->SetElevation(-100);
    map.GetTile(12, 2)->SetElevation(-100);
    map.GetTile(12, 4)->SetElevation(-100);
    map.GetTile(11, 1)->SetElevation(-100);

    const RegionCache& rRegions = map.GetRegions();
    CHECK(rRegions.TileCountInRegion(*map.GetTile(4, 2)) == 3);
    CHECK(rRegions.TileCountInRegion(*map.GetTile(6, 2)) == 3);
    CHECK(rRegions.TileCountInRegion(*map.GetTile(10, 2)) == 5);
    CHECK(rRegions.TileCountInRegion(*map.GetTile(12, 2)) == 5);
    CHECK(rRegions.RegionOf(*map.GetTile(4, 2)) != rRegions.RegionOf(*map.GetTile(10, 2)));
}
