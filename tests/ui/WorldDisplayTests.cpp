#include "StubSprites.h"
#include "ViewFixture.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "ui/style/UiStyle.h"
#include "ui/world/WorldDisplay.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;

namespace
{

bool SameColor_(const Color_t& a, const Color_t& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

std::size_t CountLines_(const RecordingGraphics& rGraphics, const Color_t& color)
{
    return static_cast<std::size_t>(
        std::ranges::count_if(rGraphics.lines, [&color](const RecordingGraphics::LineDraw_t& rLine) {
            return SameColor_(rLine.color, color);
        }));
}

RecordingGraphics Render_(ViewFixture& rFixture, ReliefMode_t relief, bool bOceanGrid)
{
    rFixture.settings.SetMapDisplay(MapDisplayConfig_t{relief, bOceanGrid});
    WorldDisplay display(*rFixture.pState, ViewFixture::FullScreen());
    RecordingGraphics graphics;
    display.Render(graphics);
    return graphics;
}

} // namespace

TEST_CASE("The map grid draws land edges and adds water edges with the ocean grid",
          "[ui][world][grid]")
{
    ViewFixture fixture;
    fixture.pPlayer->GetExploredMap().MarkAll();
    fixture.pState->GetWorldMap().GetTile(8, 8)->SetElevation(-500);
    const auto& s = Style().tileRenderer;

    const RecordingGraphics withoutOcean = Render_(fixture, ReliefMode_t::Flat, false);
    CHECK(CountLines_(withoutOcean, s.gridLandColor) > 0);
    CHECK(CountLines_(withoutOcean, s.gridWaterColor) == 0);

    const RecordingGraphics withOcean = Render_(fixture, ReliefMode_t::Flat, true);
    CHECK(CountLines_(withOcean, s.gridWaterColor) > 0);
    CHECK(CountLines_(withOcean, s.gridLandColor) == CountLines_(withoutOcean, s.gridLandColor));
}

TEST_CASE("Grid edges next to unexplored ground use the land colour", "[ui][world][grid]")
{
    ViewFixture fixture;
    WorldMap& rMap = fixture.pState->GetWorldMap();
    Tile& rWater = *rMap.GetTile(8, 8);
    rWater.SetElevation(-500);
    for (const auto& pTile : rMap.GetTiles())
    {
        if (pTile.get() != &rWater)
        {
            fixture.pPlayer->GetExploredMap().Mark(*pTile);
        }
    }

    const RecordingGraphics graphics = Render_(fixture, ReliefMode_t::Flat, true);
    CHECK(CountLines_(graphics, Style().tileRenderer.gridWaterColor) == 0);
}

TEST_CASE("Grid lines run through the raised corners", "[ui][world][grid][relief]")
{
    ViewFixture fixture;
    fixture.pPlayer->GetExploredMap().MarkAll();
    WorldMap& rMap = fixture.pState->GetWorldMap();
    for (const auto& pTile : rMap.GetTiles())
    {
        pTile->SetElevation(2000);
    }
    const auto& s = Style().tileRenderer;

    const RecordingGraphics flat = Render_(fixture, ReliefMode_t::Flat, false);
    const RecordingGraphics raised = Render_(fixture, ReliefMode_t::Smooth, false);
    const WorldDisplay display(*fixture.pState, ViewFixture::FullScreen());
    // Inland corners all sit two levels up.
    const float lift = 2.0f * s.relief.liftPerLevelRatio * display.GetViewport().TileWidth();

    const auto shiftedUp = [lift](const RecordingGraphics::LineDraw_t& rFlat,
                                  const RecordingGraphics::LineDraw_t& rRaised) {
        return std::abs(rRaised.x1 - rFlat.x1) < 0.01f && std::abs(rRaised.x2 - rFlat.x2) < 0.01f
               && std::abs(rRaised.y1 - (rFlat.y1 - lift)) < 0.01f
               && std::abs(rRaised.y2 - (rFlat.y2 - lift)) < 0.01f;
    };
    const bool bAnyRaised = std::ranges::any_of(flat.lines, [&](const auto& rFlat) {
        return std::ranges::any_of(raised.lines,
                                   [&](const auto& rRaised) { return shiftedUp(rFlat, rRaised); });
    });
    CHECK(bAnyRaised);
}

TEST_CASE("A tile's grid lines draw over its terrain and under its objects", "[ui][world][grid]")
{
    ViewFixture fixture;
    fixture.pPlayer->GetExploredMap().MarkAll();
    Tile& rTile = *fixture.pState->GetWorldMap().GetTile(8, 6);
    REQUIRE(rTile.IsLand());
    const ImprovementConfig_t* pBonus = rTile.FindOccupantConfig("Nutrients");
    REQUIRE(pBonus != nullptr);
    rTile.AddTerrainFeature(*pBonus);
    const std::string& bonusPath = pBonus->spritePaths.land.front();
    actest::WriteStubPng(bonusPath);

    const RecordingGraphics graphics = Render_(fixture, ReliefMode_t::Flat, false);
    const auto bonus = std::ranges::find_if(graphics.sprites, [&](const auto& rSprite) {
        return rSprite.textureId == bonusPath;
    });
    REQUIRE(bonus != graphics.sprites.end());
    // The tile's NW and NE edges are the last draws before its bonus.
    const auto edge = std::ranges::find_if(graphics.lines, [&](const auto& rLine) {
        return rLine.order + 1 == bonus->order;
    });
    REQUIRE(edge != graphics.lines.end());
    CHECK(SameColor_(edge->color, Style().tileRenderer.gridLandColor));
}
