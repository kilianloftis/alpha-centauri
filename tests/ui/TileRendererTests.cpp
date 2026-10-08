#include "GameFixtures.h"
#include "RecordingGraphics.h"
#include "StubSprites.h"
#include "TestHelpers.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/TileShapeGeometry.h"
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
#include <tuple>
#include <utility>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;
using Catch::Matchers::WithinAbs;

namespace
{

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

// Materialize every path in the list so a prior Missing cache entry cannot poison a later
// section in the same process (SpriteCache_ is process-wide).
void WriteStubPngs_(const std::vector<std::string>& paths)
{
    for (const std::string& path : paths)
    {
        if (!path.empty())
        {
            actest::WriteStubPng(path);
        }
    }
}

// Tile art draws through the style's palette, so its stub is written with the style.
void EnsureStyleLoaded_()
{
    static const bool bLoaded = []
    {
        UiStyle::Load(actest::FixturePath("ui/style.json"));
        actest::WriteStubPng(Style().tileRenderer.palettePath);
        return true;
    }();
    (void)bLoaded;
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
            actest::WriteStubPng(TilePath_(*pPattern, mask));
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
                actest::WriteStubPng(CoastPath_(part, corner, caseName));
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

// The shape the renderer gives this tile's own tile sprite whose path starts with prefix.
TileShape_t DrawnShape_(const Tile& rTile, const WorldMap& rMap, const std::string& prefix,
                        bool bFogged)
{
    RecordingGraphics graphics;
    TileRenderer::Render(graphics, rTile, FlatTileShape(0.0f, 0.0f, 64.0f), bFogged, &rMap);
    for (const RecordingGraphics::SpriteDraw_t& rSprite : graphics.sprites)
    {
        if (rSprite.textureId.starts_with(prefix) && rSprite.shape.has_value())
        {
            return *rSprite.shape;
        }
    }
    FAIL("sprite not drawn: " << prefix);
    return TileShape_t{};
}

// Every tile one lattice step from rCentre, at the given elevation.
void SurroundWith_(actest::WorldFixture& rWorld, const Tile& rCentre, int elevation)
{
    for (int q = -1; q <= 1; ++q)
    {
        for (int p = -1; p <= 1; ++p)
        {
            if (p != 0 || q != 0)
            {
                GetTileAtLatticeOffset(rWorld.map, rCentre, p, q)->SetElevation(elevation);
            }
        }
    }
}

} // namespace

TEST_CASE("Missing terrain art leaves only the fill; objects draw the checker", "[ui][tile]")
{
    EnsureStyleLoaded_();
    const auto& s = Style().tileRenderer;

    SECTION("moist rocky land without sprites draws only the fill")
    {
        Tile tile(0, 0);
        tile.BindMapRules(actest::TestMapRules());
        tile.SetElevation(500);
        tile.SetMoisture(Moisture_t::Moist);
        tile.SetRockiness(Rockiness_t::Rocky);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, FlatTileShape(10.0f, 20.0f, 100.0f),
                             /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        CHECK(graphics.sprites.empty());
        REQUIRE(graphics.rects.size() == 1);
        CHECK(graphics.rects.front().bFilled);
        CHECK(ColorEq_(graphics.rects.front().color, TileRenderer::FillColor(tile, false)));
    }

    SECTION("water keeps the elevation fill")
    {
        Tile tile(2, 2);
        tile.BindMapRules(actest::TestMapRules());
        tile.SetElevation(-1500);
        tile.SetMoisture(Moisture_t::Wet);
        tile.SetRockiness(Rockiness_t::Rocky);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, FlatTileShape(0.0f, 0.0f, 64.0f),
                             /*bFogged*/ false);

        CHECK(graphics.texts.empty());
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

    SECTION("arid flat land is fill only")
    {
        Tile tile(3, 3);
        tile.BindMapRules(actest::TestMapRules());
        tile.SetElevation(100);
        tile.SetMoisture(Moisture_t::Arid);
        tile.SetRockiness(Rockiness_t::Flat);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, FlatTileShape(0.0f, 0.0f, 50.0f),
                             /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        REQUIRE(graphics.rects.size() == 1);
        CHECK(ColorEq_(graphics.rects.front().color, TileRenderer::FillColor(tile, false)));
    }

    SECTION("fungus overlays terrain with its sprite when present")
    {
        actest::WorldFixture world;
        // Moist/Rocky layers are also resolved on this tile — stub them before Render so a
        // Missing SpriteCache_ entry cannot poison later sections in this process.
        WriteOccupantStubs_(world.improvements.Get("Moist"));
        WriteOccupantStubs_(world.improvements.Get("Rocky"));
        WriteOccupantStubs_(world.improvements.Get("Fungus"));
        REQUIRE(world.improvements.Get("Fungus").spriteTiles.has_value());
        // No map: every tile set uses its mask-0 sprite.
        const std::string fungusPath =
            TilePath_(world.improvements.Get("Fungus").spriteTiles->land, 0);

        Tile& rTile = *world.map.GetTile(8, 2);
        rTile.SetElevation(500);
        rTile.SetMoisture(Moisture_t::Moist);
        rTile.SetRockiness(Rockiness_t::Rocky);
        rTile.AddTerrainFeature(world.improvements.Get("Fungus"));

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, FlatTileShape(0.0f, 0.0f, 64.0f),
                             /*bFogged*/ false);

        REQUIRE_FALSE(graphics.rects.empty());
        CHECK(graphics.rects.front().bFilled);
        CHECK(ColorEq_(graphics.rects.front().color, TileRenderer::FillColor(rTile, false)));
        bool bFungusSprite = false;
        for (const RecordingGraphics::SpriteDraw_t& rSprite : graphics.sprites)
        {
            if (rSprite.textureId == fungusPath)
            {
                bFungusSprite = true;
                REQUIRE(rSprite.shape.has_value());
                CHECK(rSprite.shape->center.shade == 0.0f);
            }
        }
        CHECK(bFungusSprite);
    }

    SECTION("the moisture base fills the whole tile")
    {
        // Bound tiles resolve Moist via the registry; ensure the configured PNGs exist so CI
        // without a prior extract_terrain.py run still exercises the sprite path.
        actest::WorldFixture world;
        const ImprovementConfig_t& rMoist = world.improvements.Get("Moist");
        WriteOccupantStubs_(rMoist);
        const std::string moistPrefix = TilePrefix_(rMoist.spriteTiles->land);

        Tile& rTile = *world.map.GetTile(8, 4);
        rTile.SetElevation(500);
        rTile.SetMoisture(Moisture_t::Moist);
        rTile.SetRockiness(Rockiness_t::Flat);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, FlatTileShape(10.0f, 20.0f, 100.0f), /*bFogged*/ false,
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
        CHECK(rSprite.shape.has_value());
    }

    SECTION("land art draws as painted at any elevation")
    {
        actest::WorldFixture world;
        const ImprovementConfig_t& rMoist = world.improvements.Get("Moist");
        WriteOccupantStubs_(rMoist);
        Tile& rTile = *world.map.GetTile(8, 4);
        SetMoisture_(rTile, Moisture_t::Moist);
        for (const int elevation : {1, actest::TestMapRules().maxElevationMeters})
        {
            CAPTURE(elevation);
            rTile.SetElevation(elevation);
            CHECK(DrawnShape_(rTile, world.map, TilePrefix_(rMoist.spriteTiles->land),
                              /*bFogged*/ false)
                      .center.shade
                  == 0.0f);
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

    actest::WorldFixture world;
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

    Tile& rLand = *world.map.GetTile(8, 4);
    rLand.SetElevation(500);
    SetMoisture_(rLand, Moisture_t::Moist);

    constexpr float k_X = 10.0f;
    constexpr float k_Y = 20.0f;
    constexpr float k_Size = 100.0f;

    SECTION("water across the NE edge draws the north and east corners at the tile rect")
    {
        world.map.GetTile(9, 3)->SetElevation(shelfMeters);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);

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
        world.map.GetTile(9, 3)->SetElevation(shelfMeters);
        rLand.AddTerrainFeature(world.improvements.Get("Fungus"));
        rLand.AddImprovement(world.improvements.Get("Mine"));

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);

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
        world.map.GetTile(9, 3)->SetElevation(shelfMeters);
        rLand.AddTerrainFeature(world.improvements.Get("MonsoonJungle"));
        rLand.AddTerrainFeature(world.improvements.Get("Fungus"));
        rLand.AddTerrainFeature(world.improvements.Get("Nutrients"));
        rLand.SetHasRiver(true);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);

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
             {std::pair{7, 3}, std::pair{8, 2}, std::pair{9, 3}, std::pair{10, 4}})
        {
            world.map.GetTile(wx, wy)->SetElevation(shelfMeters);
        }

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);

        const std::vector<RecordingGraphics::SpriteDraw_t> water = CoastSprites_(graphics, "water_");
        REQUIRE(water.size() == 3);
        for (const RecordingGraphics::SpriteDraw_t& rSprite : water)
        {
            REQUIRE(rSprite.shape.has_value());
            // The land itself sits at ocean level, the fixture's shallowest band (shade 0).
            CHECK(rSprite.shape->center.shade == 0.0f);
            // Three of the N corner's four tiles are at the shelf line: (3 · -2000 + 0) / 4 is
            // the fixture's third band (shade 1), within the shelf's range.
            CHECK(rSprite.shape->north.shade == 1.0f);
            CHECK(rSprite.shape->east.shade == 0.0f);
            CHECK(rSprite.shape->south.shade == 0.0f);
        }

        // The land's N and E corners are the W and S corners of the water across its NE edge.
        RecordingGraphics waterGraphics;
        TileRenderer::Render(waterGraphics, *world.map.GetTile(9, 3), FlatTileShape(k_X, k_Y, k_Size),
                             /*bFogged*/ false, &world.map);
        const std::ptrdiff_t shelf = FirstSpriteIndex_(waterGraphics, shelfPath);
        REQUIRE(shelf >= 0);
        const std::optional<TileShape_t>& rWaterShape =
            waterGraphics.sprites[static_cast<std::size_t>(shelf)].shape;
        REQUIRE(rWaterShape.has_value());
        CHECK(rWaterShape->west.shade == water.front().shape->north.shade);
        CHECK(rWaterShape->south.shade == water.front().shape->east.shade);
    }

    SECTION("fog shades land art, hazes the terrain, and leaves water shading and objects clear")
    {
        Tile& rWater = *world.map.GetTile(9, 3);
        rWater.SetElevation(shelfMeters);
        rLand.AddImprovement(world.improvements.Get("Mine"));
        const auto& s = Style().tileRenderer;
        const std::string& minePath = world.improvements.Get("Mine").spritePaths.land.front();
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
        TileRenderer::Render(lit, rLand, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);
        RecordingGraphics fogged;
        TileRenderer::Render(fogged, rLand, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ true, &world.map);

        const std::ptrdiff_t litMoist = FirstSpriteIndex_(lit, moistPath);
        const std::ptrdiff_t foggedMoist = FirstSpriteIndex_(fogged, moistPath);
        REQUIRE(litMoist >= 0);
        REQUIRE(foggedMoist >= 0);
        const auto shadeOf = [](const RecordingGraphics& rGraphics, std::ptrdiff_t index) {
            const std::optional<TileShape_t>& rShape =
                rGraphics.sprites[static_cast<std::size_t>(index)].shape;
            REQUIRE(rShape.has_value());
            return rShape->center.shade;
        };
        CHECK(shadeOf(lit, litMoist) == 0.0f);
        CHECK(shadeOf(fogged, foggedMoist) == s.fogLandShade);
        for (const RecordingGraphics::SpriteDraw_t& rSprite : CoastSprites_(lit, "shore_"))
        {
            REQUIRE(rSprite.shape.has_value());
            CHECK(rSprite.shape->center.shade == 0.0f);
        }
        for (const RecordingGraphics::SpriteDraw_t& rSprite : CoastSprites_(fogged, "shore_"))
        {
            REQUIRE(rSprite.shape.has_value());
            CHECK(rSprite.shape->center.shade == s.fogLandShade);
            CHECK(rSprite.shape->south.shade == s.fogLandShade);
        }

        const std::vector<RecordingGraphics::SpriteDraw_t> litWater = CoastSprites_(lit, "water_");
        const std::vector<RecordingGraphics::SpriteDraw_t> foggedWater =
            CoastSprites_(fogged, "water_");
        REQUIRE(litWater.size() == foggedWater.size());
        for (std::size_t i = 0; i < litWater.size(); ++i)
        {
            REQUIRE(foggedWater[i].shape.has_value());
            CHECK(foggedWater[i].shape->center.shade == litWater[i].shape->center.shade);
            CHECK(foggedWater[i].shape->north.shade == litWater[i].shape->north.shade);
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
        TileRenderer::Render(litSea, rWater, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);
        RecordingGraphics foggedSea;
        TileRenderer::Render(foggedSea, rWater, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ true, &world.map);
        const std::ptrdiff_t litShelf = FirstSpriteIndex_(litSea, shelfPath);
        const std::ptrdiff_t foggedShelf = FirstSpriteIndex_(foggedSea, shelfPath);
        REQUIRE(litShelf >= 0);
        REQUIRE(foggedShelf >= 0);
        const std::optional<TileShape_t>& rLitShelf =
            litSea.sprites[static_cast<std::size_t>(litShelf)].shape;
        const std::optional<TileShape_t>& rFoggedShelf =
            foggedSea.sprites[static_cast<std::size_t>(foggedShelf)].shape;
        REQUIRE(rLitShelf.has_value());
        REQUIRE(rFoggedShelf.has_value());
        CHECK(rFoggedShelf->center.shade == rLitShelf->center.shade);
        CHECK(hazes(foggedSea).size() == 1);
    }

    SECTION("a one-tile island on an even row uses the regular all-water art")
    {
        SurroundWith_(world, rLand, shelfMeters);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);

        const std::vector<RecordingGraphics::SpriteDraw_t> water = CoastSprites_(graphics, "water_");
        REQUIRE(water.size() == 4);
        CHECK(water[0].textureId == CoastPath_("water", 'w', "7"));
        CHECK(water[1].textureId == CoastPath_("water", 'n', "7"));
        CHECK(water[2].textureId == CoastPath_("water", 'e', "7"));
        CHECK(water[3].textureId == CoastPath_("water", 's', "7"));
    }

    SECTION("a one-tile island on an odd row uses the alternate all-water art")
    {
        Tile& rIsland = *world.map.GetTile(9, 3);
        rIsland.SetElevation(500);
        SurroundWith_(world, rIsland, shelfMeters);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rIsland, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);

        const std::vector<RecordingGraphics::SpriteDraw_t> shore = CoastSprites_(graphics, "shore_");
        REQUIRE(shore.size() == 4);
        CHECK(shore[0].textureId == CoastPath_("shore", 'w', "7_alt"));
        CHECK(shore[1].textureId == CoastPath_("shore", 'n', "7_alt"));
        CHECK(shore[2].textureId == CoastPath_("shore", 'e', "7_alt"));
        CHECK(shore[3].textureId == CoastPath_("shore", 's', "7_alt"));
    }

    SECTION("no coast without a map, on water tiles, or inland")
    {
        Tile& rWater = *world.map.GetTile(9, 3);
        rWater.SetElevation(shelfMeters);

        RecordingGraphics noMap;
        TileRenderer::Render(noMap, rLand, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false);
        CHECK(CoastSprites_(noMap).empty());

        RecordingGraphics waterTile;
        TileRenderer::Render(waterTile, rWater, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);
        CHECK(CoastSprites_(waterTile).empty());

        RecordingGraphics inland;
        TileRenderer::Render(inland, *world.map.GetTile(7, 5), FlatTileShape(k_X, k_Y, k_Size),
                             /*bFogged*/ false, &world.map);
        CHECK(CoastSprites_(inland).empty());
    }
}

TEST_CASE("TileRenderer shades water art per vertex by depth", "[ui][tile][water]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world;
    WriteOccupantStubs_(world.improvements.Get("OceanShelf"));
    WriteOccupantStubs_(world.improvements.Get("Ocean"));
    const ElevationRulesConfig_t& rRules = actest::TestMapRules();
    for (const auto& pTile : world.map.GetTiles())
    {
        pTile->SetElevation(rRules.oceanShelfMeters);
    }
    const WaterShadingStyle_t& rShading = Style().tileRenderer.waterShading;
    Tile& rTile = *world.map.GetTile(8, 4);

    constexpr float k_X = 10.0f;
    constexpr float k_Y = 20.0f;
    constexpr float k_Size = 100.0f;

    const auto waterShape = [&](const std::string& path) {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);
        const std::ptrdiff_t index = FirstSpriteIndex_(graphics, path);
        REQUIRE(index >= 0);
        const RecordingGraphics::SpriteDraw_t& rSprite =
            graphics.sprites[static_cast<std::size_t>(index)];
        CHECK(rSprite.x == k_X);
        CHECK(rSprite.y == k_Y);
        CHECK(rSprite.destWidth == k_Size);
        CHECK(rSprite.destHeight == k_Size * 0.5f);
        REQUIRE(rSprite.shape.has_value());
        return *rSprite.shape;
    };

    SECTION("the centre takes its own depth and each corner the depths of the tiles sharing it")
    {
        // Shallower water around the N corner.
        for (const auto& [x, y] : {std::pair{7, 3}, std::pair{8, 2}, std::pair{9, 3}})
        {
            world.map.GetTile(x, y)->SetElevation(rRules.oceanLevelMeters - 500);
        }
        const TileShape_t shape =
            waterShape(world.improvements.Get("OceanShelf").spritePaths.sea.front());
        // The shelf line is the fixture's third band (shade 1). The N corner averages one tile
        // there and three in the last band, which lands in the last band (shade 0); the other
        // corners average three or four at the shelf line.
        CHECK(shape.center.shade == 1.0f);
        CHECK(shape.north.shade == 0.0f);
        CHECK(shape.west.shade == 1.0f);
        CHECK(shape.east.shade == 1.0f);
        CHECK(shape.south.shade == 1.0f);
    }

    SECTION("ocean water takes the Ocean landform's shade range")
    {
        rTile.SetElevation(rRules.minElevationMeters);
        REQUIRE(rTile.HasFeature("Ocean"));
        const TileShape_t shape =
            waterShape(world.improvements.Get("Ocean").spritePaths.sea.front());
        const WaterShadeRange_t& rOcean = rShading.shades.at("Ocean");
        // The floor is the first band (shade 3); a corner with three tiles at the shelf line
        // averages into the second (shade 2). Both move by the range's offset.
        CHECK(shape.center.shade == static_cast<float>(3 + rOcean.offset));
        CHECK(shape.north.shade == static_cast<float>(2 + rOcean.offset));
    }

    SECTION("a shelf tile with a corner at the deep shade draws the deep art")
    {
        REQUIRE(rTile.HasFeature("OceanShelf"));
        // The N corner averages the shelf line with three tiles on the floor: the first band.
        for (const auto& [x, y] : {std::pair{7, 3}, std::pair{8, 2}, std::pair{9, 3}})
        {
            world.map.GetTile(x, y)->SetElevation(rRules.minElevationMeters);
        }
        const WaterShadeRange_t& rOcean = rShading.shades.at("Ocean");
        const TileShape_t shape =
            waterShape(world.improvements.Get("Ocean").spritePaths.sea.front());
        CHECK(shape.center.shade == static_cast<float>(std::max(1 + rOcean.offset, 0)));
        CHECK(shape.north.shade == static_cast<float>(std::min(3 + rOcean.offset, rOcean.max)));
    }

    SECTION("an ocean tile whose corners are all shallow draws the shelf art")
    {
        for (const auto& pTile : world.map.GetTiles())
        {
            pTile->SetElevation(rRules.oceanLevelMeters - 500);
        }
        rTile.SetElevation(rRules.oceanShelfMeters - 1);
        REQUIRE(rTile.HasFeature("Ocean"));
        const TileShape_t shape =
            waterShape(world.improvements.Get("OceanShelf").spritePaths.sea.front());
        // The centre sits in the second band (shade 2) but does not switch the art.
        CHECK(shape.center.shade == 2.0f);
        CHECK(shape.north.shade == 0.0f);
    }
}

TEST_CASE("TileRenderer draws an occupant's art for the tile's surface", "[ui][tile]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world;
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

    Tile& rLand = *world.map.GetTile(7, 5);
    rLand.SetElevation(500);
    Tile& rSea = *world.map.GetTile(9, 3);
    rSea.SetElevation(actest::TestMapRules().oceanShelfMeters);
    for (Tile* pTile : {&rLand, &rSea})
    {
        pTile->AddTerrainFeature(world.improvements.Get("Fungus"));
        pTile->AddTerrainFeature(world.improvements.Get("Nutrients"));
    }

    RecordingGraphics onLand;
    TileRenderer::Render(onLand, rLand, FlatTileShape(0.0f, 0.0f, 100.0f), /*bFogged*/ false, &world.map);
    CHECK(drew(onLand, fungus.land.front()));
    CHECK_FALSE(drew(onLand, fungus.sea.front()));
    CHECK(drew(onLand, nutrients.land.front()));
    CHECK_FALSE(drew(onLand, nutrients.sea.front()));

    RecordingGraphics atSea;
    TileRenderer::Render(atSea, rSea, FlatTileShape(0.0f, 0.0f, 100.0f), /*bFogged*/ false, &world.map);
    CHECK(drew(atSea, fungus.sea.front()));
    CHECK_FALSE(drew(atSea, fungus.land.front()));
    CHECK(drew(atSea, nutrients.sea.front()));
    CHECK_FALSE(drew(atSea, nutrients.land.front()));
}

TEST_CASE("Object sprites hang from the tile's seat as SMAC anchors them", "[ui][tile]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world;
    const ImprovementConfig_t& rNutrients = world.improvements.Get("Nutrients");
    WriteOccupantStubs_(rNutrients);
    REQUIRE(rNutrients.spriteOverhangRatio > 0.0f);

    Tile& rTile = *world.map.GetTile(8, 4);
    rTile.SetElevation(500);
    rTile.AddTerrainFeature(rNutrients);

    constexpr float k_X = 10.0f;
    constexpr float k_Y = 20.0f;
    constexpr float k_Size = 100.0f;
    constexpr float k_Height = k_Size * 0.5f;
    RecordingGraphics graphics;
    TileRenderer::Render(graphics, rTile, FlatTileShape(k_X, k_Y, k_Size), /*bFogged*/ false, &world.map);

    const std::string& path = rNutrients.spritePaths.land.front();
    const auto it = std::ranges::find_if(graphics.sprites,
                                         [&path](const RecordingGraphics::SpriteDraw_t& rSprite) {
                                             return rSprite.textureId == path;
                                         });
    REQUIRE(it != graphics.sprites.end());
    const float overhang = k_Height * rNutrients.spriteOverhangRatio;
    // The cell starts at the tile's top corner and its overhang hangs below the tile.
    CHECK_THAT(it->x, WithinAbs(k_X, 0.001f));
    CHECK_THAT(it->destWidth, WithinAbs(k_Size, 0.001f));
    CHECK_THAT(it->y, WithinAbs(k_Y, 0.001f));
    CHECK_THAT(it->y + it->destHeight, WithinAbs(k_Y + k_Height + overhang, 0.001f));
    CHECK_FALSE(it->shape.has_value());
}

TEST_CASE("TileRenderer picks tile-set sprites from the tile's neighbors", "[ui][tile][autotile]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world;
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

    Tile& rTile = *world.map.GetTile(8, 4);
    const auto forEachInBlock = [&](auto&& fn) {
        for (int q = -1; q <= 1; ++q)
        {
            for (int p = -1; p <= 1; ++p)
            {
                fn(*GetTileAtLatticeOffset(world.map, rTile, p, q));
            }
        }
    };
    forEachInBlock([](Tile& rBlockTile) {
        rBlockTile.SetElevation(500);
        SetMoisture_(rBlockTile, Moisture_t::Moist);
    });
    const auto render = [&world, &rTile]() {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, FlatTileShape(0.0f, 0.0f, 100.0f), /*bFogged*/ false, &world.map);
        return graphics;
    };

    SECTION("a moist tile among moist land uses the fully connected cell")
    {
        CHECK(countDrawn(render(), tilePath("Moist", 255)) == 1);
    }

    SECTION("water and wetter land connect like moist land")
    {
        SetMoisture_(*world.map.GetTile(9, 3), Moisture_t::Wet);
        world.map.GetTile(9, 5)->SetElevation(-500);
        CHECK(countDrawn(render(), tilePath("Moist", 255)) == 1);
    }

    SECTION("moisture fades out toward drier land across an edge")
    {
        // The NE edge (blob bit 1) drops, and with it the N and E corners (bits 0 and 2).
        SetMoisture_(*world.map.GetTile(9, 3), Moisture_t::Arid);
        CHECK(countDrawn(render(), tilePath("Moist", 248)) == 1);
    }

    SECTION("a moist tile among drier land is an isolated patch")
    {
        forEachInBlock([](Tile& rBlockTile) { SetMoisture_(rBlockTile, Moisture_t::Arid); });
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
        for (const auto& [x, y] : {std::pair{8, 4}, std::pair{9, 5}, std::pair{8, 6}})
        {
            world.map.GetTile(x, y)->AddImprovement(world.improvements.Get("Forest"));
        }
        // (9, 5) is the SE edge (edge bit 1); (8, 6) only touches the S corner.
        CHECK(countDrawn(render(), tilePath("Forest", 2)) == 1);
    }

    SECTION("fungus counts a corner between two fungus edges")
    {
        for (const auto& [x, y] :
             {std::pair{8, 4}, std::pair{9, 3}, std::pair{10, 4}, std::pair{9, 5}})
        {
            world.map.GetTile(x, y)->AddTerrainFeature(world.improvements.Get("Fungus"));
        }
        // NE edge (bit 1) + E corner (bit 2) + SE edge (bit 3).
        CHECK(countDrawn(render(), tilePath("Fungus", 14)) == 1);
    }

    SECTION("sea fungus shows on the shelf and joins only fungus that shows")
    {
        const ImprovementConfig_t& rFungus = world.improvements.Get("Fungus");
        const ElevationRulesConfig_t& rRules = actest::TestMapRules();
        // The tile and its NE neighbor (9, 3) on the shelf; its SE neighbor (9, 5) deeper.
        for (const auto& [x, y, elevation] : {std::tuple{8, 4, rRules.oceanShelfMeters},
                                              std::tuple{9, 3, rRules.oceanShelfMeters},
                                              std::tuple{9, 5, rRules.minElevationMeters}})
        {
            world.map.GetTile(x, y)->SetElevation(elevation);
            world.map.GetTile(x, y)->AddTerrainFeature(rFungus);
        }
        const std::string seaPrefix = TilePrefix_(rFungus.spriteTiles->sea);
        // Only the NE edge (bit 1) connects.
        CHECK(countDrawn(render(), TilePath_(rFungus.spriteTiles->sea, 2)) == 1);

        RecordingGraphics deep;
        TileRenderer::Render(deep, *world.map.GetTile(9, 5),
                             FlatTileShape(0.0f, 0.0f, 100.0f), /*bFogged*/ false,
                             &world.map);
        CHECK(countDrawn(deep, seaPrefix) == 0);
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
        world.map.GetTile(9, 3)->SetHasRiver(true); // north: edge bit 0
        world.map.GetTile(7, 3)->SetHasRiver(true); // west: edge bit 3
        const RecordingGraphics graphics = render();
        CHECK(countDrawn(graphics, tilePath("River", 9)) == 1);
        CHECK(graphics.lines.empty());
    }
}

TEST_CASE("A river without art draws nothing beyond the fill", "[ui][tile]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world;
    world.map.GetTile(9, 3)->SetHasRiver(true);

    // Not bound to the occupant registry, so no river art resolves.
    Tile tile(8, 4);
    tile.BindMapRules(actest::TestMapRules());
    tile.SetElevation(500);
    tile.SetHasRiver(true);

    RecordingGraphics graphics;
    TileRenderer::Render(graphics, tile, FlatTileShape(10.0f, 20.0f, 100.0f),
                         /*bFogged*/ false, &world.map);
    CHECK(graphics.lines.empty());
    CHECK(graphics.sprites.empty());
    REQUIRE_FALSE(graphics.rects.empty());
    CHECK(graphics.rects.front().bFilled);
    CHECK(ColorEq_(graphics.rects.front().color, TileRenderer::FillColor(tile, false)));
}

TEST_CASE("TileRenderer draws on the given shape and shades only land terrain in sight",
          "[ui][tile][relief]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world;
    for (const char* id : {"Moist", "OceanShelf", "Nutrients"})
    {
        WriteOccupantStubs_(world.improvements.Get(id));
    }
    const std::string moistPrefix = TilePrefix_(world.improvements.Get("Moist").spriteTiles->land);

    // A raised, shaded tile: the centre 30 px up and the N corner 10 px up.
    TileShape_t shape = FlatTileShape(10.0f, 20.0f, 100.0f);
    shape.center.y -= 30.0f;
    shape.north.y -= 10.0f;
    shape.center.shade = -1.5f;
    shape.west.shade = 0.5f;
    shape.north.shade = -1.0f;
    shape.east.shade = 0.25f;
    shape.south.shade = -0.5f;

    Tile& rLand = *world.map.GetTile(8, 4);
    rLand.SetElevation(500);
    SetMoisture_(rLand, Moisture_t::Moist);

    const auto drawnShape = [](const RecordingGraphics& rGraphics, const std::string& prefix) {
        const std::ptrdiff_t index = FirstSpriteIndex_(rGraphics, prefix);
        REQUIRE(index >= 0);
        const std::optional<TileShape_t>& rShape =
            rGraphics.sprites[static_cast<std::size_t>(index)].shape;
        REQUIRE(rShape.has_value());
        return *rShape;
    };

    SECTION("land terrain lands on the shape and carries its shades")
    {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, shape, /*bFogged*/ false, &world.map);
        const TileShape_t drawn = drawnShape(graphics, moistPrefix);
        CHECK(drawn.center.y == shape.center.y);
        CHECK(drawn.north.y == shape.north.y);
        CHECK(drawn.west.x == shape.west.x);
        CHECK(drawn.center.shade == -1.5f);
        CHECK(drawn.west.shade == 0.5f);
        CHECK(drawn.south.shade == -0.5f);
    }

    SECTION("every tile sprite draws through the style's palette")
    {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, shape, /*bFogged*/ false, &world.map);
        bool bTileSprite = false;
        for (const RecordingGraphics::SpriteDraw_t& rSprite : graphics.sprites)
        {
            if (rSprite.shape.has_value())
            {
                bTileSprite = true;
                CHECK(rSprite.paletteId == Style().tileRenderer.palettePath);
            }
        }
        CHECK(bTileSprite);
    }

    SECTION("object sprites stand on a flat footprint seated at the mean of the corners")
    {
        const ImprovementConfig_t& rNutrients = world.improvements.Get("Nutrients");
        rLand.AddTerrainFeature(rNutrients);
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rLand, shape, /*bFogged*/ false, &world.map);
        const std::ptrdiff_t index =
            FirstSpriteIndex_(graphics, rNutrients.spritePaths.land.front());
        REQUIRE(index >= 0);
        const RecordingGraphics::SpriteDraw_t& rSprite =
            graphics.sprites[static_cast<std::size_t>(index)];
        CHECK_FALSE(rSprite.shape.has_value());
        CHECK_THAT(rSprite.x, WithinAbs(10.0f, 0.001f));
        CHECK_THAT(rSprite.destWidth, WithinAbs(100.0f, 0.001f));
        // SMAC seats objects at the corners' mean height, not the raised centre; the cell starts
        // half a tile height above that.
        const float seatY = (shape.west.y + shape.north.y + shape.east.y + shape.south.y) / 4.0f;
        CHECK_THAT(rSprite.y, WithinAbs(seatY - 25.0f, 0.001f));
    }

    SECTION("terrain and objects draw in separate halves")
    {
        const ImprovementConfig_t& rNutrients = world.improvements.Get("Nutrients");
        rLand.AddTerrainFeature(rNutrients);
        const std::string& bonusPath = rNutrients.spritePaths.land.front();

        RecordingGraphics terrain;
        TileRenderer::RenderTerrain(terrain, rLand, shape, /*bFogged*/ false, &world.map);
        CHECK(FirstSpriteIndex_(terrain, moistPrefix) >= 0);
        CHECK(FirstSpriteIndex_(terrain, bonusPath) < 0);

        RecordingGraphics objects;
        TileRenderer::RenderObjects(objects, rLand, shape);
        CHECK(FirstSpriteIndex_(objects, bonusPath) >= 0);
        CHECK(FirstSpriteIndex_(objects, moistPrefix) < 0);
        CHECK(objects.rects.empty());
    }

    SECTION("fogged land draws at the fog shade, and water at its depth shades")
    {
        RecordingGraphics fogged;
        TileRenderer::Render(fogged, rLand, shape, /*bFogged*/ true, &world.map);
        const TileShape_t foggedLand = drawnShape(fogged, moistPrefix);
        CHECK(foggedLand.center.shade == Style().tileRenderer.fogLandShade);
        CHECK(foggedLand.north.shade == Style().tileRenderer.fogLandShade);

        Tile& rWater = *world.map.GetTile(9, 3);
        rWater.SetElevation(actest::TestMapRules().oceanShelfMeters);
        RecordingGraphics sea;
        TileRenderer::Render(sea, rWater, shape, /*bFogged*/ false, &world.map);
        const TileShape_t water =
            drawnShape(sea, world.improvements.Get("OceanShelf").spritePaths.sea.front());
        // The shelf line is the fixture's shade 1; the relief's shades do not reach water.
        CHECK(water.center.shade == 1.0f);
        CHECK(water.center.y == shape.center.y);
    }
}

TEST_CASE("TileRenderer draws road networks the way SMAC links them", "[ui][tile][roads]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world;
    const ImprovementConfig_t& rRoad = world.improvements.Get("Road");
    const ImprovementConfig_t& rTube = world.improvements.Get("MagTube");
    WriteOccupantStubs_(rRoad);
    WriteOccupantStubs_(rTube);
    for (const auto& pTile : world.map.GetTiles())
    {
        pTile->SetElevation(500);
    }
    const auto roadCell = [&rRoad](unsigned cell) { return TilePath_(rRoad.spriteTiles->land, cell); };
    const auto tubeCell = [&rTube](unsigned cell) { return TilePath_(rTube.spriteTiles->land, cell); };
    const auto drawn = [&world](int x, int y) {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, *world.map.GetTile(x, y),
                             FlatTileShape(0.0f, 0.0f, 100.0f), /*bFogged*/ false,
                             &world.map);
        std::vector<std::string> paths;
        for (const RecordingGraphics::SpriteDraw_t& rSprite : graphics.sprites)
        {
            paths.push_back(rSprite.textureId);
        }
        return paths;
    };
    const auto has = [](const std::vector<std::string>& rPaths, const std::string& rPath) {
        return std::ranges::find(rPaths, rPath) != rPaths.end();
    };
    Tile& rTile = *world.map.GetTile(8, 4);
    rTile.AddImprovement(rRoad);

    SECTION("a lone road draws its hub")
    {
        const auto paths = drawn(8, 4);
        CHECK(has(paths, roadCell(0)));
    }

    SECTION("each neighbor with a road draws one link cell and no hub")
    {
        // NE edge (0, -1) is SMAC direction 0, cell 3; the N corner (-1, -1) is direction 7,
        // cell 2.
        world.map.GetTile(9, 3)->AddImprovement(rRoad);
        world.map.GetTile(8, 2)->AddImprovement(rRoad);
        const auto paths = drawn(8, 4);
        CHECK(has(paths, roadCell(3)));
        CHECK(has(paths, roadCell(2)));
        CHECK_FALSE(has(paths, roadCell(0)));
    }

    SECTION("a base carries roads and never draws a hub")
    {
        // (9, 5) lies across the SE edge: direction 2, cell 5; seen from it, (8, 4) is across
        // its NW edge: direction 6, cell 1.
        world.map.GetTile(9, 5)->AddImprovement(world.improvements.Get("Base"));
        CHECK(has(drawn(8, 4), roadCell(5)));
        const auto basePaths = drawn(9, 5);
        CHECK(has(basePaths, roadCell(1)));
        CHECK_FALSE(has(basePaths, roadCell(0)));
    }

    SECTION("a mag tube link replaces the road link where both tiles carry tubes")
    {
        Tile& rNeighbor = *world.map.GetTile(9, 3);
        rNeighbor.AddImprovement(rRoad);
        rNeighbor.AddImprovement(rTube);
        rTile.AddImprovement(rTube);
        const auto paths = drawn(8, 4);
        CHECK(has(paths, tubeCell(3)));
        CHECK_FALSE(has(paths, roadCell(3)));
        CHECK_FALSE(has(paths, tubeCell(0)));
        CHECK_FALSE(has(paths, roadCell(0)));
    }

    SECTION("a tube with only road links draws its hub over the road links")
    {
        world.map.GetTile(9, 3)->AddImprovement(rRoad);
        rTile.AddImprovement(rTube);
        const auto paths = drawn(8, 4);
        CHECK(has(paths, roadCell(3)));
        CHECK(has(paths, tubeCell(0)));
    }

    SECTION("water neighbors carry no link")
    {
        Tile& rSea = *world.map.GetTile(9, 3);
        rSea.SetElevation(actest::TestMapRules().oceanShelfMeters);
        rSea.AddImprovement(world.improvements.Get("Base"));
        const auto paths = drawn(8, 4);
        CHECK_FALSE(has(paths, roadCell(3)));
        CHECK(has(paths, roadCell(0)));
    }
}

TEST_CASE("TileRenderer draws farms like SMAC: ground, structures by yield, and hidden structures",
          "[ui][tile][farm]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world;
    const ImprovementConfig_t& rFarm = world.improvements.Get("Farm");
    const ImprovementConfig_t& rMoist = world.improvements.Get("Moist");
    WriteOccupantStubs_(rMoist);
    WriteOccupantStubs_(world.improvements.Get("Condenser"));
    for (const auto& [rMoisture, rPaths] : rFarm.groundSprites)
    {
        WriteStubPngs_(rPaths);
    }
    WriteStubPngs_(rFarm.spriteYieldRows->paths.land);

    Tile& rTile = *world.map.GetTile(8, 4);
    rTile.SetElevation(500);
    SetMoisture_(rTile, Moisture_t::Moist);
    rTile.AddImprovement(rFarm);
    const auto render = [&](const TileRenderer::YieldLookup_t& rYieldOf) {
        RecordingGraphics graphics;
        TileRenderer::Render(graphics, rTile, FlatTileShape(0.0f, 0.0f, 100.0f),
                             /*bFogged*/ false, &world.map, rYieldOf);
        return graphics;
    };
    const auto nutrients = [](int amount) {
        return TileRenderer::YieldLookup_t([amount](const Tile&) {
            return TileResources_t{amount, 0, 0};
        });
    };
    const std::vector<std::string>& rRows = rFarm.spriteYieldRows->paths.land;

    SECTION("the farm ground replaces the moisture base")
    {
        const RecordingGraphics graphics = render(nutrients(1));
        const std::vector<std::string>& rGround = rFarm.groundSprites.at("Moist");
        CHECK(FirstSpriteIndex_(graphics, TilePrefix_(rMoist.spriteTiles->land)) < 0);
        const bool bGround = std::ranges::any_of(graphics.sprites, [&](const auto& rSprite) {
            return std::ranges::find(rGround, rSprite.textureId) != rGround.end()
                   && rSprite.shape.has_value();
        });
        CHECK(bGround);
    }

    SECTION("structures follow the nutrient yield, clamped to the rows there are")
    {
        CHECK(FirstSpriteIndex_(render(nutrients(0)), rRows[0]) >= 0);
        CHECK(FirstSpriteIndex_(render(nutrients(2)), rRows[1]) >= 0);
        CHECK(FirstSpriteIndex_(render(nutrients(9)), rRows[2]) >= 0);
        CHECK(FirstSpriteIndex_(render({}), rRows[0]) >= 0);
    }

    SECTION("an occupant that hides the farm structures draws in their place")
    {
        const ImprovementConfig_t& rCondenser = world.improvements.Get("Condenser");
        rTile.AddImprovement(rCondenser);
        const RecordingGraphics graphics = render(nutrients(2));
        CHECK(FirstSpriteIndex_(graphics, rRows[1]) < 0);
        CHECK(FirstSpriteIndex_(graphics, rCondenser.spritePaths.land.front()) >= 0);
    }
}

TEST_CASE("Improvements draw their sea art at sea", "[ui][tile]")
{
    EnsureStyleLoaded_();
    actest::WorldFixture world;
    const ImprovementConfig_t& rKelp = world.improvements.Get("KelpFarm");
    WriteOccupantStubs_(rKelp);
    Tile& rSea = *world.map.GetTile(8, 4);
    rSea.SetElevation(actest::TestMapRules().oceanShelfMeters);
    rSea.AddImprovement(rKelp);

    RecordingGraphics graphics;
    TileRenderer::Render(graphics, rSea, FlatTileShape(0.0f, 0.0f, 100.0f),
                         /*bFogged*/ false, &world.map);
    const std::ptrdiff_t index = FirstSpriteIndex_(graphics, rKelp.spritePaths.sea.front());
    REQUIRE(index >= 0);
    CHECK_FALSE(graphics.sprites[static_cast<std::size_t>(index)].shape.has_value());
}

TEST_CASE("Object art that fails to load shows a magenta and black checker", "[ui][tile]")
{
    EnsureStyleLoaded_();
    const auto& s = Style().tileRenderer;
    actest::WorldFixture world;
    Tile& rLand = *world.map.GetTile(8, 4);
    rLand.SetElevation(500);
    const TileShape_t shape = FlatTileShape(0.0f, 0.0f, 100.0f);
    const auto checkerCells = [&](const RecordingGraphics& rGraphics) {
        std::vector<RecordingGraphics::RectDraw_t> cells;
        std::ranges::copy_if(rGraphics.rects, std::back_inserter(cells), [&](const auto& rRect) {
            return rRect.bFilled
                   && (ColorEq_(rRect.color, s.missingArtColor)
                       || ColorEq_(rRect.color, s.missingArtAltColor));
        });
        return cells;
    };

    SECTION("configured art that is not on disk")
    {
        ImprovementConfig_t missing = world.improvements.Get("Condenser");
        missing.spritePaths.land = {"tests/fixtures/sprites/missing/no_such_art.png"};
        std::filesystem::remove(missing.spritePaths.land.front());
        rLand.AddImprovement(missing);
        RecordingGraphics graphics;
        TileRenderer::RenderObjects(graphics, rLand, shape);

        const auto cells = checkerCells(graphics);
        REQUIRE(cells.size() == 4);
        const float side = 100.0f * s.missingArtSizeRatio;
        const float left = std::ranges::min(cells, {}, &RecordingGraphics::RectDraw_t::x).x;
        const float top = std::ranges::min(cells, {}, &RecordingGraphics::RectDraw_t::y).y;
        CHECK_THAT(left, Catch::Matchers::WithinAbs(50.0f - side * 0.5f, 1e-3));
        CHECK_THAT(top, Catch::Matchers::WithinAbs(25.0f - side * 0.5f, 1e-3));
        const auto colorAt = [&](float x, float y) {
            return std::ranges::find_if(cells, [&](const auto& rCell) {
                       return std::abs(rCell.x - x) < 1e-3f && std::abs(rCell.y - y) < 1e-3f;
                   })->color;
        };
        CHECK(ColorEq_(colorAt(left, top), s.missingArtColor));
        CHECK(ColorEq_(colorAt(left + side * 0.5f, top), s.missingArtAltColor));
        CHECK(ColorEq_(colorAt(left, top + side * 0.5f), s.missingArtAltColor));
        CHECK(ColorEq_(colorAt(left + side * 0.5f, top + side * 0.5f), s.missingArtColor));
    }

    SECTION("art that loads draws no checker")
    {
        const ImprovementConfig_t& rCondenser = world.improvements.Get("Condenser");
        WriteOccupantStubs_(rCondenser);
        rLand.AddImprovement(rCondenser);
        RecordingGraphics graphics;
        TileRenderer::RenderObjects(graphics, rLand, shape);
        CHECK(checkerCells(graphics).empty());
    }

    SECTION("an occupant with no art configured draws nothing")
    {
        ImprovementConfig_t artless = world.improvements.Get("Condenser");
        artless.spritePaths = {};
        rLand.AddImprovement(artless);
        RecordingGraphics graphics;
        TileRenderer::RenderObjects(graphics, rLand, shape);
        CHECK(checkerCells(graphics).empty());
        CHECK(graphics.sprites.empty());
    }
}
