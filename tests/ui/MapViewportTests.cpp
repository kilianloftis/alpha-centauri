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

TEST_CASE("MapViewport projects SMAC tiles as a rectangular brick", "[ui][viewport][brick]")
{
    actest::WorldFixture world;
    const int width = world.map.GetWidth();
    const WindowLayout_t layout{0.0f, 0.0f, 400.0f, 300.0f};
    MapViewport viewport(world.map, layout, /*tileSize*/ 40.0f);

    REQUIRE(viewport.TileWidth() == 40.0f);
    REQUIRE(viewport.TileHeight() == 20.0f);

    SECTION("layout size is counted in half-tile map units")
    {
        CHECK(viewport.VisibleCols() == 20);
        CHECK(viewport.VisibleRows() == 30);
    }

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

    SECTION("a row's tiles share a screen y and sit one tile width apart")
    {
        for (const int y : {0, 1, 4, 5})
        {
            const int firstX = y % 2;
            const auto first = viewport.PixelOriginOf(firstX, y);
            REQUIRE(first);
            CHECK_THAT(first->second, WithinAbs(10.0f * static_cast<float>(y), 0.01f));
            for (int step = 1; step < 5; ++step)
            {
                const auto next = viewport.PixelOriginOf(firstX + 2 * step, y);
                REQUIRE(next);
                CHECK_THAT(next->second, WithinAbs(first->second, 0.01f));
                CHECK_THAT(next->first - first->first, WithinAbs(40.0f * step, 0.01f));
            }
        }
    }

    SECTION("odd rows are offset by half a tile from the row above")
    {
        const auto even = viewport.PixelOriginOf(2, 2);
        const auto odd = viewport.PixelOriginOf(3, 3);
        REQUIRE(even);
        REQUIRE(odd);
        CHECK_THAT(odd->first - even->first, WithinAbs(20.0f, 0.01f));
        CHECK_THAT(odd->second - even->second, WithinAbs(10.0f, 0.01f));
    }

    SECTION("row 0 is the top of the drawn map")
    {
        float topY = 1.0e9f;
        int firstRow = -1;
        viewport.ForEachVisibleTile([&](const Tile& rTile, const TileShape_t& rShape) {
            if (firstRow < 0)
            {
                firstRow = rTile.GetY();
            }
            topY = std::min(topY, rShape.north.y);
        });
        CHECK(firstRow == 0);
        CHECK_THAT(topY, WithinAbs(layout.y, 0.01f));
    }

    SECTION("the wrap seam is a vertical line")
    {
        REQUIRE(viewport.SetCamera(width - 4, 0));
        for (int y = 0; y < 8; ++y)
        {
            const int firstX = y % 2;
            const int lastX = width - 2 + firstX;
            const auto before = viewport.PixelOriginOf(lastX, y);
            const auto after = viewport.PixelOriginOf(firstX, y);
            REQUIRE(before);
            REQUIRE(after);
            CHECK_THAT(after->second, WithinAbs(before->second, 0.01f));
            CHECK_THAT(after->first - before->first, WithinAbs(40.0f, 0.01f));
            CHECK_THAT(after->first, WithinAbs(20.0f * static_cast<float>(firstX + 4), 0.01f));
        }
    }

    SECTION("unproject hits the diamond under the cursor")
    {
        const auto hit = viewport.WorldCoordsAtPixel(20.0f, 10.0f);
        REQUIRE(hit);
        CHECK(hit->first == 0);
        CHECK(hit->second == 0);

        const auto eastHit = viewport.WorldCoordsAtPixel(60.0f, 10.0f);
        REQUIRE(eastHit);
        CHECK(eastHit->first == 2);
        CHECK(eastHit->second == 0);

        const auto southEastHit = viewport.WorldCoordsAtPixel(40.0f, 20.0f);
        REQUIRE(southEastHit);
        CHECK(southEastHit->first == 1);
        CHECK(southEastHit->second == 1);

        const auto southHit = viewport.WorldCoordsAtPixel(20.0f, 30.0f);
        REQUIRE(southHit);
        CHECK(southHit->first == 0);
        CHECK(southHit->second == 2);
    }

    SECTION("wrap-X places eastern tiles when camera sits near the seam")
    {
        REQUIRE(viewport.SetCamera(width - 2, 0));
        const auto origin = viewport.PixelOriginOf(0, 0);
        REQUIRE(origin);
        CHECK_THAT(origin->first, WithinAbs(40.0f, 0.01f));
        const auto hit = viewport.WorldCoordsAtPixel(origin->first + 20.0f, origin->second + 10.0f);
        REQUIRE(hit);
        CHECK(hit->first == 0);
        CHECK(hit->second == 0);
    }

    SECTION("hit tests round-trip every visible tile, across the seam")
    {
        for (const int cameraX : {0, width - 4, width / 2})
        {
            viewport.SetCamera(cameraX, 0);
            int checked = 0;
            viewport.ForEachVisibleTile([&](const Tile& rTile, const TileShape_t& rShape) {
                const auto [originX, originY] = viewport.FootprintOrigin(rShape);
                const std::pair<float, float> center{originX + viewport.TileWidth() * 0.5f,
                                                     originY + viewport.TileHeight() * 0.5f};
                const bool bInLayout = center.first >= layout.x
                    && center.first < layout.x + layout.width && center.second >= layout.y
                    && center.second < layout.y + layout.height;
                if (!bInLayout)
                {
                    return;
                }
                const auto hit = viewport.WorldCoordsAtPixel(center.first, center.second);
                REQUIRE(hit);
                CHECK(hit->first == rTile.GetX());
                CHECK(hit->second == rTile.GetY());
                ++checked;
            });
            CHECK(checked > 0);
        }
    }

    SECTION("ForEachVisibleTile enumerates rows north to south")
    {
        std::vector<float> depthKeys;
        const float halfH = viewport.TileHeight() * 0.5f;
        viewport.ForEachVisibleTile([&](const Tile&, const TileShape_t& rShape) {
            depthKeys.push_back((rShape.north.y - layout.y) / halfH);
        });
        REQUIRE_FALSE(depthKeys.empty());
        CHECK(std::is_sorted(depthKeys.begin(), depthKeys.end()));
    }

    SECTION("a row's visible tiles share one screen y")
    {
        std::vector<std::pair<int, float>> rows;
        viewport.ForEachVisibleTile([&](const Tile& rTile, const TileShape_t& rShape) {
            rows.emplace_back(rTile.GetY(), rShape.north.y);
        });
        REQUIRE_FALSE(rows.empty());
        for (const auto& [row, northY] : rows)
        {
            CHECK_THAT(northY, WithinAbs(10.0f * static_cast<float>(row), 0.01f));
        }
    }
}

TEST_CASE("MapViewport raises tiles with the relief and hit-tests the raised shapes",
          "[ui][viewport][relief]")
{
    actest::WorldFixture world;
    for (const auto& pTile : world.map.GetTiles())
    {
        pTile->SetElevation(1000);
    }
    Tile& rHill = *world.map.GetTile(10, 8);
    rHill.SetElevation(3000);
    // A quarter tile width (10 px on a 40 px tile) per 1000 m level.
    const ReliefStyle_t k_Style{0.25f, 1000.0f, 250.0f, 0.0f};

    SECTION("flat mode keeps the flat projection")
    {
        MapViewport viewport(world.map, WindowLayout_t{0.0f, 0.0f, 400.0f, 300.0f}, 40.0f);
        viewport.SetRelief(ReliefMode_t::Flat, k_Style);
        const auto center = viewport.PixelCenterOf(rHill);
        REQUIRE(center);
        CHECK_THAT(center->first, WithinAbs(220.0f, 0.01f));
        CHECK_THAT(center->second, WithinAbs(90.0f, 0.01f));
    }

    SECTION("a raised tile's contents sit at the mean of its corner lifts")
    {
        MapViewport viewport(world.map, WindowLayout_t{0.0f, 0.0f, 400.0f, 300.0f}, 40.0f);
        viewport.SetRelief(ReliefMode_t::Smooth, k_Style);
        const auto center = viewport.PixelCenterOf(rHill);
        REQUIRE(center);
        // Each corner averages the hill's 3 levels with three tiles at 1: 1.5 levels, 15 px.
        CHECK_THAT(center->first, WithinAbs(220.0f, 0.01f));
        CHECK_THAT(center->second, WithinAbs(90.0f - 15.0f, 0.01f));
    }

    SECTION("a click on the raised tile picks it over the tile behind")
    {
        MapViewport viewport(world.map, WindowLayout_t{0.0f, 0.0f, 400.0f, 300.0f}, 40.0f);
        viewport.SetRelief(ReliefMode_t::Smooth, k_Style);
        // (220, 60) is the hill's raised centre and lies within the raised tile (10, 6) behind it
        // too.
        const auto hit = viewport.WorldCoordsAtPixel(220.0f, 60.0f);
        REQUIRE(hit);
        CHECK(hit->first == 10);
        CHECK(hit->second == 8);
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

TEST_CASE("MapViewport hit tests round-trip raised tiles across the seam",
          "[ui][viewport][relief]")
{
    actest::WorldFixture world;
    for (const auto& pTile : world.map.GetTiles())
    {
        pTile->SetElevation(1000);
    }
    const int width = world.map.GetWidth();
    Tile& rSeamHill = *world.map.GetTile(0, 8);
    rSeamHill.SetElevation(3000);
    const ReliefStyle_t k_Style{0.25f, 1000.0f, 250.0f, 0.0f};
    const WindowLayout_t layout{0.0f, 0.0f, 400.0f, 300.0f};
    MapViewport viewport(world.map, layout, 40.0f);
    viewport.SetRelief(ReliefMode_t::Smooth, k_Style);
    REQUIRE(viewport.SetCamera(width - 8, 0));

    const auto center = viewport.PixelCenterOf(rSeamHill);
    REQUIRE(center);
    const auto hit = viewport.WorldCoordsAtPixel(center->first, center->second);
    REQUIRE(hit);
    CHECK(hit->first == 0);
    CHECK(hit->second == 8);
}
