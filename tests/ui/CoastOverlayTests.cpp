#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/Tile.h"
#include "ui/CoastOverlay.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

using namespace ac;

namespace
{

void MakeWater_(actest::WorldFixture& rWorld, int x, int y)
{
    Tile& rTile = *rWorld.map.GetTile(x, y);
    rTile.SetElevation(actest::TestMapRules().oceanShelfMeters);
    REQUIRE(rTile.IsWater());
}

void MakeIsland_(actest::WorldFixture& rWorld, int x, int y)
{
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx != 0 || dy != 0)
            {
                MakeWater_(rWorld, x + dx, y + dy);
            }
        }
    }
}

const CoastCornerArt_t& Corner_(const CoastOverlay_t& rOverlay, CoastCorner_t corner)
{
    return rOverlay.corners[static_cast<std::size_t>(corner)];
}

bool NoCoast_(const CoastOverlay_t& rOverlay)
{
    return std::ranges::all_of(rOverlay.corners, [](const CoastCornerArt_t& rArt) {
        return rArt.waterMask == 0 && !rArt.bAlternate;
    });
}

} // namespace

TEST_CASE("Land surrounded by land has no coast", "[ui][coast]")
{
    actest::WorldFixture world(5, 5);
    const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(2, 2), world.map);
    CHECK(NoCoast_(overlay));
    CHECK(overlay.waterNeighbors.empty());
}

TEST_CASE("Water tiles never get a coast", "[ui][coast]")
{
    actest::WorldFixture world(5, 5);
    MakeWater_(world, 2, 2);
    MakeWater_(world, 3, 2);

    const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(2, 2), world.map);
    CHECK(NoCoast_(overlay));
    CHECK(overlay.waterNeighbors.empty());
}

TEST_CASE("Water across a diamond edge marks the two corners on that edge", "[ui][coast]")
{
    struct Case_t
    {
        int dx;
        int dy;
        // The edge is clockwise of one corner (bit 4) and counter-clockwise of the next (bit 1).
        CoastCorner_t bit4Corner;
        CoastCorner_t bit1Corner;
    };
    const Case_t cases[] = {
        {0, -1, CoastCorner_t::North, CoastCorner_t::East},
        {1, 0, CoastCorner_t::East, CoastCorner_t::South},
        {0, 1, CoastCorner_t::South, CoastCorner_t::West},
        {-1, 0, CoastCorner_t::West, CoastCorner_t::North},
    };
    for (const Case_t& rCase : cases)
    {
        CAPTURE(rCase.dx, rCase.dy);
        actest::WorldFixture world(5, 5);
        MakeWater_(world, 2 + rCase.dx, 2 + rCase.dy);

        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(2, 2), world.map);
        for (const CoastCornerArt_t& rArt : overlay.corners)
        {
            CAPTURE(static_cast<int>(rArt.corner));
            std::uint8_t expected = 0;
            if (rArt.corner == rCase.bit4Corner)
            {
                expected = 4;
            }
            else if (rArt.corner == rCase.bit1Corner)
            {
                expected = 1;
            }
            CHECK(rArt.waterMask == expected);
        }
        CHECK(overlay.waterNeighbors.size() == 1);
    }
}

TEST_CASE("Water touching a diamond corner marks only that corner", "[ui][coast]")
{
    struct Case_t
    {
        int dx;
        int dy;
        CoastCorner_t corner;
    };
    const Case_t cases[] = {
        {-1, -1, CoastCorner_t::North},
        {1, -1, CoastCorner_t::East},
        {1, 1, CoastCorner_t::South},
        {-1, 1, CoastCorner_t::West},
    };
    for (const Case_t& rCase : cases)
    {
        CAPTURE(rCase.dx, rCase.dy);
        actest::WorldFixture world(5, 5);
        MakeWater_(world, 2 + rCase.dx, 2 + rCase.dy);

        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(2, 2), world.map);
        for (const CoastCornerArt_t& rArt : overlay.corners)
        {
            CAPTURE(static_cast<int>(rArt.corner));
            CHECK(rArt.waterMask == (rArt.corner == rCase.corner ? 2 : 0));
        }
        CHECK(overlay.waterNeighbors.size() == 1);
    }
}

TEST_CASE("A one-tile island is water on every side of every corner", "[ui][coast]")
{
    actest::WorldFixture world(5, 5);
    MakeIsland_(world, 2, 2);

    const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(2, 2), world.map);
    for (const CoastCornerArt_t& rArt : overlay.corners)
    {
        CHECK(rArt.waterMask == 7);
    }
    CHECK(overlay.waterNeighbors.size() == 8);
}

TEST_CASE("Rows beyond the map edge count as land", "[ui][coast]")
{
    actest::WorldFixture world(5, 5);
    const CoastOverlay_t top = ResolveCoastOverlay(*world.map.GetTile(2, 0), world.map);
    CHECK(NoCoast_(top));
    const CoastOverlay_t bottom = ResolveCoastOverlay(*world.map.GetTile(2, 4), world.map);
    CHECK(NoCoast_(bottom));
}

TEST_CASE("Coast neighbors wrap across the map's x seam", "[ui][coast]")
{
    actest::WorldFixture world(5, 5);
    MakeWater_(world, 4, 2);

    const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(0, 2), world.map);
    CHECK(Corner_(overlay, CoastCorner_t::West).waterMask == 4);
    CHECK(Corner_(overlay, CoastCorner_t::North).waterMask == 1);
    CHECK(Corner_(overlay, CoastCorner_t::East).waterMask == 0);
    CHECK(Corner_(overlay, CoastCorner_t::South).waterMask == 0);
}

TEST_CASE("Only all-water corners on odd rows use the alternate island shape", "[ui][coast]")
{
    SECTION("island on an odd row")
    {
        actest::WorldFixture world(5, 5);
        MakeIsland_(world, 2, 1);
        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(2, 1), world.map);
        for (const CoastCornerArt_t& rArt : overlay.corners)
        {
            CHECK(rArt.bAlternate);
        }
    }

    SECTION("island on an even row")
    {
        actest::WorldFixture world(5, 5);
        MakeIsland_(world, 2, 2);
        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(2, 2), world.map);
        for (const CoastCornerArt_t& rArt : overlay.corners)
        {
            CHECK_FALSE(rArt.bAlternate);
        }
    }

    SECTION("partial coast on an odd row")
    {
        actest::WorldFixture world(5, 5);
        MakeWater_(world, 1, 1);
        MakeWater_(world, 1, 0);
        MakeWater_(world, 2, 0);
        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(2, 1), world.map);
        REQUIRE(Corner_(overlay, CoastCorner_t::North).waterMask == 7);
        CHECK(Corner_(overlay, CoastCorner_t::North).bAlternate);
        for (const CoastCornerArt_t& rArt : overlay.corners)
        {
            if (rArt.corner != CoastCorner_t::North)
            {
                CHECK(rArt.waterMask != 7);
                CHECK_FALSE(rArt.bAlternate);
            }
        }
    }
}
