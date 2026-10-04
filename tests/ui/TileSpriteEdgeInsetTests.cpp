#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/Tile.h"
#include "ui/TileSpriteEdgeInset.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

TEST_CASE("Moisture tier edges match at-least tier; null map mismatches all",
          "[ui][tile_edge_inset]")
{
    actest::WorldFixture world(5, 5);
    Tile& rCenter = *world.map.GetTile(2, 2);
    rCenter.SetElevation(500);
    rCenter.SetMoisture(Moisture_t::Wet);

    Tile& rEast = *world.map.GetTile(3, 2);
    rEast.SetElevation(500);
    rEast.SetMoisture(Moisture_t::Moist);

    Tile& rWest = *world.map.GetTile(1, 2);
    rWest.SetElevation(500);
    rWest.SetMoisture(Moisture_t::Arid);

    const SpriteEdgeMatch_t nullMap = MatchMoistureTierEdges(rCenter, nullptr, Moisture_t::Moist);
    CHECK_FALSE(nullMap.bNe);
    CHECK_FALSE(nullMap.bSe);
    CHECK_FALSE(nullMap.bSw);
    CHECK_FALSE(nullMap.bNw);

    // Moist tier: east (moist) matches, west (arid) does not. E→SE, W→NW.
    const SpriteEdgeMatch_t moistTier =
        MatchMoistureTierEdges(rCenter, &world.map, Moisture_t::Moist);
    CHECK(moistTier.bSe);
    CHECK_FALSE(moistTier.bNw);

    // Wet tier: neither ortho land neighbor is wet.
    const SpriteEdgeMatch_t wetTier =
        MatchMoistureTierEdges(rCenter, &world.map, Moisture_t::Wet);
    CHECK_FALSE(wetTier.bSe);
    CHECK_FALSE(wetTier.bNw);

    // Arid tier: both land neighbors are at least arid.
    const SpriteEdgeMatch_t aridTier =
        MatchMoistureTierEdges(rCenter, &world.map, Moisture_t::Arid);
    CHECK(aridTier.bSe);
    CHECK(aridTier.bNw);
}

TEST_CASE("Coastal water neighbors do not inset moisture tiers", "[ui][tile_edge_inset]")
{
    actest::WorldFixture world(5, 5);
    Tile& rLand = *world.map.GetTile(2, 2);
    rLand.SetElevation(500);
    rLand.SetMoisture(Moisture_t::Moist);

    Tile& rLandEast = *world.map.GetTile(3, 2);
    rLandEast.SetElevation(500);
    rLandEast.SetMoisture(Moisture_t::Moist);

    // South neighbor is water (map E→SE, S→SW).
    Tile& rWaterSouth = *world.map.GetTile(2, 3);
    rWaterSouth.SetElevation(actest::TestMapRules().oceanShelfMeters);
    REQUIRE(rWaterSouth.IsWater());

    const SpriteEdgeMatch_t moist =
        MatchMoistureTierEdges(rLand, &world.map, Moisture_t::Moist);
    CHECK(moist.bSe); // land east
    CHECK(moist.bSw); // water south — flush, not a rainfall mismatch
}

TEST_CASE("Coastal land neighbors do not inset sea landform sprites", "[ui][tile_edge_inset]")
{
    actest::WorldFixture world(5, 5);
    Tile& rShelf = *world.map.GetTile(2, 2);
    rShelf.SetElevation(actest::TestMapRules().oceanShelfMeters);
    REQUIRE(rShelf.IsWater());
    REQUIRE(rShelf.HasFeature("OceanShelf"));

    Tile& rShelfEast = *world.map.GetTile(3, 2);
    rShelfEast.SetElevation(actest::TestMapRules().oceanShelfMeters);
    REQUIRE(rShelfEast.HasFeature("OceanShelf"));

    Tile& rLandSouth = *world.map.GetTile(2, 3);
    rLandSouth.SetElevation(500);
    REQUIRE(rLandSouth.IsLand());

    const SpriteEdgeMatch_t shelf =
        MatchSeaLandformEdges(rShelf, &world.map, "OceanShelf");
    CHECK(shelf.bSe); // shelf east
    CHECK(shelf.bSw); // land south — flush, not a depth-band mismatch
}
