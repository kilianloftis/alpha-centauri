#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/WorldMap.h"
#include "ui/world/MapViewport.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <vector>

using namespace ac;
using Catch::Matchers::WithinAbs;

TEST_CASE("MapViewport project/unproject round-trips isometric diamonds", "[ui][viewport][iso]")
{
    actest::WorldFixture world(9, 9);
    const WindowLayout_t layout{0.0f, 0.0f, 400.0f, 300.0f};
    MapViewport viewport(world.map, layout, /*tileSize*/ 40.0f);

    REQUIRE(viewport.TileWidth() == 40.0f);
    REQUIRE(viewport.TileHeight() == 20.0f);

    SECTION("origin tile AABB and center")
    {
        const auto origin = viewport.PixelOriginOf(0, 0);
        REQUIRE(origin);
        CHECK_THAT(origin->first, WithinAbs(0.0f, 0.01f));
        CHECK_THAT(origin->second, WithinAbs(0.0f, 0.01f));

        const Tile* pTile = world.map.GetTile(0, 0);
        REQUIRE(pTile);
        const auto center = viewport.PixelCenterOf(*pTile);
        REQUIRE(center);
        CHECK_THAT(center->first, WithinAbs(20.0f, 0.01f));
        CHECK_THAT(center->second, WithinAbs(10.0f, 0.01f));
    }

    SECTION("neighbor offsets follow 2:1 diamond axes")
    {
        const auto east = viewport.PixelOriginOf(1, 0);
        const auto south = viewport.PixelOriginOf(0, 1);
        REQUIRE(east);
        REQUIRE(south);
        CHECK_THAT(east->first, WithinAbs(20.0f, 0.01f));
        CHECK_THAT(east->second, WithinAbs(10.0f, 0.01f));
        CHECK_THAT(south->first, WithinAbs(-20.0f, 0.01f));
        CHECK_THAT(south->second, WithinAbs(10.0f, 0.01f));
    }

    SECTION("unproject hits the diamond under the cursor")
    {
        const auto hit = viewport.WorldCoordsAtPixel(20.0f, 10.0f);
        REQUIRE(hit);
        CHECK(hit->first == 0);
        CHECK(hit->second == 0);

        const auto eastHit = viewport.WorldCoordsAtPixel(40.0f, 20.0f);
        REQUIRE(eastHit);
        CHECK(eastHit->first == 1);
        CHECK(eastHit->second == 0);
    }

    SECTION("wrap-X places eastern tiles when camera sits near the seam")
    {
        REQUIRE(viewport.SetCamera(7, 0));
        const auto origin = viewport.PixelOriginOf(0, 0);
        REQUIRE(origin);
        const auto hit = viewport.WorldCoordsAtPixel(origin->first + 20.0f, origin->second + 10.0f);
        REQUIRE(hit);
        CHECK(hit->first == 0);
        CHECK(hit->second == 0);
    }

    SECTION("ForEachVisibleTile enumerates back-to-front by depth")
    {
        // Depth key is relX+relY, recoverable from AABB Y: y = layout.y + depth * (h/2).
        std::vector<float> depthKeys;
        const float halfH = viewport.TileHeight() * 0.5f;
        viewport.ForEachVisibleTile([&](const Tile&, const TileShape_t& rShape) {
            depthKeys.push_back((rShape.north.y - layout.y) / halfH);
        });
        REQUIRE_FALSE(depthKeys.empty());
        CHECK(std::is_sorted(depthKeys.begin(), depthKeys.end()));
    }
}

TEST_CASE("MapViewport raises tiles with the relief and hit-tests the raised shapes",
          "[ui][viewport][relief]")
{
    actest::WorldFixture world(9, 9);
    for (int y = 0; y < 9; ++y)
    {
        for (int x = 0; x < 9; ++x)
        {
            world.map.GetTile(x, y)->SetElevation(1000);
        }
    }
    Tile& rHill = *world.map.GetTile(4, 4);
    rHill.SetElevation(3000);
    // A quarter tile width (10 px on a 40 px tile) per 1000 m level.
    const ReliefStyle_t k_Style{0.25f, 1000.0f, 250.0f, 0.0f};

    SECTION("flat mode keeps the flat projection")
    {
        MapViewport viewport(world.map, WindowLayout_t{0.0f, 0.0f, 400.0f, 300.0f}, 40.0f);
        viewport.SetRelief(ReliefMode_t::Flat, k_Style);
        const auto center = viewport.PixelCenterOf(rHill);
        REQUIRE(center);
        CHECK_THAT(center->first, WithinAbs(20.0f, 0.01f));
        CHECK_THAT(center->second, WithinAbs(90.0f, 0.01f));
    }

    SECTION("a raised tile's centre sits above its flat centre by its lift")
    {
        MapViewport viewport(world.map, WindowLayout_t{0.0f, 0.0f, 400.0f, 300.0f}, 40.0f);
        viewport.SetRelief(ReliefMode_t::Smooth, k_Style);
        const auto center = viewport.PixelCenterOf(rHill);
        REQUIRE(center);
        CHECK_THAT(center->first, WithinAbs(20.0f, 0.01f));
        CHECK_THAT(center->second, WithinAbs(90.0f - 30.0f, 0.01f));
    }

    SECTION("a click on the raised tile picks it over the tile behind")
    {
        MapViewport viewport(world.map, WindowLayout_t{0.0f, 0.0f, 400.0f, 300.0f}, 40.0f);
        viewport.SetRelief(ReliefMode_t::Smooth, k_Style);
        // (20, 60) is the hill's raised centre and lies within the raised (3, 3) behind it too.
        const auto hit = viewport.WorldCoordsAtPixel(20.0f, 60.0f);
        REQUIRE(hit);
        CHECK(hit->first == 4);
        CHECK(hit->second == 4);
    }

    SECTION("a tile below the layout is visited once it rises into view")
    {
        // The hill's flat box starts at y = 80, below this layout; raised, its top is at 60.
        MapViewport viewport(world.map, WindowLayout_t{0.0f, 0.0f, 400.0f, 79.0f}, 40.0f);
        const auto visits = [&viewport, &rHill]() {
            bool bVisited = false;
            viewport.ForEachVisibleTile([&](const Tile& rTile, const TileShape_t&) {
                bVisited = bVisited || &rTile == &rHill;
            });
            return bVisited;
        };
        viewport.SetRelief(ReliefMode_t::Flat, k_Style);
        CHECK_FALSE(visits());
        viewport.SetRelief(ReliefMode_t::Smooth, k_Style);
        CHECK(visits());
    }
}
