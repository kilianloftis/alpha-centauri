#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "ui/TileAutotile.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <set>
#include <utility>

using namespace ac;

namespace
{

// Neighbors marked by elevation: land at 500 m matches, everything else does not.
bool IsMarked_(const Tile& rNeighbor)
{
    return rNeighbor.GetElevation() == 500;
}

Tile& Centre_(actest::WorldFixture& rWorld)
{
    return *rWorld.map.GetTile(8, 8);
}

Tile& Offset_(actest::WorldFixture& rWorld, int p, int q)
{
    return *GetTileAtLatticeOffset(rWorld.map, Centre_(rWorld), p, q);
}

void Mark_(actest::WorldFixture& rWorld, int p, int q)
{
    Offset_(rWorld, p, q).SetElevation(500);
}

std::uint8_t Mask_(actest::WorldFixture& rWorld, SpriteTileLayout_t layout, const Tile& rTile)
{
    return ResolveTileMask(layout, rTile, rWorld.map, IsMarked_);
}

std::uint8_t Mask_(actest::WorldFixture& rWorld, SpriteTileLayout_t layout)
{
    return Mask_(rWorld, layout, Centre_(rWorld));
}

} // namespace

TEST_CASE("A tile with no matching neighbor has mask 0", "[ui][autotile]")
{
    actest::WorldFixture world;
    CHECK(Mask_(world, SpriteTileLayout_t::Edges) == 0);
    CHECK(Mask_(world, SpriteTileLayout_t::Blob) == 0);
}

TEST_CASE("Each edge neighbor sets its own bit", "[ui][autotile]")
{
    // (lattice offset, edge-layout bit, blob-layout bit): the diamond's NE, SE, SW, NW edges.
    const struct
    {
        int p;
        int q;
        std::uint8_t edgeBit;
        std::uint8_t blobBit;
    } k_Cases[] = {
        {0, -1, 1, 1u << 1},
        {1, 0, 2, 1u << 3},
        {0, 1, 4, 1u << 5},
        {-1, 0, 8, 1u << 7},
    };
    for (const auto& rCase : k_Cases)
    {
        CAPTURE(rCase.p, rCase.q);
        actest::WorldFixture world;
        Mark_(world, rCase.p, rCase.q);
        CHECK(Mask_(world, SpriteTileLayout_t::Edges) == rCase.edgeBit);
        CHECK(Mask_(world, SpriteTileLayout_t::Blob) == rCase.blobBit);
    }
}

TEST_CASE("Corner neighbors count only between two matching edges", "[ui][autotile]")
{
    // Corners clockwise from N: (lattice offset, corner bit, the two edge offsets beside it).
    const struct
    {
        int p;
        int q;
        std::uint8_t cornerBit;
        std::pair<int, int> edgeBefore;
        std::pair<int, int> edgeAfter;
    } k_Corners[] = {
        {-1, -1, 1u << 0, {-1, 0}, {0, -1}},
        {1, -1, 1u << 2, {0, -1}, {1, 0}},
        {1, 1, 1u << 4, {1, 0}, {0, 1}},
        {-1, 1, 1u << 6, {0, 1}, {-1, 0}},
    };
    for (const auto& rCorner : k_Corners)
    {
        CAPTURE(rCorner.p, rCorner.q);
        actest::WorldFixture world;
        Mark_(world, rCorner.p, rCorner.q);
        CHECK(Mask_(world, SpriteTileLayout_t::Blob) == 0);
        CHECK(Mask_(world, SpriteTileLayout_t::Edges) == 0);

        Mark_(world, rCorner.edgeBefore.first, rCorner.edgeBefore.second);
        CHECK((Mask_(world, SpriteTileLayout_t::Blob) & rCorner.cornerBit) == 0);

        Mark_(world, rCorner.edgeAfter.first, rCorner.edgeAfter.second);
        CHECK((Mask_(world, SpriteTileLayout_t::Blob) & rCorner.cornerBit) != 0);
    }
}

TEST_CASE("Every neighbor matching gives the full masks", "[ui][autotile]")
{
    actest::WorldFixture world;
    for (int q = -1; q <= 1; ++q)
    {
        for (int p = -1; p <= 1; ++p)
        {
            if (p != 0 || q != 0)
            {
                Mark_(world, p, q);
            }
        }
    }
    CHECK(Mask_(world, SpriteTileLayout_t::Edges) == 15);
    CHECK(Mask_(world, SpriteTileLayout_t::Blob) == 255);
}

TEST_CASE("Blob masks reduce to 47 cases", "[ui][autotile]")
{
    actest::WorldFixture world;
    const int k_Deltas[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}};
    std::set<std::uint8_t> masks;
    for (unsigned neighbors = 0; neighbors < 256; ++neighbors)
    {
        for (unsigned bit = 0; bit < 8; ++bit)
        {
            Offset_(world, k_Deltas[bit][0], k_Deltas[bit][1])
                .SetElevation((neighbors >> bit & 1u) ? 500 : 0);
        }
        masks.insert(Mask_(world, SpriteTileLayout_t::Blob));
    }
    CHECK(masks.size() == 47);
}

TEST_CASE("Tile masks wrap in x and stop at the map's top and bottom", "[ui][autotile]")
{
    SECTION("x wraps")
    {
        actest::WorldFixture west(8, 5);
        west.map.GetTile(7, 1)->SetElevation(500);
        CHECK(Mask_(west, SpriteTileLayout_t::Edges, *west.map.GetTile(0, 2)) == 8);

        actest::WorldFixture east(8, 5);
        east.map.GetTile(0, 0)->SetElevation(500);
        CHECK(Mask_(east, SpriteTileLayout_t::Edges, *east.map.GetTile(7, 1)) == 1);
    }
    SECTION("rows off the map never match")
    {
        actest::WorldFixture world(8, 5);
        for (const auto& pTile : world.map.GetTiles())
        {
            pTile->SetElevation(500);
        }
        CHECK(Mask_(world, SpriteTileLayout_t::Edges, *world.map.GetTile(4, 0)) == (2 | 4));
        CHECK(Mask_(world, SpriteTileLayout_t::Edges, *world.map.GetTile(4, 4)) == (1 | 8));
    }
}
