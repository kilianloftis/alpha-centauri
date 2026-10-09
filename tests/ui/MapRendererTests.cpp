#include "ViewFixture.h"

#include "game/Faction.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionTileMemory.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "ui/MapRenderer.h"
#include "ui/TileShapeGeometry.h"
#include "ui/style/UiStyle.h"
#include "ui/world/FactionBaseArt.h"
#include "ui/world/LocationPanel.h"
#include "ui/world/MapAppearance.h"
#include "ui/world/UnitMarkerRenderer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;
using Catch::Matchers::WithinAbs;

namespace
{

constexpr float k_TileWidth = 100.0f;

bool SameColor_(const Color_t& a, const Color_t& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool SamePoint_(float x1, float y1, float x2, float y2)
{
    return std::abs(x1 - x2) < 0.01f && std::abs(y1 - y2) < 0.01f;
}

Unit& MakeUnit_(ViewFixture& rFixture, int x, int y, std::deque<UnitDesign>& rDesigns)
{
    const UnitComponentConfig_t* pChassis = rFixture.unitComponents.Find("test_chassis");
    REQUIRE(pChassis);
    UnitSlotConfig_t slot;
    slot.id = "slot_0";
    slot.displayName = slot.id;
    slot.componentType = pChassis->type;
    rDesigns.emplace_back(std::vector<UnitSlotConfig_t>{slot},
                          std::unordered_map<std::string, const UnitComponentConfig_t*>{
                              {slot.id, pChassis}});
    Tile* pTile = rFixture.pState->GetWorldMap().GetTile(x, y);
    REQUIRE(pTile);
    return rFixture.pPlayer->GetUnitManager().CreateUnit(
        rFixture.pState->AllocateUnitId(), rDesigns.back(),
        rFixture.pState->GetWorldMap().GetUnitPositions(), *pTile, nullptr);
}

// Every coast overlay extract_terrain.py writes, so a coast that is drawn shows up.
void AddCoastArt_(ViewFixture& rFixture)
{
    for (const std::string_view part : {"water", "shore"})
    {
        for (const char corner : {'w', 'n', 'e', 's'})
        {
            for (const std::string_view caseName : {"1", "2", "3", "4", "5", "6", "7", "7_alt"})
            {
                rFixture.pSprites->existing.insert(Style().tileRenderer.coastSpriteDir + "/"
                                                   + std::string(part) + "_" + corner + "_"
                                                   + std::string(caseName) + ".png");
            }
        }
    }
}

// Faction base art for the fixture's faction, as extract_faction.py lays it out.
std::string AddBaseArt_(ViewFixture& rFixture)
{
    rFixture.factionDefinition.id = "gaian";
    std::filesystem::create_directories("assets/factions/gaian");
    const std::string spritePath = BareBaseSpritePath("gaian", false, 1);
    rFixture.pSprites->existing.insert(spritePath);
    return spritePath;
}

} // namespace

TEST_CASE("A shrouded tile shows only the shroud and land-coloured edges", "[ui][map]")
{
    // Declared first so ~Faction runs while this config is still alive.
    FactionConfig_t enemyDefinition;
    ViewFixture fixture;
    enemyDefinition = fixture.factionDefinition;
    enemyDefinition.id = "enemy_faction";
    enemyDefinition.identity.name = "Enemy";
    Faction& enemy = fixture.pState->AddFaction(std::make_unique<Faction>(
        fixture.pState->AllocateFactionId(), false, enemyDefinition, fixture.dataContext,
        fixture.pState->GetWorldMap(), fixture.settings, actest::k_TestFactionSeed + 1));

    WorldMap& rMap = fixture.pState->GetWorldMap();
    Tile& rTile = *rMap.GetTile(8, 8);
    REQUIRE(enemy.CreateBase(fixture.pState->AllocateBaseId(), "Hidden", &rTile,
                             fixture.pState->GetTileEffects(),
                             fixture.pState->GetSecretProjectAvailability()));
    const ImprovementConfig_t* pBonus = rTile.FindOccupantConfig("Nutrients");
    REQUIRE(pBonus);
    rTile.AddTerrainFeature(*pBonus);
    fixture.pSprites->existing.insert(
        std::get<OccupantSpritePaths_t>(pBonus->art.value().sprites).land.front());
    // Water across one edge: the live map would give this land tile a coast.
    GetTileAtLatticeOffset(rMap, rTile, 0, -1)->SetElevation(-500);
    AddCoastArt_(fixture);
    REQUIRE(rTile.IsLand());
    REQUIRE_FALSE(fixture.pPlayer->GetExploredMap().IsExplored(rTile));

    const PlacedTile_t placed{&rTile, FlatTileShape(0.0f, 0.0f, k_TileWidth)};
    MapContent_t content;
    content.showsBase = [](const BaseManager&) { return true; };
    RecordingGraphics graphics;
    fixture.pMapRenderer->Render(graphics, std::span(&placed, 1),
                                 MapAppearance::Fogged(rMap, fixture.pPlayer), content);

    CHECK(graphics.sprites.empty());
    CHECK(graphics.texts.empty());
    REQUIRE(graphics.rects.size() == 1);
    CHECK(SameColor_(graphics.rects.front().color, Style().tileRenderer.shroudColor));
    REQUIRE_FALSE(graphics.lines.empty());
    for (const RecordingGraphics::LineDraw_t& rLine : graphics.lines)
    {
        CHECK(SameColor_(rLine.color, Style().mapRenderer.gridLandColor));
    }
}

TEST_CASE("An edge between placed tiles draws once, and a partial scene keeps a closed outline",
          "[ui][map][grid]")
{
    ViewFixture fixture(false);
    const WorldMap& rMap = fixture.pState->GetWorldMap();
    const Tile& rBack = *rMap.GetTile(8, 8);
    // Lattice (1, 0) is map (+1, +1): the tile in front, down and to the right.
    const Tile* pFront = GetTileAtLatticeOffset(rMap, rBack, 1, 0);
    REQUIRE(pFront);
    REQUIRE(rBack.IsLand());
    REQUIRE(pFront->IsLand());
    const std::array<PlacedTile_t, 2> tiles{
        PlacedTile_t{&rBack, FlatTileShape(0.0f, 0.0f, k_TileWidth)},
        PlacedTile_t{pFront, FlatTileShape(k_TileWidth * 0.5f, k_TileWidth * 0.25f, k_TileWidth)}};

    RecordingGraphics graphics;
    fixture.pMapRenderer->Render(graphics, tiles, MapAppearance::Clear(rMap, nullptr),
                                 MapContent_t{});

    // Two diamonds sharing one edge.
    CHECK(graphics.lines.size() == 7);
    const TileShape_t& rBackShape = tiles[0].shape;
    const auto isShared = [&rBackShape](const RecordingGraphics::LineDraw_t& rLine) {
        const TileVertex_t& rA = rBackShape.east;
        const TileVertex_t& rB = rBackShape.south;
        return (SamePoint_(rLine.x1, rLine.y1, rA.x, rA.y) && SamePoint_(rLine.x2, rLine.y2, rB.x, rB.y))
               || (SamePoint_(rLine.x1, rLine.y1, rB.x, rB.y)
                   && SamePoint_(rLine.x2, rLine.y2, rA.x, rA.y));
    };
    CHECK(std::ranges::count_if(graphics.lines, isShared) == 1);
}

TEST_CASE("Bases draw over every tile with their names, and only the bases the content shows",
          "[ui][map][bases]")
{
    ViewFixture fixture;
    const std::string spritePath = AddBaseArt_(fixture);
    fixture.pPlayer->GetExploredMap().MarkAll();
    BaseManager& rShown = fixture.MakeBase(8, 8);
    BaseManager& rOther = fixture.MakeBase(20, 8);

    const WorldMap& rMap = fixture.pState->GetWorldMap();
    const Tile* pFront = GetTileAtLatticeOffset(rMap, rShown.GetTile(), 1, 0);
    REQUIRE(pFront);
    const std::array<PlacedTile_t, 3> tiles{
        PlacedTile_t{&rShown.GetTile(), FlatTileShape(0.0f, 0.0f, k_TileWidth)},
        PlacedTile_t{pFront, FlatTileShape(k_TileWidth * 0.5f, k_TileWidth * 0.25f, k_TileWidth)},
        PlacedTile_t{&rOther.GetTile(), FlatTileShape(4.0f * k_TileWidth, 0.0f, k_TileWidth)}};
    MapContent_t content;
    content.showsBase = [&rShown](const BaseManager& rBase) { return &rBase == &rShown; };

    RecordingGraphics graphics;
    fixture.pMapRenderer->Render(graphics, tiles, MapAppearance::Fogged(rMap, fixture.pPlayer),
                                 content);

    const auto baseSprite = std::ranges::find_if(graphics.sprites, [&](const auto& rSprite) {
        return rSprite.textureId == spritePath;
    });
    REQUIRE(baseSprite != graphics.sprites.end());
    CHECK(std::ranges::count_if(graphics.sprites, [&](const auto& rSprite) {
              return rSprite.textureId == spritePath;
          })
          == 1);
    const auto [originX, originY] = FootprintOrigin(tiles[0].shape);
    const float spriteHeight =
        k_TileWidth * k_IsoHeightRatio * (1.0f + Style().mapRenderer.baseSpriteOverhangRatio);
    // Fixture size1 origin_y_ratio (-0.20) lifts the cell off the south tip.
    constexpr float k_Size1OriginYRatio = -0.20f;
    CHECK(baseSprite->x == originX);
    CHECK_THAT(baseSprite->y, WithinAbs(originY + k_TileWidth * k_Size1OriginYRatio, 0.01f));
    CHECK_THAT(baseSprite->destWidth, WithinAbs(k_TileWidth, 0.01f));
    CHECK_THAT(baseSprite->destHeight, WithinAbs(spriteHeight, 0.01f));
    // Every tile's terrain and grid come first, so a tile in front cannot cover the overhang.
    for (const RecordingGraphics::RectDraw_t& rRect : graphics.rects)
    {
        CHECK(rRect.order < baseSprite->order);
    }
    for (const RecordingGraphics::LineDraw_t& rLine : graphics.lines)
    {
        CHECK(rLine.order < baseSprite->order);
    }

    REQUIRE(graphics.texts.size() == 1);
    const float nameX = k_TileWidth * Style().mapRenderer.baseNameOffsetXRatio;
    const float nameY = k_TileWidth * Style().mapRenderer.baseNameOffsetYRatio;
    CHECK(SamePoint_(graphics.texts.front().x, graphics.texts.front().y, nameX, nameY));
}

TEST_CASE("Units draw last, as the content filters them, and Render reports where",
          "[ui][map][units]")
{
    ViewFixture fixture;
    const std::string spritePath = AddBaseArt_(fixture);
    fixture.pPlayer->GetExploredMap().MarkAll();
    BaseManager& rBase = fixture.MakeBase(8, 8);
    std::deque<UnitDesign> designs;
    const Unit& rHidden = MakeUnit_(fixture, 8, 8, designs);
    const Unit& rShown = MakeUnit_(fixture, 8, 8, designs);

    const PlacedTile_t placed{&rBase.GetTile(), FlatTileShape(0.0f, 0.0f, k_TileWidth)};
    MapContent_t content;
    content.showsBase = [](const BaseManager&) { return true; };
    content.showsUnit = [&rShown](const Unit& rUnit) { return &rUnit == &rShown; };
    content.pSelectedUnit = &rShown;

    RecordingGraphics graphics;
    const UnitMarkerRects_t markers = fixture.pMapRenderer->Render(
        graphics, std::span(&placed, 1),
        MapAppearance::Fogged(fixture.pState->GetWorldMap(), fixture.pPlayer), content);

    CHECK_FALSE(markers.contains(rHidden.GetUnitId()));
    REQUIRE(markers.contains(rShown.GetUnitId()));
    // The first slot: a filtered unit takes no room.
    const auto [tileX, tileY] = FootprintOrigin(placed.shape);
    const Rectangle_t expected = UnitMarkerRenderer::MarkerRectOnTile(tileX, tileY, k_TileWidth, 0);
    const Rectangle_t& rMarker = markers.at(rShown.GetUnitId());
    CHECK(SamePoint_(rMarker.x, rMarker.y, expected.x, expected.y));

    const auto chip = std::ranges::find_if(graphics.rects, [&rMarker](const auto& rRect) {
        return rRect.bFilled && SamePoint_(rRect.x, rRect.y, rMarker.x, rMarker.y);
    });
    REQUIRE(chip != graphics.rects.end());
    const auto baseSprite = std::ranges::find_if(graphics.sprites, [&](const auto& rSprite) {
        return rSprite.textureId == spritePath;
    });
    REQUIRE(baseSprite != graphics.sprites.end());
    CHECK(baseSprite->order < chip->order);
    CHECK(std::ranges::any_of(graphics.rects, [](const auto& rRect) {
        return !rRect.bFilled && SameColor_(rRect.color, Style().unitMarker.selectionBorderColor);
    }));
}

TEST_CASE("A tile shows one unit, and a base shows one only when it is selected",
          "[ui][map][units]")
{
    ViewFixture fixture;
    fixture.pPlayer->GetExploredMap().MarkAll();
    WorldMap& rMap = fixture.pState->GetWorldMap();
    std::deque<UnitDesign> designs;
    const Unit& rFirst = MakeUnit_(fixture, 8, 8, designs);
    const Unit& rSecond = MakeUnit_(fixture, 8, 8, designs);
    const PlacedTile_t placed{rMap.GetTile(8, 8), FlatTileShape(0.0f, 0.0f, k_TileWidth)};

    const auto render = [&](const Unit* pSelected, bool bShowBases) {
        MapContent_t content;
        content.showsBase = [bShowBases](const BaseManager&) { return bShowBases; };
        content.showsUnit = [](const Unit&) { return true; };
        content.pSelectedUnit = pSelected;
        RecordingGraphics graphics;
        const UnitMarkerRects_t markers = fixture.pMapRenderer->Render(
            graphics, std::span(&placed, 1), MapAppearance::Fogged(rMap, fixture.pPlayer),
            content);
        return markers;
    };

    SECTION("two units and no base show the first")
    {
        const UnitMarkerRects_t markers = render(nullptr, true);
        CHECK(markers.size() == 1);
        CHECK(markers.contains(rFirst.GetUnitId()));
        CHECK_FALSE(markers.contains(rSecond.GetUnitId()));
    }

    SECTION("the selected unit is the one drawn")
    {
        const UnitMarkerRects_t markers = render(&rSecond, true);
        CHECK(markers.size() == 1);
        CHECK(markers.contains(rSecond.GetUnitId()));
        CHECK_FALSE(markers.contains(rFirst.GetUnitId()));
    }

    SECTION("a base hides every unit until one is selected")
    {
        fixture.MakeBase(8, 8);
        const UnitMarkerRects_t hidden = render(nullptr, true);
        CHECK(hidden.empty());

        const UnitMarkerRects_t selected = render(&rSecond, true);
        CHECK(selected.size() == 1);
        CHECK(selected.contains(rSecond.GetUnitId()));
    }
}

TEST_CASE("The location preview shows a remembered tile without fog, its farm by the tile's yield",
          "[ui][map][location]")
{
    ViewFixture fixture;
    Tile& rTile = *fixture.pState->GetWorldMap().GetTile(8, 8);
    const ImprovementConfig_t& rFarm = fixture.improvements.Get("Farm");
    rTile.AddImprovement(rFarm);
    const std::vector<std::string>& rRows =
        std::get<OccupantYieldRows_t>(rFarm.art.value().sprites).paths.land;
    fixture.pSprites->existing.insert(rRows.begin(), rRows.end());
    fixture.pPlayer->GetExploredMap().Mark(rTile);
    fixture.pPlayer->GetTileMemory().Record(rTile);
    REQUIRE_FALSE(fixture.pPlayer->GetVisibleMap().IsVisible(rTile));
    const int nutrients =
        fixture.pState->GetTileEffects().ResolveTileYield(rTile).effective.nutrients;
    // A yield past the first row, so the row shows the yield was read.
    REQUIRE(nutrients >= 2);

    LocationPanel panel(*fixture.pState, *fixture.pMapRenderer, fixture.pSprites->sprites,
                        WindowLayout_t{0.0f, 0.0f, 300.0f, 400.0f});
    panel.SetSelectedTile(&rTile);
    RecordingGraphics graphics;
    panel.Render(graphics);

    CHECK(std::ranges::none_of(graphics.rects, [](const auto& rRect) {
        return SameColor_(rRect.color, Style().tileRenderer.fogHazeColor);
    }));
    const std::size_t row =
        static_cast<std::size_t>(std::clamp(nutrients - 1, 0, static_cast<int>(rRows.size()) - 1));
    CHECK(std::ranges::any_of(graphics.sprites, [&](const auto& rSprite) {
        return rSprite.textureId == rRows[row];
    }));
}
