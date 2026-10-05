#include "GameFixtures.h"
#include "TestHelpers.h"

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

void Mark_(actest::WorldFixture& rWorld, int x, int y)
{
    rWorld.map.GetTile(x, y)->SetElevation(500);
}

std::uint8_t Mask_(actest::WorldFixture& rWorld, SpriteTileLayout_t layout, int x, int y)
{
    return ResolveTileMask(layout, *rWorld.map.GetTile(x, y), rWorld.map, IsMarked_);
}

} // namespace

TEST_CASE("A tile with no matching neighbor has mask 0", "[ui][autotile]")
{
    actest::WorldFixture world(5, 5);
    CHECK(Mask_(world, SpriteTileLayout_t::Edges, 2, 2) == 0);
    CHECK(Mask_(world, SpriteTileLayout_t::Blob, 2, 2) == 0);
}

TEST_CASE("Each edge neighbor sets its own bit", "[ui][autotile]")
{
    // (delta, edge-layout bit, blob-layout bit): the diamond's NE, SE, SW, NW edges.
    const struct
    {
        int dx;
        int dy;
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
        CAPTURE(rCase.dx, rCase.dy);
        actest::WorldFixture world(5, 5);
        Mark_(world, 2 + rCase.dx, 2 + rCase.dy);
        CHECK(Mask_(world, SpriteTileLayout_t::Edges, 2, 2) == rCase.edgeBit);
        CHECK(Mask_(world, SpriteTileLayout_t::Blob, 2, 2) == rCase.blobBit);
    }
}

TEST_CASE("Corner neighbors count only between two matching edges", "[ui][autotile]")
{
    // Corners clockwise from N: (delta, corner bit, the two edge deltas beside it).
    const struct
    {
        int dx;
        int dy;
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
        CAPTURE(rCorner.dx, rCorner.dy);
        actest::WorldFixture world(5, 5);
        Mark_(world, 2 + rCorner.dx, 2 + rCorner.dy);
        CHECK(Mask_(world, SpriteTileLayout_t::Blob, 2, 2) == 0);
        CHECK(Mask_(world, SpriteTileLayout_t::Edges, 2, 2) == 0);

        Mark_(world, 2 + rCorner.edgeBefore.first, 2 + rCorner.edgeBefore.second);
        CHECK((Mask_(world, SpriteTileLayout_t::Blob, 2, 2) & rCorner.cornerBit) == 0);

        Mark_(world, 2 + rCorner.edgeAfter.first, 2 + rCorner.edgeAfter.second);
        CHECK((Mask_(world, SpriteTileLayout_t::Blob, 2, 2) & rCorner.cornerBit) != 0);
    }
}

TEST_CASE("Every neighbor matching gives the full masks", "[ui][autotile]")
{
    actest::WorldFixture world(5, 5);
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx != 0 || dy != 0)
            {
                Mark_(world, 2 + dx, 2 + dy);
            }
        }
    }
    CHECK(Mask_(world, SpriteTileLayout_t::Edges, 2, 2) == 15);
    CHECK(Mask_(world, SpriteTileLayout_t::Blob, 2, 2) == 255);
}

TEST_CASE("Blob masks reduce to 47 cases", "[ui][autotile]")
{
    actest::WorldFixture world(5, 5);
    const int k_Deltas[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}};
    std::set<std::uint8_t> masks;
    for (unsigned neighbors = 0; neighbors < 256; ++neighbors)
    {
        for (unsigned bit = 0; bit < 8; ++bit)
        {
            world.map.GetTile(2 + k_Deltas[bit][0], 2 + k_Deltas[bit][1])
                ->SetElevation((neighbors >> bit & 1u) ? 500 : 0);
        }
        masks.insert(Mask_(world, SpriteTileLayout_t::Blob, 2, 2));
    }
    CHECK(masks.size() == 47);
}

TEST_CASE("Tile masks wrap in x and stop at the map's top and bottom", "[ui][autotile]")
{
    SECTION("x wraps")
    {
        actest::WorldFixture world(5, 5);
        Mark_(world, 4, 2);
        CHECK(Mask_(world, SpriteTileLayout_t::Edges, 0, 2) == 8);
    }
    SECTION("rows off the map never match")
    {
        actest::WorldFixture world(5, 5);
        CHECK(Mask_(world, SpriteTileLayout_t::Blob, 2, 0) == 0);
        CHECK(Mask_(world, SpriteTileLayout_t::Blob, 2, 4) == 0);
    }
}
