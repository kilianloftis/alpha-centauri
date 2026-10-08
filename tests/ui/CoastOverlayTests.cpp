#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "ui/CoastOverlay.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

using namespace ac;

namespace
{

void MakeWater_(Tile& rTile)
{
    rTile.SetElevation(actest::TestMapRules().oceanShelfMeters);
    REQUIRE(rTile.IsWater());
}

void MakeWater_(actest::WorldFixture& rWorld, int x, int y)
{
    MakeWater_(*rWorld.map.GetTile(x, y));
}

void MakeWaterAtOffset_(actest::WorldFixture& rWorld, const Tile& rOrigin, int p, int q)
{
    MakeWater_(*GetTileAtLatticeOffset(rWorld.map, rOrigin, p, q));
}

void MakeIsland_(actest::WorldFixture& rWorld, int x, int y)
{
    const Tile& rCentre = *rWorld.map.GetTile(x, y);
    for (int q = -1; q <= 1; ++q)
    {
        for (int p = -1; p <= 1; ++p)
        {
            if (p != 0 || q != 0)
            {
                MakeWaterAtOffset_(rWorld, rCentre, p, q);
            }
        }
    }
}

const CoastCornerArt_t& Corner_(const CoastOverlay_t& rOverlay, DiamondCorner_t corner)
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
    actest::WorldFixture world;
    const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(8, 4), world.map);
    CHECK(NoCoast_(overlay));
}

TEST_CASE("Water tiles never get a coast", "[ui][coast]")
{
    actest::WorldFixture world;
    MakeWater_(world, 8, 4);
    MakeWater_(world, 9, 5);

    const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(8, 4), world.map);
    CHECK(NoCoast_(overlay));
}

TEST_CASE("Water across a diamond edge marks the two corners on that edge", "[ui][coast]")
{
    struct Case_t
    {
        int p;
        int q;
        // The edge is clockwise of one corner (bit 4) and counter-clockwise of the next (bit 1).
        DiamondCorner_t bit4Corner;
        DiamondCorner_t bit1Corner;
    };
    const Case_t cases[] = {
        {0, -1, DiamondCorner_t::North, DiamondCorner_t::East},
        {1, 0, DiamondCorner_t::East, DiamondCorner_t::South},
        {0, 1, DiamondCorner_t::South, DiamondCorner_t::West},
        {-1, 0, DiamondCorner_t::West, DiamondCorner_t::North},
    };
    for (const Case_t& rCase : cases)
    {
        CAPTURE(rCase.p, rCase.q);
        actest::WorldFixture world;
        MakeWaterAtOffset_(world, *world.map.GetTile(8, 4), rCase.p, rCase.q);

        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(8, 4), world.map);
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
    }
}

TEST_CASE("Water touching a diamond corner marks only that corner", "[ui][coast]")
{
    struct Case_t
    {
        int p;
        int q;
        DiamondCorner_t corner;
    };
    const Case_t cases[] = {
        {-1, -1, DiamondCorner_t::North},
        {1, -1, DiamondCorner_t::East},
        {1, 1, DiamondCorner_t::South},
        {-1, 1, DiamondCorner_t::West},
    };
    for (const Case_t& rCase : cases)
    {
        CAPTURE(rCase.p, rCase.q);
        actest::WorldFixture world;
        MakeWaterAtOffset_(world, *world.map.GetTile(8, 4), rCase.p, rCase.q);

        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(8, 4), world.map);
        for (const CoastCornerArt_t& rArt : overlay.corners)
        {
            CAPTURE(static_cast<int>(rArt.corner));
            CHECK(rArt.waterMask == (rArt.corner == rCase.corner ? 2 : 0));
        }
    }
}

TEST_CASE("A one-tile island is water on every side of every corner", "[ui][coast]")
{
    actest::WorldFixture world;
    MakeIsland_(world, 8, 4);

    const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(8, 4), world.map);
    for (const CoastCornerArt_t& rArt : overlay.corners)
    {
        CHECK(rArt.waterMask == 7);
    }
}

TEST_CASE("Rows beyond the map edge count as land", "[ui][coast]")
{
    actest::WorldFixture world;
    const CoastOverlay_t top = ResolveCoastOverlay(*world.map.GetTile(8, 0), world.map);
    CHECK(NoCoast_(top));
    const CoastOverlay_t bottom =
        ResolveCoastOverlay(*world.map.GetTile(8, world.map.GetHeight() - 1), world.map);
    CHECK(NoCoast_(bottom));
}

TEST_CASE("Coast neighbors wrap across the map's x seam", "[ui][coast]")
{
    actest::WorldFixture world;
    MakeWater_(world, -1, 3);

    const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(0, 4), world.map);
    CHECK(Corner_(overlay, DiamondCorner_t::West).waterMask == 4);
    CHECK(Corner_(overlay, DiamondCorner_t::North).waterMask == 1);
    CHECK(Corner_(overlay, DiamondCorner_t::East).waterMask == 0);
    CHECK(Corner_(overlay, DiamondCorner_t::South).waterMask == 0);
}

TEST_CASE("Only all-water corners on odd rows use the alternate island shape", "[ui][coast]")
{
    SECTION("island on an odd row")
    {
        actest::WorldFixture world;
        MakeIsland_(world, 9, 3);
        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(9, 3), world.map);
        for (const CoastCornerArt_t& rArt : overlay.corners)
        {
            CHECK(rArt.bAlternate);
        }
    }

    SECTION("island on an even row")
    {
        actest::WorldFixture world;
        MakeIsland_(world, 8, 4);
        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(8, 4), world.map);
        for (const CoastCornerArt_t& rArt : overlay.corners)
        {
            CHECK_FALSE(rArt.bAlternate);
        }
    }

    SECTION("partial coast on an odd row")
    {
        actest::WorldFixture world;
        MakeWater_(world, 8, 2);
        MakeWater_(world, 9, 1);
        MakeWater_(world, 10, 2);
        const CoastOverlay_t overlay = ResolveCoastOverlay(*world.map.GetTile(9, 3), world.map);
        REQUIRE(Corner_(overlay, DiamondCorner_t::North).waterMask == 7);
        CHECK(Corner_(overlay, DiamondCorner_t::North).bAlternate);
        for (const CoastCornerArt_t& rArt : overlay.corners)
        {
            if (rArt.corner != DiamondCorner_t::North)
            {
                CHECK(rArt.waterMask != 7);
                CHECK_FALSE(rArt.bAlternate);
            }
        }
    }
}
