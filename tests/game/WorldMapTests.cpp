#include "TestHelpers.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/TileFlagMap.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace ac;

TEST_CASE("A WorldMap holds width * height / 2 tiles, one per even-parity coordinate",
          "[map][worldmap]")
{
    const WorldMap map(8, 5, actest::TestMapRules());

    CHECK(map.GetWidth() == 8);
    CHECK(map.GetHeight() == 5);
    CHECK(map.GetTiles().size() == 20);

    for (const auto& pTile : map.GetTiles())
    {
        CHECK(((pTile->GetX() + pTile->GetY()) & 1) == 0);
        CHECK(pTile->GetX() >= 0);
        CHECK(pTile->GetX() < map.GetWidth());
        CHECK(pTile->GetY() >= 0);
        CHECK(pTile->GetY() < map.GetHeight());
    }
}

TEST_CASE("WorldMap tiles are stored row-major, north row first", "[map][worldmap]")
{
    const WorldMap map(8, 4, actest::TestMapRules());
    const auto tiles = map.GetTiles();
    const int tilesPerRow = map.GetWidth() / 2;

    SECTION("even rows start at x = 0 and odd rows at x = 1")
    {
        for (int y = 0; y < map.GetHeight(); ++y)
        {
            for (int column = 0; column < tilesPerRow; ++column)
            {
                const Tile& rTile = *tiles[static_cast<size_t>(y * tilesPerRow + column)];
                CHECK(rTile.GetY() == y);
                CHECK(rTile.GetX() == (y & 1) + 2 * column);
            }
        }
    }

    SECTION("GetTileIndex is the tile's position in GetTiles")
    {
        for (size_t i = 0; i < tiles.size(); ++i)
        {
            CHECK(static_cast<size_t>(map.GetTileIndex(*tiles[i])) == i);
            CHECK(map.GetTile(tiles[i]->GetX(), tiles[i]->GetY()) == tiles[i].get());
        }
    }
}

TEST_CASE("WorldMap wraps x at the width and not y", "[map][worldmap]")
{
    WorldMap map(8, 4, actest::TestMapRules());

    SECTION("a whole period east or west is the same tile")
    {
        CHECK(map.GetTile(2, 2) == map.GetTile(2 + 8, 2));
        CHECK(map.GetTile(2, 2) == map.GetTile(2 - 8, 2));
        CHECK(map.GetTile(3, 1) == map.GetTile(3 + 16, 1));
    }

    SECTION("one step west of the first column is the last column")
    {
        const Tile* pFirstEven = map.GetTile(0, 0);
        const Tile* pLastEven = map.GetTile(6, 0);
        REQUIRE(pFirstEven);
        REQUIRE(pLastEven);
        CHECK(map.GetTile(-2, 0) == pLastEven);
        CHECK(map.GetTile(8, 0) == pFirstEven);

        const Tile* pFirstOdd = map.GetTile(1, 1);
        const Tile* pLastOdd = map.GetTile(7, 1);
        REQUIRE(pFirstOdd);
        REQUIRE(pLastOdd);
        CHECK(map.GetTile(-1, 1) == pLastOdd);
        CHECK(map.GetTile(9, 1) == pFirstOdd);
    }

    SECTION("rows above the first and below the last are null")
    {
        CHECK(map.GetTile(0, -2) == nullptr);
        CHECK(map.GetTile(1, -1) == nullptr);
        CHECK(map.GetTile(0, 4) == nullptr);
        CHECK(map.GetTile(1, 5) == nullptr);
    }
}

TEST_CASE("WorldMap rejects odd-parity addresses and odd widths", "[map][worldmap]")
{
    WorldMap map(8, 4, actest::TestMapRules());

    CHECK_THROWS_AS(map.GetTile(1, 0), std::invalid_argument);
    CHECK_THROWS_AS(map.GetTile(0, 1), std::invalid_argument);
    CHECK_THROWS_AS(map.GetTile(-1, 2), std::invalid_argument);
    CHECK_THROWS_AS(map.GetTileIndex(1, 0), std::invalid_argument);

    const WorldMap& rConstMap = map;
    CHECK_THROWS_AS(rConstMap.GetTile(1, 2), std::invalid_argument);

    CHECK_THROWS_AS(WorldMap(7, 4, actest::TestMapRules()), std::invalid_argument);
    CHECK_THROWS_AS(WorldMap(1, 1, actest::TestMapRules()), std::invalid_argument);
    CHECK_NOTHROW(WorldMap(8, 3, actest::TestMapRules()));
}

TEST_CASE("TerritoryMap and TileFlagMap hold one entry per tile and reject odd parity",
          "[map][worldmap]")
{
    const WorldMap map(8, 4, actest::TestMapRules());

    SECTION("territory starts unowned and rejects odd-parity reads")
    {
        CHECK(map.GetTerritory().GetWidth() == 8);
        CHECK(map.GetTerritory().GetHeight() == 4);
        for (const auto& pTile : map.GetTiles())
        {
            CHECK_FALSE(map.GetTerritory().HasOwner(*pTile));
        }
        CHECK_THROWS_AS(map.GetTerritory().HasOwner(1, 0), std::invalid_argument);
    }

    SECTION("a flag lands on its own tile only")
    {
        TileFlagMap flags;
        flags.Reset(map.GetWidth(), map.GetHeight());
        flags.Set(2, 0);

        for (const auto& pTile : map.GetTiles())
        {
            const bool bIsMarked = pTile->GetX() == 2 && pTile->GetY() == 0;
            CHECK(flags.Test(*pTile) == bIsMarked);
        }
        CHECK_THROWS_AS(flags.Test(1, 0), std::invalid_argument);
        CHECK_THROWS_AS(flags.Set(0, 1), std::invalid_argument);
    }
}
