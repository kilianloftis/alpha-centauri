#include "ui/TileSpriteEdgeInset.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace ac;
using Catch::Matchers::WithinAbs;

TEST_CASE("DestRectForEdgeInsets shrinks unmatched edges and stays 2:1", "[ui][tile_edge_inset]")
{
    constexpr float k_X = 10.0f;
    constexpr float k_Y = 20.0f;
    constexpr float k_Size = 100.0f;
    constexpr float k_Ratio = 0.10f;

    SECTION("all-mismatch (and null-map style) shrinks symmetrically")
    {
        const SpriteEdgeMatch_t none{};
        const SpriteDestRect_t dest = DestRectForEdgeInsets(k_X, k_Y, k_Size, none, k_Ratio);
        CHECK_THAT(dest.width, WithinAbs(80.0f, 0.001f));
        CHECK_THAT(dest.height, WithinAbs(40.0f, 0.001f));
        CHECK_THAT(dest.x, WithinAbs(20.0f, 0.001f));
        CHECK_THAT(dest.y, WithinAbs(25.0f, 0.001f));
    }

    SECTION("zero inset leaves the full AABB")
    {
        const SpriteEdgeMatch_t none{};
        const SpriteDestRect_t dest = DestRectForEdgeInsets(k_X, k_Y, k_Size, none, 0.0f);
        CHECK_THAT(dest.x, WithinAbs(k_X, 0.001f));
        CHECK_THAT(dest.y, WithinAbs(k_Y, 0.001f));
        CHECK_THAT(dest.width, WithinAbs(k_Size, 0.001f));
        CHECK_THAT(dest.height, WithinAbs(k_Size * 0.5f, 0.001f));
    }

    SECTION("matching SE edge extends toward the SE relative to all-mismatch")
    {
        SpriteEdgeMatch_t seOnly{};
        seOnly.bSe = true;
        const SpriteDestRect_t all = DestRectForEdgeInsets(k_X, k_Y, k_Size, SpriteEdgeMatch_t{},
                                                           k_Ratio);
        const SpriteDestRect_t se =
            DestRectForEdgeInsets(k_X, k_Y, k_Size, seOnly, k_Ratio);
        // Flush SE → less right/bottom pad → dest grows and/or shifts SE.
        CHECK(se.width >= all.width - 0.001f);
        CHECK(se.x + se.width > all.x + all.width - 0.001f);
        CHECK(se.y + se.height > all.y + all.height - 0.001f);
        CHECK_THAT(se.height, WithinAbs(se.width * 0.5f, 0.001f));
    }

    SECTION("coastal tile stays flush on land edges while insetting water edges")
    {
        // Land to the north (NE+NW match), water to the south (SE+SW mismatch).
        SpriteEdgeMatch_t coastal{};
        coastal.bNe = true;
        coastal.bNw = true;
        const SpriteDestRect_t dest =
            DestRectForEdgeInsets(k_X, k_Y, k_Size, coastal, k_Ratio);
        CHECK_THAT(dest.y, WithinAbs(k_Y, 0.001f));
        CHECK(dest.y + dest.height < k_Y + k_Size * 0.5f - 0.001f);
        CHECK_THAT(dest.height, WithinAbs(dest.width * 0.5f, 0.001f));
    }
}

TEST_CASE("Inset sprites never extend past the tile diamond", "[ui][tile_edge_inset]")
{
    constexpr float k_X = 10.0f;
    constexpr float k_Y = 20.0f;
    constexpr float k_Size = 100.0f;
    const float halfW = k_Size * 0.5f;
    const float halfH = k_Size * 0.25f;
    const auto inTile = [&](float px, float py) {
        return std::abs(px - (k_X + halfW)) / halfW + std::abs(py - (k_Y + halfH)) / halfH
               <= 1.0f + 1e-4f;
    };

    for (int bits = 0; bits < 16; ++bits)
    {
        SpriteEdgeMatch_t match{};
        match.bNe = (bits & 1) != 0;
        match.bSe = (bits & 2) != 0;
        match.bSw = (bits & 4) != 0;
        match.bNw = (bits & 8) != 0;
        for (const float ratio : {0.1f, 0.25f, 0.45f})
        {
            CAPTURE(bits, ratio);
            const SpriteDestRect_t dest = DestRectForEdgeInsets(k_X, k_Y, k_Size, match, ratio);
            CHECK_THAT(dest.height, WithinAbs(dest.width * 0.5f, 0.001f));
            CHECK(inTile(dest.x + dest.width * 0.5f, dest.y));
            CHECK(inTile(dest.x + dest.width, dest.y + dest.height * 0.5f));
            CHECK(inTile(dest.x + dest.width * 0.5f, dest.y + dest.height));
            CHECK(inTile(dest.x, dest.y + dest.height * 0.5f));
        }
    }
}

TEST_CASE("An inset edge leaves the opposite matched edge flush", "[ui][tile_edge_inset]")
{
    // Drier land to the SW only: the sprite shrinks away from SW and keeps the tile's NE edge.
    SpriteEdgeMatch_t match{};
    match.bNe = true;
    match.bSe = true;
    match.bNw = true;
    const SpriteDestRect_t dest = DestRectForEdgeInsets(0.0f, 0.0f, 100.0f, match, 0.1f);
    CHECK(dest.width < 100.0f);

    // Tile NE edge runs from N (50, 0) to E (100, 25): y = (x - 50) / 2.
    const float northX = dest.x + dest.width * 0.5f;
    const float eastX = dest.x + dest.width;
    CHECK_THAT(dest.y, WithinAbs((northX - 50.0f) * 0.5f, 0.001f));
    CHECK_THAT(dest.y + dest.height * 0.5f, WithinAbs((eastX - 50.0f) * 0.5f, 0.001f));
}
