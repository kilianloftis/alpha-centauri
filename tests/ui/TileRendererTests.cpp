#include "GameFixtures.h"
#include "RecordingGraphics.h"
#include "TestHelpers.h"

#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/TileSpriteEdgeInset.h"
#include "ui/style/UiStyle.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;

namespace
{

// Minimal 1x1 PNG so CI without extract_terrain.py can still exercise sprite draws.
constexpr std::uint8_t k_StubPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
    0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
    0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78,
    0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00,
    0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

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

void WriteStubPng_(const std::string& path)
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(k_StubPng), sizeof(k_StubPng));
}

// Materialize every path in the list so a prior Missing cache entry cannot poison a later
// section in the same process (SpriteCache_ is process-wide).
void WriteStubPngs_(const std::vector<std::string>& paths)
{
    for (const std::string& path : paths)
    {
        if (!path.empty())
        {
            WriteStubPng_(path);
        }
    }
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

    SECTION("fungus overlays terrain instead of replacing it with a solid fill")
    {
        actest::WorldFixture world(3, 3);
        // Moist/Rocky layers are also resolved on this tile — stub them before Render so a
        // Missing SpriteCache_ entry cannot poison later sections in this process.
        WriteStubPngs_(world.improvements.Get("Moist").spritePaths);
        WriteStubPngs_(world.improvements.Get("Rocky").spritePaths);
        const auto& fungusPaths = world.improvements.Get("Fungus").spritePaths;
        REQUIRE_FALSE(fungusPaths.empty());
        const std::string& fungusPath =
            PickSpritePath(fungusPaths, 1, 1, world.improvements.Get("Fungus").id);
        WriteStubPng_(fungusPath);

        Tile& rTile = *world.map.GetTile(1, 1);
        rTile.SetElevation(500);
        rTile.SetMoisture(Moisture_t::Moist);
        rTile.SetRockiness(Rockiness_t::Rocky);
        rTile.AddTerrainFeature(world.improvements.Get("Fungus"));

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, 0.0f, 0.0f, 64.0f, /*bFogged*/ false);

        REQUIRE_FALSE(graphics.rects.empty());
        CHECK(graphics.rects.front().bFilled);
        CHECK_FALSE(ColorEq_(graphics.rects.front().color, s.fungusColor));
        bool bFungusSprite = false;
        for (const RecordingGraphics::SpriteDraw_t& rSprite : graphics.sprites)
        {
            if (rSprite.textureId == fungusPath)
            {
                bFungusSprite = true;
                CHECK(ColorEq_(rSprite.tint, Color_t::White()));
            }
        }
        CHECK(bFungusSprite);
    }

    SECTION("occupant sprite_paths draws a scaled tinted sprite")
    {
        // Bound tiles resolve Moist via the registry; ensure the configured PNG exists so CI
        // without a prior extract_terrain.py run still exercises the sprite path.
        actest::WorldFixture world(5, 5);
        const auto& moistPaths = world.improvements.Get("Moist").spritePaths;
        REQUIRE_FALSE(moistPaths.empty());
        WriteStubPngs_(moistPaths);
        const std::string& spritePath = PickSpritePath(moistPaths, 2, 2, "Moist");

        Tile& rTile = *world.map.GetTile(2, 2);
        rTile.SetElevation(500);
        rTile.SetMoisture(Moisture_t::Moist);
        rTile.SetRockiness(Rockiness_t::Flat);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, 10.0f, 20.0f, 100.0f, /*bFogged*/ false,
                             &world.map);

        const SpriteDestRect_t expected = DestRectForEdgeInsets(
            10.0f, 20.0f, 100.0f, MatchMoistureTierEdges(rTile, &world.map, Moisture_t::Moist),
            Style().tileRenderer.spriteEdgeInsetRatio);

        REQUIRE_FALSE(graphics.sprites.empty());
        bool bFound = false;
        for (const RecordingGraphics::SpriteDraw_t& rSprite : graphics.sprites)
        {
            if (rSprite.textureId == spritePath && rSprite.bScaled)
            {
                CHECK(rSprite.x == expected.x);
                CHECK(rSprite.y == expected.y);
                CHECK(rSprite.destWidth == expected.width);
                CHECK(rSprite.destHeight == expected.height);
                bFound = true;
            }
        }
        CHECK(bFound);
    }
}

TEST_CASE("PickSpriteIndex is stable and in range", "[ui][tile_renderer]")
{
    CHECK(PickSpriteIndex(0, 0, "Moist", 0) == 0);

    const size_t a = PickSpriteIndex(3, 7, "Moist", 4);
    const size_t b = PickSpriteIndex(3, 7, "Moist", 4);
    CHECK(a == b);
    CHECK(a < 4);

    const size_t otherTile = PickSpriteIndex(4, 7, "Moist", 4);
    const size_t otherId = PickSpriteIndex(3, 7, "Wet", 4);
    // Different coordinates or content id may differ; at least one of these pairs does
    // under the FNV mix used by the picker.
    CHECK((a != otherTile || a != otherId));

    for (int x = 0; x < 16; ++x)
    {
        for (int y = 0; y < 16; ++y)
        {
            const size_t index = PickSpriteIndex(x, y, "Rolling", 4);
            CHECK(index < 4);
        }
    }
}
