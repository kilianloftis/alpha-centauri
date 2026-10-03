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
        viewport.ForEachVisibleTile([&](const Tile&, float, float pixelY) {
            depthKeys.push_back((pixelY - layout.y) / halfH);
        });
        REQUIRE_FALSE(depthKeys.empty());
        CHECK(std::is_sorted(depthKeys.begin(), depthKeys.end()));
    }
}
