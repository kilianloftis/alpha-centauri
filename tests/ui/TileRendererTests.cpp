#include "GameFixtures.h"
#include "RecordingGraphics.h"
#include "TestHelpers.h"

#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/style/UiStyle.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;
using Catch::Matchers::WithinAbs;

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

// Test processes run in parallel and share these files, so a stub is written once, through a
// rename, and never truncated under another process's load.
void WriteStubPng_(const std::string& path)
{
    std::error_code error;
    if (std::filesystem::file_size(path, error) == sizeof(k_StubPng))
    {
        return;
    }
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    const std::string tempPath = path + "." + std::to_string(std::random_device{}()) + ".tmp";
    {
        std::ofstream out(tempPath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(k_StubPng), sizeof(k_StubPng));
    }
    std::filesystem::rename(tempPath, path);
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

// Tile changes re-derive the current moisture from the base, so fixtures set both.
void SetMoisture_(Tile& rTile, Moisture_t moisture)
{
    rTile.SetBaseMoisture(moisture);
    rTile.SetMoisture(moisture);
}

// A tile set's sprite for one neighbor mask.
std::string TilePath_(const std::string& pattern, unsigned mask)
{
    std::string path = pattern;
    path.replace(path.find("{mask}"), std::string_view("{mask}").size(), std::to_string(mask));
    return path;
}

// Everything before {mask}, to match any sprite of the set.
std::string TilePrefix_(const std::string& pattern)
{
    return pattern.substr(0, pattern.find("{mask}"));
}

void WriteOccupantStubs_(const ImprovementConfig_t& rOccupant)
{
    WriteStubPngs_(rOccupant.spritePaths.land);
    WriteStubPngs_(rOccupant.spritePaths.sea);
    if (!rOccupant.spriteTiles)
    {
        return;
    }
    for (const std::string* pPattern : {&rOccupant.spriteTiles->land, &rOccupant.spriteTiles->sea})
    {
        if (pPattern->empty())
        {
            continue;
        }
        for (unsigned mask = 0; mask < 256; ++mask)
        {
            WriteStubPng_(TilePath_(*pPattern, mask));
        }
    }
}

// extract_terrain.py's coast overlay naming: <dir>/<part>_<corner>_<case>.png.
std::string CoastPath_(std::string_view part, char corner, std::string_view caseName)
{
    return Style().tileRenderer.coastSpriteDir + "/" + std::string(part) + "_" + corner + "_"
           + std::string(caseName) + ".png";
}

void WriteCoastStubs_()
{
    for (const std::string_view part : {"water", "shore"})
    {
        for (const char corner : {'w', 'n', 'e', 's'})
        {
            for (const std::string_view caseName : {"1", "2", "3", "4", "5", "6", "7", "7_alt"})
            {
                WriteStubPng_(CoastPath_(part, corner, caseName));
            }
        }
    }
}

std::vector<RecordingGraphics::SpriteDraw_t> CoastSprites_(const RecordingGraphics& rGraphics,
                                                           std::string_view part = "")
{
    const std::string prefix = Style().tileRenderer.coastSpriteDir + "/" + std::string(part);
    std::vector<RecordingGraphics::SpriteDraw_t> coast;
    for (const RecordingGraphics::SpriteDraw_t& rSprite : rGraphics.sprites)
    {
        if (rSprite.textureId.starts_with(prefix))
        {
            coast.push_back(rSprite);
        }
    }
    return coast;
}

std::ptrdiff_t FirstSpriteIndex_(const RecordingGraphics& rGraphics, std::string_view prefix)
{
    for (std::size_t i = 0; i < rGraphics.sprites.size(); ++i)
    {
        if (rGraphics.sprites[i].textureId.starts_with(prefix))
        {
            return static_cast<std::ptrdiff_t>(i);
        }
    }
    return -1;
}

std::ptrdiff_t LastSpriteIndex_(const RecordingGraphics& rGraphics, std::string_view prefix)
{
    for (std::size_t i = rGraphics.sprites.size(); i > 0; --i)
    {
        if (rGraphics.sprites[i - 1].textureId.starts_with(prefix))
        {
            return static_cast<std::ptrdiff_t>(i - 1);
        }
    }
    return -1;
}

// Tint the renderer gives this tile's own sprite whose path starts with prefix.
Color_t DrawnTint_(const Tile& rTile, const WorldMap& rMap, const std::string& prefix, bool bFogged)
{
    RecordingGraphics graphics;
    TileRenderer::Render(graphics, rTile, 0.0f, 0.0f, 64.0f, bFogged, &rMap);
    for (const RecordingGraphics::SpriteDraw_t& rSprite : graphics.sprites)
    {
        if (rSprite.textureId.starts_with(prefix))
        {
            return rSprite.tint;
        }
    }
    FAIL("sprite not drawn: " << prefix);
    return Color_t::White();
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
        WriteOccupantStubs_(world.improvements.Get("Moist"));
        WriteOccupantStubs_(world.improvements.Get("Rocky"));
        WriteOccupantStubs_(world.improvements.Get("Fungus"));
        REQUIRE(world.improvements.Get("Fungus").spriteTiles.has_value());
        // No map: every tile set uses its mask-0 sprite.
        const std::string fungusPath =
            TilePath_(world.improvements.Get("Fungus").spriteTiles->land, 0);

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

    SECTION("the moisture base fills the whole tile")
    {
        // Bound tiles resolve Moist via the registry; ensure the configured PNGs exist so CI
        // without a prior extract_terrain.py run still exercises the sprite path.
        actest::WorldFixture world(5, 5);
        const ImprovementConfig_t& rMoist = world.improvements.Get("Moist");
        WriteOccupantStubs_(rMoist);
        const std::string moistPrefix = TilePrefix_(rMoist.spriteTiles->land);

        Tile& rTile = *world.map.GetTile(2, 2);
        rTile.SetElevation(500);
        rTile.SetMoisture(Moisture_t::Moist);
        rTile.SetRockiness(Rockiness_t::Flat);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, 10.0f, 20.0f, 100.0f, /*bFogged*/ false,
                             &world.map);

        const std::ptrdiff_t index = FirstSpriteIndex_(graphics, moistPrefix);
        REQUIRE(index >= 0);
        const RecordingGraphics::SpriteDraw_t& rSprite =
            graphics.sprites[static_cast<std::size_t>(index)];
        CHECK(rSprite.bScaled);
        CHECK(rSprite.x == 10.0f);
        CHECK(rSprite.y == 20.0f);
        CHECK(rSprite.destWidth == 100.0f);
        CHECK(rSprite.destHeight == 50.0f);
        // Terrain cells are diamonds that meet their neighbors edge to edge.
        CHECK(rSprite.diamondTint.has_value());
    }

    SECTION("land art keeps its sheet colours at any elevation")
    {
        actest::WorldFixture world(5, 5);
        const ImprovementConfig_t& rMoist = world.improvements.Get("Moist");
        WriteOccupantStubs_(rMoist);
        Tile& rTile = *world.map.GetTile(2, 2);
        SetMoisture_(rTile, Moisture_t::Moist);
        for (const int elevation : {1, actest::TestMapRules().maxElevationMeters})
        {
            CAPTURE(elevation);
            rTile.SetElevation(elevation);
            CHECK(ColorEq_(DrawnTint_(rTile, world.map, TilePrefix_(rMoist.spriteTiles->land),
                                      /*bFogged*/ false),
                           Color_t::White()));
        }
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

TEST_CASE("TileRenderer draws SMAC coast overlays on land next to water", "[ui][tile][coast]")
{
    EnsureStyleLoaded_();
    WriteCoastStubs_();

    actest::WorldFixture world(5, 5);
    for (const char* id :
         {"Moist", "Fungus", "MonsoonJungle", "River", "OceanShelf", "Nutrients", "Mine"})
    {
        WriteOccupantStubs_(world.improvements.Get(id));
    }
    const auto tilePrefix = [&world](const char* id) {
        return TilePrefix_(world.improvements.Get(id).spriteTiles->land);
    };
    const std::string moistPath = tilePrefix("Moist");
    const std::string& shelfPath = world.improvements.Get("OceanShelf").spritePaths.sea.front();
    const int shelfMeters = actest::TestMapRules().oceanShelfMeters;

    Tile& rLand = *world.map.GetTile(2, 2);
    rLand.SetElevation(500);
    SetMoisture_(rLand, Moisture_t::Moist);

    constexpr float k_X = 10.0f;
    constexpr float k_Y = 20.0f;
    constexpr float k_Size = 100.0f;

    SECTION("water across the NE edge draws the north and east corners at the tile rect")
    {
        world.map.GetTile(2, 1)->SetElevation(shelfMeters);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);

        const std::vector<RecordingGraphics::SpriteDraw_t> coast = CoastSprites_(graphics);
        REQUIRE(coast.size() == 4);
        CHECK(coast[0].textureId == CoastPath_("water", 'n', "4"));
        CHECK(coast[1].textureId == CoastPath_("water", 'e', "1"));
        CHECK(coast[2].textureId == CoastPath_("shore", 'n', "4"));
        CHECK(coast[3].textureId == CoastPath_("shore", 'e', "1"));
        for (const RecordingGraphics::SpriteDraw_t& rSprite : coast)
        {
            CHECK(rSprite.bScaled);
            CHECK(rSprite.x == k_X);
            CHECK(rSprite.y == k_Y);
            CHECK(rSprite.destWidth == k_Size);
            CHECK(rSprite.destHeight == k_Size * 0.5f);
        }
    }

    SECTION("coast covers the terrain layers and sits under improvements")
    {
        world.map.GetTile(2, 1)->SetElevation(shelfMeters);
        rLand.AddTerrainFeature(world.improvements.Get("Fungus"));
        rLand.AddImprovement(world.improvements.Get("Mine"));

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);

        const std::string coastPrefix = Style().tileRenderer.coastSpriteDir + "/";
        const std::ptrdiff_t moist = FirstSpriteIndex_(graphics, moistPath);
        const std::ptrdiff_t fungus = FirstSpriteIndex_(graphics, tilePrefix("Fungus"));
        const std::ptrdiff_t firstCoast = FirstSpriteIndex_(graphics, coastPrefix);
        const std::ptrdiff_t lastCoast = LastSpriteIndex_(graphics, coastPrefix);
        const std::ptrdiff_t mine =
            FirstSpriteIndex_(graphics, world.improvements.Get("Mine").spritePaths.land.front());
        REQUIRE(moist >= 0);
        REQUIRE(fungus >= 0);
        REQUIRE(firstCoast >= 0);
        REQUIRE(mine >= 0);
        CHECK(moist < firstCoast);
        CHECK(fungus < firstCoast);
        CHECK(lastCoast < mine);
    }

    SECTION("landmarks draw between the base and vegetation, rivers between coast and bonuses")
    {
        world.map.GetTile(2, 1)->SetElevation(shelfMeters);
        rLand.AddTerrainFeature(world.improvements.Get("MonsoonJungle"));
        rLand.AddTerrainFeature(world.improvements.Get("Fungus"));
        rLand.AddTerrainFeature(world.improvements.Get("Nutrients"));
        rLand.SetHasRiver(true);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);

        const std::string coastPrefix = Style().tileRenderer.coastSpriteDir + "/";
        const std::ptrdiff_t moist = FirstSpriteIndex_(graphics, moistPath);
        const std::ptrdiff_t jungle = FirstSpriteIndex_(graphics, tilePrefix("MonsoonJungle"));
        const std::ptrdiff_t fungus = FirstSpriteIndex_(graphics, tilePrefix("Fungus"));
        const std::ptrdiff_t lastCoast = LastSpriteIndex_(graphics, coastPrefix);
        const std::ptrdiff_t river = FirstSpriteIndex_(graphics, tilePrefix("River"));
        const std::ptrdiff_t bonus = FirstSpriteIndex_(
            graphics, world.improvements.Get("Nutrients").spritePaths.land.front());
        REQUIRE(moist >= 0);
        REQUIRE(jungle >= 0);
        REQUIRE(fungus >= 0);
        REQUIRE(lastCoast >= 0);
        REQUIRE(river >= 0);
        REQUIRE(bonus >= 0);
        CHECK(moist < jungle);
        CHECK(jungle < fungus);
        CHECK(lastCoast < river);
        CHECK(river < bonus);
    }

    SECTION("coast water is shaded by the depths around the land tile's own vertices")
    {
        // Water across the NW edge, at the N corner, across the NE edge and at the E corner.
        for (const auto& [wx, wy] :
             {std::pair{1, 2}, std::pair{1, 1}, std::pair{2, 1}, std::pair{3, 1}})
        {
            world.map.GetTile(wx, wy)->SetElevation(shelfMeters);
        }

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);

        const std::vector<Color_t>& rShelf =
            Style().tileRenderer.waterShading.tints.at("OceanShelf");
        const std::vector<RecordingGraphics::SpriteDraw_t> water = CoastSprites_(graphics, "water_");
        REQUIRE(water.size() == 3);
        for (const RecordingGraphics::SpriteDraw_t& rSprite : water)
        {
            REQUIRE(rSprite.diamondTint.has_value());
            // The land itself sits at ocean level, the fixture's shallowest band (shade 0).
            CHECK(ColorEq_(rSprite.diamondTint->center, rShelf[0]));
            // Three of the N corner's four tiles are at the shelf line: (3 · -2000 + 0) / 4 is
            // the fixture's third band (shade 1).
            CHECK(ColorEq_(rSprite.diamondTint->north, rShelf[1]));
            CHECK(ColorEq_(rSprite.diamondTint->east, rShelf[0]));
            CHECK(ColorEq_(rSprite.diamondTint->south, rShelf[0]));
        }

        // The land's N and E corners are the W and S corners of the water across its NE edge.
        RecordingGraphics waterGraphics;
        TileRenderer::Render(waterGraphics, *world.map.GetTile(2, 1), k_X, k_Y, k_Size,
                             /*bFogged*/ false, &world.map);
        const std::ptrdiff_t shelf = FirstSpriteIndex_(waterGraphics, shelfPath);
        REQUIRE(shelf >= 0);
        const std::optional<DiamondTint_t>& rWaterTint =
            waterGraphics.sprites[static_cast<std::size_t>(shelf)].diamondTint;
        REQUIRE(rWaterTint.has_value());
        CHECK(ColorEq_(rWaterTint->west, water.front().diamondTint->north));
        CHECK(ColorEq_(rWaterTint->south, water.front().diamondTint->east));
    }

    SECTION("fog dims land art, hazes the terrain, and leaves water shading and objects clear")
    {
        Tile& rWater = *world.map.GetTile(2, 1);
        rWater.SetElevation(shelfMeters);
        rLand.AddImprovement(world.improvements.Get("Mine"));
        const auto& s = Style().tileRenderer;
        const std::string& minePath = world.improvements.Get("Mine").spritePaths.land.front();
        const auto dimmed = static_cast<std::uint8_t>(std::lround(255.0f * s.fogTerrainDimRatio));
        const Color_t fogTint{dimmed, dimmed, dimmed, 255};
        const auto hazes = [&s](const RecordingGraphics& rGraphics) {
            std::vector<RecordingGraphics::RectDraw_t> haze;
            for (const RecordingGraphics::RectDraw_t& rRect : rGraphics.rects)
            {
                if (rRect.bFilled && ColorEq_(rRect.color, s.fogHazeColor))
                {
                    haze.push_back(rRect);
                }
            }
            return haze;
        };

        RecordingGraphics lit;
        TileRenderer::Render(lit, rLand, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);
        RecordingGraphics fogged;
        TileRenderer::Render(fogged, rLand, k_X, k_Y, k_Size, /*bFogged*/ true, &world.map);

        const std::ptrdiff_t litMoist = FirstSpriteIndex_(lit, moistPath);
        const std::ptrdiff_t foggedMoist = FirstSpriteIndex_(fogged, moistPath);
        REQUIRE(litMoist >= 0);
        REQUIRE(foggedMoist >= 0);
        CHECK(ColorEq_(lit.sprites[static_cast<std::size_t>(litMoist)].tint, Color_t::White()));
        CHECK(ColorEq_(fogged.sprites[static_cast<std::size_t>(foggedMoist)].tint, fogTint));
        for (const RecordingGraphics::SpriteDraw_t& rSprite : CoastSprites_(fogged, "shore_"))
        {
            CHECK(ColorEq_(rSprite.tint, fogTint));
        }

        const std::vector<RecordingGraphics::SpriteDraw_t> litWater = CoastSprites_(lit, "water_");
        const std::vector<RecordingGraphics::SpriteDraw_t> foggedWater =
            CoastSprites_(fogged, "water_");
        REQUIRE(litWater.size() == foggedWater.size());
        for (std::size_t i = 0; i < litWater.size(); ++i)
        {
            REQUIRE(foggedWater[i].diamondTint.has_value());
            CHECK(ColorEq_(foggedWater[i].diamondTint->center, litWater[i].diamondTint->center));
            CHECK(ColorEq_(foggedWater[i].diamondTint->north, litWater[i].diamondTint->north));
        }

        CHECK(hazes(lit).empty());
        const std::vector<RecordingGraphics::RectDraw_t> haze = hazes(fogged);
        REQUIRE(haze.size() == 1);
        CHECK(haze.front().x == k_X);
        CHECK(haze.front().width == k_Size);
        const std::ptrdiff_t lastCoast =
            LastSpriteIndex_(fogged, Style().tileRenderer.coastSpriteDir + "/");
        const std::ptrdiff_t mine = FirstSpriteIndex_(fogged, minePath);
        REQUIRE(lastCoast >= 0);
        REQUIRE(mine >= 0);
        CHECK(fogged.sprites[static_cast<std::size_t>(foggedMoist)].order < haze.front().order);
        CHECK(fogged.sprites[static_cast<std::size_t>(lastCoast)].order < haze.front().order);
        CHECK(haze.front().order < fogged.sprites[static_cast<std::size_t>(mine)].order);
        for (const RecordingGraphics::SpriteDraw_t& rSprite : fogged.sprites)
        {
            if (rSprite.textureId == minePath)
            {
                CHECK(ColorEq_(rSprite.tint, Color_t::White()));
            }
        }

        RecordingGraphics litSea;
        TileRenderer::Render(litSea, rWater, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);
        RecordingGraphics foggedSea;
        TileRenderer::Render(foggedSea, rWater, k_X, k_Y, k_Size, /*bFogged*/ true, &world.map);
        const std::ptrdiff_t litShelf = FirstSpriteIndex_(litSea, shelfPath);
        const std::ptrdiff_t foggedShelf = FirstSpriteIndex_(foggedSea, shelfPath);
        REQUIRE(litShelf >= 0);
        REQUIRE(foggedShelf >= 0);
        const std::optional<DiamondTint_t>& rLitShelf =
            litSea.sprites[static_cast<std::size_t>(litShelf)].diamondTint;
        const std::optional<DiamondTint_t>& rFoggedShelf =
            foggedSea.sprites[static_cast<std::size_t>(foggedShelf)].diamondTint;
        REQUIRE(rLitShelf.has_value());
        REQUIRE(rFoggedShelf.has_value());
        CHECK(ColorEq_(rFoggedShelf->center, rLitShelf->center));
        CHECK(hazes(foggedSea).size() == 1);
    }

    SECTION("a one-tile island on an even row uses the regular all-water art")
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx != 0 || dy != 0)
                {
                    world.map.GetTile(2 + dx, 2 + dy)->SetElevation(shelfMeters);
                }
            }
        }

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);

        const std::vector<RecordingGraphics::SpriteDraw_t> water = CoastSprites_(graphics, "water_");
        REQUIRE(water.size() == 4);
        CHECK(water[0].textureId == CoastPath_("water", 'w', "7"));
        CHECK(water[1].textureId == CoastPath_("water", 'n', "7"));
        CHECK(water[2].textureId == CoastPath_("water", 'e', "7"));
        CHECK(water[3].textureId == CoastPath_("water", 's', "7"));
    }

    SECTION("a one-tile island on an odd row uses the alternate all-water art")
    {
        Tile& rIsland = *world.map.GetTile(2, 1);
        rIsland.SetElevation(500);
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx != 0 || dy != 0)
                {
                    world.map.GetTile(2 + dx, 1 + dy)->SetElevation(shelfMeters);
                }
            }
        }

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rIsland, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);

        const std::vector<RecordingGraphics::SpriteDraw_t> shore = CoastSprites_(graphics, "shore_");
        REQUIRE(shore.size() == 4);
        CHECK(shore[0].textureId == CoastPath_("shore", 'w', "7_alt"));
        CHECK(shore[1].textureId == CoastPath_("shore", 'n', "7_alt"));
        CHECK(shore[2].textureId == CoastPath_("shore", 'e', "7_alt"));
        CHECK(shore[3].textureId == CoastPath_("shore", 's', "7_alt"));
    }

    SECTION("no coast without a map, on water tiles, or inland")
    {
        Tile& rWater = *world.map.GetTile(2, 1);
        rWater.SetElevation(shelfMeters);

        RecordingGraphics noMap;
        TileRenderer::Render(noMap, rLand, k_X, k_Y, k_Size, /*bFogged*/ false);
        CHECK(CoastSprites_(noMap).empty());

        RecordingGraphics waterTile;
        TileRenderer::Render(waterTile, rWater, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);
        CHECK(CoastSprites_(waterTile).empty());

        RecordingGraphics inland;
        TileRenderer::Render(inland, *world.map.GetTile(2, 3), k_X, k_Y, k_Size,
                             /*bFogged*/ false, &world.map);
        CHECK(CoastSprites_(inland).empty());
    }
}

TEST_CASE("TileRenderer shades water art per vertex by depth", "[ui][tile][water]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world(5, 5);
    WriteOccupantStubs_(world.improvements.Get("OceanShelf"));
    WriteOccupantStubs_(world.improvements.Get("Ocean"));
    const ElevationRulesConfig_t& rRules = actest::TestMapRules();
    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            world.map.GetTile(x, y)->SetElevation(rRules.oceanShelfMeters);
        }
    }
    const WaterShadingStyle_t& rShading = Style().tileRenderer.waterShading;
    Tile& rTile = *world.map.GetTile(2, 2);

    constexpr float k_X = 10.0f;
    constexpr float k_Y = 20.0f;
    constexpr float k_Size = 100.0f;

    const auto waterTint = [&](const std::string& path) {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);
        const std::ptrdiff_t index = FirstSpriteIndex_(graphics, path);
        REQUIRE(index >= 0);
        const RecordingGraphics::SpriteDraw_t& rSprite =
            graphics.sprites[static_cast<std::size_t>(index)];
        CHECK(rSprite.x == k_X);
        CHECK(rSprite.y == k_Y);
        CHECK(rSprite.destWidth == k_Size);
        CHECK(rSprite.destHeight == k_Size * 0.5f);
        REQUIRE(rSprite.diamondTint.has_value());
        return *rSprite.diamondTint;
    };

    SECTION("the centre takes its own depth and each corner the depths of the tiles sharing it")
    {
        // Shallower water around the N corner.
        for (const auto& [x, y] : {std::pair{1, 2}, std::pair{1, 1}, std::pair{2, 1}})
        {
            world.map.GetTile(x, y)->SetElevation(rRules.oceanLevelMeters - 500);
        }
        const DiamondTint_t tint =
            waterTint(world.improvements.Get("OceanShelf").spritePaths.sea.front());
        const std::vector<Color_t>& rTints = rShading.tints.at("OceanShelf");
        // The shelf line is the fixture's third band (shade 1). The N corner averages one tile
        // there and three in the last band, which lands in the last band (shade 0); the other
        // corners average three or four at the shelf line.
        CHECK(ColorEq_(tint.center, rTints[1]));
        CHECK(ColorEq_(tint.north, rTints[0]));
        CHECK(ColorEq_(tint.west, rTints[1]));
        CHECK(ColorEq_(tint.east, rTints[1]));
        CHECK(ColorEq_(tint.south, rTints[1]));
    }

    SECTION("ocean water takes the Ocean landform's tints")
    {
        rTile.SetElevation(rRules.minElevationMeters);
        REQUIRE(rTile.HasFeature("Ocean"));
        const DiamondTint_t tint =
            waterTint(world.improvements.Get("Ocean").spritePaths.sea.front());
        const std::vector<Color_t>& rTints = rShading.tints.at("Ocean");
        // The floor is the first band (shade 3); a corner with three tiles at the shelf line
        // averages into the second (shade 2).
        CHECK(ColorEq_(tint.center, rTints[3]));
        CHECK(ColorEq_(tint.north, rTints[2]));
    }
}

TEST_CASE("TileRenderer draws an occupant's art for the tile's surface", "[ui][tile]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world(5, 5);
    for (const char* id : {"Moist", "Fungus", "OceanShelf", "Nutrients"})
    {
        WriteOccupantStubs_(world.improvements.Get(id));
    }
    const OccupantSpriteTiles_t& fungusTiles = *world.improvements.Get("Fungus").spriteTiles;
    const OccupantSpritePaths_t fungus{{TilePrefix_(fungusTiles.land)},
                                       {TilePrefix_(fungusTiles.sea)}};
    const OccupantSpritePaths_t& nutrients = world.improvements.Get("Nutrients").spritePaths;
    const auto drew = [](const RecordingGraphics& rGraphics, const std::string& prefix) {
        return std::ranges::any_of(rGraphics.sprites,
                                   [&prefix](const RecordingGraphics::SpriteDraw_t& rSprite) {
                                       return rSprite.textureId.starts_with(prefix);
                                   });
    };

    Tile& rLand = *world.map.GetTile(2, 3);
    rLand.SetElevation(500);
    Tile& rSea = *world.map.GetTile(2, 1);
    rSea.SetElevation(actest::TestMapRules().oceanShelfMeters);
    for (Tile* pTile : {&rLand, &rSea})
    {
        pTile->AddTerrainFeature(world.improvements.Get("Fungus"));
        pTile->AddTerrainFeature(world.improvements.Get("Nutrients"));
    }

    RecordingGraphics onLand;
    TileRenderer::Render(onLand, rLand, 0.0f, 0.0f, 100.0f, /*bFogged*/ false, &world.map);
    CHECK(drew(onLand, fungus.land.front()));
    CHECK_FALSE(drew(onLand, fungus.sea.front()));
    CHECK(drew(onLand, nutrients.land.front()));
    CHECK_FALSE(drew(onLand, nutrients.sea.front()));

    RecordingGraphics atSea;
    TileRenderer::Render(atSea, rSea, 0.0f, 0.0f, 100.0f, /*bFogged*/ false, &world.map);
    CHECK(drew(atSea, fungus.sea.front()));
    CHECK_FALSE(drew(atSea, fungus.land.front()));
    CHECK(drew(atSea, nutrients.sea.front()));
    CHECK_FALSE(drew(atSea, nutrients.land.front()));
}

TEST_CASE("Object sprites stand on the tile's footprint and reach above it", "[ui][tile]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world(5, 5);
    const ImprovementConfig_t& rNutrients = world.improvements.Get("Nutrients");
    WriteOccupantStubs_(rNutrients);
    REQUIRE(rNutrients.spriteOverhangRatio > 0.0f);

    Tile& rTile = *world.map.GetTile(2, 2);
    rTile.SetElevation(500);
    rTile.AddTerrainFeature(rNutrients);

    constexpr float k_X = 10.0f;
    constexpr float k_Y = 20.0f;
    constexpr float k_Size = 100.0f;
    constexpr float k_Height = k_Size * 0.5f;
    RecordingGraphics graphics;
    TileRenderer::Render(graphics, rTile, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);

    const std::string& path = rNutrients.spritePaths.land.front();
    const auto it = std::ranges::find_if(graphics.sprites,
                                         [&path](const RecordingGraphics::SpriteDraw_t& rSprite) {
                                             return rSprite.textureId == path;
                                         });
    REQUIRE(it != graphics.sprites.end());
    const float overhang = k_Height * rNutrients.spriteOverhangRatio;
    // The bottom of the sprite is the tile's bottom; the extra height goes above the tile.
    CHECK_THAT(it->x, WithinAbs(k_X, 0.001f));
    CHECK_THAT(it->destWidth, WithinAbs(k_Size, 0.001f));
    CHECK_THAT(it->y, WithinAbs(k_Y - overhang, 0.001f));
    CHECK_THAT(it->y + it->destHeight, WithinAbs(k_Y + k_Height, 0.001f));
    CHECK_FALSE(it->diamondTint.has_value());
}

TEST_CASE("TileRenderer picks tile-set sprites from the tile's neighbors", "[ui][tile][autotile]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world(5, 5);
    for (const char* id : {"Moist", "Fungus", "Forest", "River", "MonsoonJungle"})
    {
        WriteOccupantStubs_(world.improvements.Get(id));
    }
    const auto tilePath = [&world](const char* id, unsigned mask) {
        return TilePath_(world.improvements.Get(id).spriteTiles->land, mask);
    };
    const auto tilePrefix = [&world](const char* id) {
        return TilePrefix_(world.improvements.Get(id).spriteTiles->land);
    };
    const auto countDrawn = [](const RecordingGraphics& rGraphics, const std::string& prefix) {
        return std::ranges::count_if(rGraphics.sprites,
                                     [&prefix](const RecordingGraphics::SpriteDraw_t& rSprite) {
                                         return rSprite.textureId.starts_with(prefix);
                                     });
    };

    for (int y = 1; y <= 3; ++y)
    {
        for (int x = 1; x <= 3; ++x)
        {
            world.map.GetTile(x, y)->SetElevation(500);
            SetMoisture_(*world.map.GetTile(x, y), Moisture_t::Moist);
        }
    }
    Tile& rTile = *world.map.GetTile(2, 2);
    const auto render = [&world, &rTile]() {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, 0.0f, 0.0f, 100.0f, /*bFogged*/ false, &world.map);
        return graphics;
    };

    SECTION("a moist tile among moist land uses the fully connected cell")
    {
        CHECK(countDrawn(render(), tilePath("Moist", 255)) == 1);
    }

    SECTION("water and wetter land connect like moist land")
    {
        SetMoisture_(*world.map.GetTile(2, 1), Moisture_t::Wet);
        world.map.GetTile(3, 2)->SetElevation(-500);
        CHECK(countDrawn(render(), tilePath("Moist", 255)) == 1);
    }

    SECTION("moisture fades out toward drier land across an edge")
    {
        // The NE edge (blob bit 1) drops, and with it the N and E corners (bits 0 and 2).
        SetMoisture_(*world.map.GetTile(2, 1), Moisture_t::Arid);
        CHECK(countDrawn(render(), tilePath("Moist", 248)) == 1);
    }

    SECTION("a moist tile among drier land is an isolated patch")
    {
        for (int y = 1; y <= 3; ++y)
        {
            for (int x = 1; x <= 3; ++x)
            {
                SetMoisture_(*world.map.GetTile(x, y), Moisture_t::Arid);
            }
        }
        SetMoisture_(rTile, Moisture_t::Moist);
        CHECK(countDrawn(render(), tilePath("Moist", 0)) == 1);
    }

    SECTION("an arid tile draws no moist tile")
    {
        SetMoisture_(rTile, Moisture_t::Arid);
        CHECK(countDrawn(render(), tilePrefix("Moist")) == 0);
    }

    SECTION("forest connects across edges, not corners")
    {
        for (const auto& [x, y] : {std::pair{2, 2}, std::pair{3, 2}, std::pair{3, 3}})
        {
            world.map.GetTile(x, y)->AddImprovement(world.improvements.Get("Forest"));
        }
        // (3, 2) is the SE edge (edge bit 1); (3, 3) only touches the S corner.
        CHECK(countDrawn(render(), tilePath("Forest", 2)) == 1);
    }

    SECTION("fungus counts a corner between two fungus edges")
    {
        for (const auto& [x, y] : {std::pair{2, 2}, std::pair{2, 1}, std::pair{3, 1}, std::pair{3, 2}})
        {
            world.map.GetTile(x, y)->AddTerrainFeature(world.improvements.Get("Fungus"));
        }
        // NE edge (bit 1) + E corner (bit 2) + SE edge (bit 3).
        CHECK(countDrawn(render(), tilePath("Fungus", 14)) == 1);
    }

    SECTION("jungle draws once, through the landmark layer")
    {
        rTile.AddTerrainFeature(world.improvements.Get("MonsoonJungle"));
        const RecordingGraphics graphics = render();
        CHECK(countDrawn(graphics, tilePrefix("MonsoonJungle")) == 1);
        CHECK(countDrawn(graphics, tilePath("MonsoonJungle", 0)) == 1);
    }

    SECTION("a river draws the cell for its connections")
    {
        rTile.SetHasRiver(true);
        world.map.GetTile(2, 1)->SetHasRiver(true); // north: edge bit 0
        world.map.GetTile(1, 2)->SetHasRiver(true); // west: edge bit 3
        const RecordingGraphics graphics = render();
        CHECK(countDrawn(graphics, tilePath("River", 9)) == 1);
        CHECK(graphics.lines.empty());
    }
}

TEST_CASE("A river without art falls back to lines toward its connections", "[ui][tile]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world(5, 5);
    world.map.GetTile(2, 1)->SetHasRiver(true);

    // Not bound to the occupant registry, so no river art resolves.
    Tile tile(2, 2);
    tile.BindMapRules(actest::TestMapRules());
    tile.SetElevation(500);
    tile.SetHasRiver(true);

    constexpr float k_X = 10.0f;
    constexpr float k_Y = 20.0f;
    constexpr float k_Size = 100.0f;

    SECTION("a line from the centre to each connected edge")
    {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, k_X, k_Y, k_Size, /*bFogged*/ false, &world.map);
        REQUIRE(graphics.lines.size() == 1);
        // North on the grid is the diamond's NE edge, whose midpoint is at (¾ w, ¼ h).
        CHECK_THAT(graphics.lines[0].x1, WithinAbs(k_X + 50.0f, 0.001f));
        CHECK_THAT(graphics.lines[0].y1, WithinAbs(k_Y + 25.0f, 0.001f));
        CHECK_THAT(graphics.lines[0].x2, WithinAbs(k_X + 75.0f, 0.001f));
        CHECK_THAT(graphics.lines[0].y2, WithinAbs(k_Y + 12.5f, 0.001f));
    }

    SECTION("a cross when no neighbor has a river")
    {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, k_X, k_Y, k_Size, /*bFogged*/ false);
        CHECK(graphics.lines.size() == 2);
    }
}
