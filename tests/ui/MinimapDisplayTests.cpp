#include "ViewFixture.h"

#include "game/Faction.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "ui/style/UiStyle.h"
#include "ui/world/MapViewport.h"
#include "ui/world/MinimapDisplay.h"
#include "ui/world/WorldDisplay.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;
using Catch::Matchers::WithinAbs;

namespace
{

constexpr float k_PixelW = 20.0f;
constexpr float k_PixelH = 10.0f;

struct MinimapRig_t
{
    ViewFixture fixture{false};
    WorldDisplay display{*fixture.pState, fixture.pSprites->renderer, ViewFixture::FullScreen()};
    WindowLayout_t layout{100.0f, 50.0f, 720.0f, 170.0f};
    std::vector<std::pair<int, int>> centered;
    MinimapDisplay minimap{*fixture.pState, fixture.pSprites->renderer, layout,
                           display.GetViewport(),
                           [this](int x, int y) { centered.emplace_back(x, y); }};

    void ClickPixel(int px, int py, MouseButton_t button = MouseButton_t::Left)
    {
        minimap.HandleMouseClick(MouseEvent_t{
            button, static_cast<int>(layout.x + (static_cast<float>(px) + 0.5f) * k_PixelW),
            static_cast<int>(layout.y + (static_cast<float>(py) + 0.5f) * k_PixelH),
            ModifierState_t{}});
    }

    std::vector<RecordingGraphics::RectDraw_t> RenderFrameRects()
    {
        RecordingGraphics graphics;
        minimap.Render(graphics);
        std::vector<RecordingGraphics::RectDraw_t> frame;
        std::ranges::copy_if(graphics.rects, std::back_inserter(frame),
                             [](const RecordingGraphics::RectDraw_t& rRect) {
                                 return !rRect.bFilled;
                             });
        return frame;
    }
};

} // namespace

TEST_CASE("Minimap draws the map as a brick with 2:1 texels", "[ui][minimap]")
{
    MinimapRig_t rig;
    const WorldMap& rMap = rig.fixture.pState->GetWorldMap();

    SECTION("a layout that fits exactly fills it")
    {
        RecordingGraphics graphics;
        rig.minimap.Render(graphics);
        REQUIRE(graphics.sprites.size() == 1);
        const auto& rSprite = graphics.sprites.front();
        CHECK_THAT(rSprite.x, WithinAbs(rig.layout.x, 0.01f));
        CHECK_THAT(rSprite.y, WithinAbs(rig.layout.y, 0.01f));
        CHECK_THAT(rSprite.destWidth, WithinAbs(k_PixelW * rMap.GetWidth(), 0.01f));
        CHECK_THAT(rSprite.destHeight, WithinAbs(k_PixelH * rMap.GetHeight(), 0.01f));
        CHECK(graphics.upsertTextureCount == 1);
    }

    SECTION("a narrow layout keeps the 2:1 texel aspect and centres the image")
    {
        const WindowLayout_t narrow{0.0f, 0.0f, 300.0f, 400.0f};
        MinimapDisplay minimap(*rig.fixture.pState, rig.fixture.pSprites->renderer, narrow,
                               rig.display.GetViewport(), [](int, int) {});
        RecordingGraphics graphics;
        minimap.Render(graphics);
        REQUIRE(graphics.sprites.size() == 1);
        const auto& rSprite = graphics.sprites.front();
        const float texelW = rSprite.destWidth / static_cast<float>(rMap.GetWidth());
        const float texelH = rSprite.destHeight / static_cast<float>(rMap.GetHeight());
        CHECK_THAT(texelW, WithinAbs(2.0f * texelH, 0.001f));
        CHECK_THAT(rSprite.destWidth, WithinAbs(narrow.width, 0.01f));
        CHECK_THAT(rSprite.y, WithinAbs((narrow.height - rSprite.destHeight) * 0.5f, 0.01f));
    }
}

TEST_CASE("Minimap click resolves to the tile under the pixel", "[ui][minimap]")
{
    MinimapRig_t rig;
    const int width = rig.fixture.pState->GetWorldMap().GetWidth();

    SECTION("either half of a tile's two pixels picks that tile")
    {
        rig.ClickPixel(6, 2);
        rig.ClickPixel(7, 2);
        rig.ClickPixel(7, 3);
        rig.ClickPixel(8, 3);
        const std::vector<std::pair<int, int>> expected{{6, 2}, {6, 2}, {7, 3}, {7, 3}};
        CHECK(rig.centered == expected);
    }

    SECTION("the second pixel of the last tile in a row wraps to pixel 0")
    {
        rig.ClickPixel(width - 2, 2);
        rig.ClickPixel(0, 3);
        rig.ClickPixel(width - 1, 3);
        const std::vector<std::pair<int, int>> expected{{width - 2, 2}, {width - 1, 3},
                                                        {width - 1, 3}};
        CHECK(rig.centered == expected);
    }

    SECTION("clicks outside the image or with another button are ignored")
    {
        rig.ClickPixel(-1, 2);
        rig.ClickPixel(2, -1);
        rig.ClickPixel(width, 2);
        rig.ClickPixel(2, rig.fixture.pState->GetWorldMap().GetHeight());
        rig.ClickPixel(6, 2, MouseButton_t::Right);
        CHECK(rig.centered.empty());
    }
}

TEST_CASE("Minimap viewport frame splits at the seam", "[ui][minimap]")
{
    MinimapRig_t rig;
    MapViewport& rViewport = rig.display.GetViewport();
    const int width = rig.fixture.pState->GetWorldMap().GetWidth();
    const int cols = rViewport.VisibleCols();
    REQUIRE(cols < width);

    SECTION("a frame inside the map is one rectangle in map units")
    {
        rViewport.SetCamera(4, 0);
        const auto frame = rig.RenderFrameRects();
        REQUIRE(frame.size() == 1);
        CHECK_THAT(frame[0].x, WithinAbs(rig.layout.x + 4.0f * k_PixelW, 0.01f));
        CHECK_THAT(frame[0].width, WithinAbs(static_cast<float>(cols) * k_PixelW, 0.01f));
        CHECK_THAT(frame[0].y, WithinAbs(rig.layout.y, 0.01f));
    }

    SECTION("a frame straddling the seam is two rectangles")
    {
        const int cameraX = width - 4;
        rViewport.SetCamera(cameraX, 0);
        const auto frame = rig.RenderFrameRects();
        REQUIRE(frame.size() == 2);
        CHECK_THAT(frame[0].x, WithinAbs(rig.layout.x + static_cast<float>(cameraX) * k_PixelW,
                                         0.01f));
        CHECK_THAT(frame[0].width, WithinAbs(4.0f * k_PixelW, 0.01f));
        CHECK_THAT(frame[1].x, WithinAbs(rig.layout.x, 0.01f));
        CHECK_THAT(frame[1].width, WithinAbs(static_cast<float>(cols - 4) * k_PixelW, 0.01f));
    }
}

TEST_CASE("Minimap redraws when the player's tile memory changes", "[ui][minimap][memory]")
{
    ViewFixture fixture;
    WorldDisplay display{*fixture.pState, fixture.pSprites->renderer, ViewFixture::FullScreen()};
    MinimapDisplay minimap{*fixture.pState, fixture.pSprites->renderer,
                           WindowLayout_t{100.0f, 50.0f, 720.0f, 170.0f}, display.GetViewport(),
                           [](int, int) {}};
    Tile& rTile = *fixture.pState->GetWorldMap().GetTile(8, 6);
    const ImprovementConfig_t* pBonus = rTile.FindOccupantConfig("Nutrients");
    REQUIRE(pBonus != nullptr);
    rTile.AddTerrainFeature(*pBonus);

    RecordingGraphics graphics;
    minimap.Render(graphics);
    const int uploads = graphics.upsertTextureCount;
    minimap.Render(graphics);
    CHECK(graphics.upsertTextureCount == uploads);

    fixture.pPlayer->GetTileMemory().Record(rTile);
    minimap.Render(graphics);
    CHECK(graphics.upsertTextureCount == uploads + 1);
}
