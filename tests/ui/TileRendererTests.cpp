#include "GameFixtures.h"
#include "RecordingGraphics.h"
#include "TestHelpers.h"

#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/style/UiStyle.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>

using namespace ac;
using actest::RecordingGraphics;

namespace
{

void EnsureStyleLoaded_()
{
    static const bool bLoaded = []
    {
        UiStyle::Load(actest::FixturePath("ui/style.json"));
        return true;
    }();
    (void)bLoaded;
}

bool ColorEq_(const Color_t& a, const Color_t& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool HasFilledColor_(const RecordingGraphics& rGraphics, const Color_t& color)
{
    for (const RecordingGraphics::RectDraw_t& rRect : rGraphics.rects)
    {
        if (rRect.bFilled && ColorEq_(rRect.color, color))
        {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("TileRenderer paints moisture/rockiness instead of numeric placeholders",
          "[ui][tile]")
{
    EnsureStyleLoaded_();
    const auto& s = Style().tileRenderer;

    SECTION("moist rocky land gets a grey ring and green center, no text")
    {
        Tile tile(0, 0);
        tile.BindMapRules(actest::TestMapRules());
        tile.SetElevation(500);
        tile.SetMoisture(Moisture_t::Moist);
        tile.SetRockiness(Rockiness_t::Rocky);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, 10.0f, 20.0f, 100.0f, /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        CHECK(HasFilledColor_(graphics, s.rockyRingColor));
        CHECK(HasFilledColor_(graphics, s.moistCenterColor));
    }

    SECTION("wet rolling land uses the lighter grey and darker green")
    {
        Tile tile(1, 1);
        tile.BindMapRules(actest::TestMapRules());
        tile.SetElevation(200);
        tile.SetMoisture(Moisture_t::Wet);
        tile.SetRockiness(Rockiness_t::Rolling);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, 0.0f, 0.0f, 80.0f, /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        CHECK(HasFilledColor_(graphics, s.rollingRingColor));
        CHECK(HasFilledColor_(graphics, s.wetCenterColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.rockyRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.moistCenterColor));
    }

    SECTION("water keeps the elevation fill and skips landform overlays")
    {
        Tile tile(2, 2);
        tile.BindMapRules(actest::TestMapRules());
        tile.SetElevation(-1500);
        tile.SetMoisture(Moisture_t::Wet);
        tile.SetRockiness(Rockiness_t::Rocky);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, 0.0f, 0.0f, 64.0f, /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        CHECK_FALSE(HasFilledColor_(graphics, s.rockyRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.rollingRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.moistCenterColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.wetCenterColor));
        REQUIRE_FALSE(graphics.rects.empty());
        CHECK(graphics.rects.front().bFilled);
        CHECK(ColorEq_(graphics.rects.front().color,
                       TileRenderer::FillColor(tile, /*bFogged*/ false)));
    }

    SECTION("deeper water is darker than shallower water on the elevation gradient")
    {
        Tile deep(4, 4);
        deep.BindMapRules(actest::TestMapRules());
        deep.SetElevation(actest::TestMapRules().minElevationMeters);
        Tile shallow(5, 5);
        shallow.BindMapRules(actest::TestMapRules());
        shallow.SetElevation(actest::TestMapRules().oceanLevelMeters - 1);

        const Color_t deepFill = TileRenderer::FillColor(deep, /*bFogged*/ false);
        const Color_t shallowFill = TileRenderer::FillColor(shallow, /*bFogged*/ false);
        const int deepLuma = deepFill.r + deepFill.g + deepFill.b;
        const int shallowLuma = shallowFill.r + shallowFill.g + shallowFill.b;
        CHECK(deepLuma < shallowLuma);
        CHECK(ColorEq_(deepFill, s.waterLowColor));
        CHECK(ColorEq_(shallowFill, s.waterHighColor));
    }

    SECTION("arid flat land is brown fill only")
    {
        Tile tile(3, 3);
        tile.BindMapRules(actest::TestMapRules());
        tile.SetElevation(100);
        tile.SetMoisture(Moisture_t::Arid);
        tile.SetRockiness(Rockiness_t::Flat);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, 0.0f, 0.0f, 50.0f, /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        CHECK_FALSE(HasFilledColor_(graphics, s.rockyRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.rollingRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.moistCenterColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.wetCenterColor));
    }

    SECTION("occupant sprite_path draws a scaled tinted sprite")
    {
        // Bound tiles resolve Moist via the registry; ensure the configured PNG exists so CI
        // without a prior extract_terrain.py run still exercises the sprite path.
        actest::WorldFixture world(5, 5);
        const std::string& spritePath = world.improvements.Get("Moist").spritePath;
        REQUIRE_FALSE(spritePath.empty());
        std::filesystem::create_directories(std::filesystem::path(spritePath).parent_path());
        static const std::uint8_t k_Png[] = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
            0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
            0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78,
            0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00,
            0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};
        {
            std::ofstream out(spritePath, std::ios::binary);
            out.write(reinterpret_cast<const char*>(k_Png), sizeof(k_Png));
        }

        Tile& rTile = *world.map.GetTile(2, 2);
        rTile.SetElevation(500);
        rTile.SetMoisture(Moisture_t::Moist);
        rTile.SetRockiness(Rockiness_t::Flat);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, 10.0f, 20.0f, 100.0f, /*bFogged*/ false);

        REQUIRE_FALSE(graphics.sprites.empty());
        bool bFound = false;
        for (const RecordingGraphics::SpriteDraw_t& rSprite : graphics.sprites)
        {
            if (rSprite.textureId == spritePath && rSprite.bScaled)
            {
                CHECK(rSprite.x == 10.0f);
                CHECK(rSprite.y == 20.0f);
                CHECK(rSprite.destWidth == 100.0f);
                CHECK(rSprite.destHeight == 50.0f);
                bFound = true;
            }
        }
        CHECK(bFound);
    }
}
