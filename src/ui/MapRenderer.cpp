#include "ui/MapRenderer.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/buildings/BaseSpriteSizesConfig.h"
#include "game/buildings/MapOverlayChannelsConfig.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "ui/style/UiStyle.h"
#include "ui/world/MapAppearance.h"
#include "ui/world/UnitMarkerRenderer.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace ac
{

namespace
{

constexpr size_t k_BaseNameMinTruncChars = 3;

template<typename T>
const T& Require_(const std::unique_ptr<T>& rpConfig, const char* pName)
{
    if (!rpConfig)
    {
        throw std::runtime_error(std::string("MapRenderer: game data has no ") + pName);
    }
    return *rpConfig;
}

// The name cut down, with a trailing ".", until it fits maxWidth; never below three characters.
std::string FitBaseName_(Graphics& rGraphics, const std::string& rName, unsigned int fontSize,
                         float maxWidth)
{
    std::string displayName = rName;
    if (rGraphics.MeasureTextWidth(displayName, fontSize) <= maxWidth)
    {
        return displayName;
    }
    for (size_t n = displayName.size(); n > 0; --n)
    {
        std::string candidate;
        if (n > k_BaseNameMinTruncChars)
        {
            candidate = displayName.substr(0, n - 1) + ".";
        }
        else if (n >= k_BaseNameMinTruncChars)
        {
            candidate = displayName.substr(0, n);
        }
        else
        {
            break;
        }
        if (rGraphics.MeasureTextWidth(candidate, fontSize) <= maxWidth)
        {
            return candidate;
        }
    }
    return displayName;
}

float WidthOf_(const TileShape_t& rShape)
{
    return rShape.east.x - rShape.west.x;
}

} // namespace

MapRenderer::MapRenderer(SpriteLibrary& rSprites, const GameState& rGameState,
                         const TileRendererStyle_t& rTileStyle, const MapRendererStyle_t& rStyle)
    : m_rGameState(rGameState)
    , m_rStyle(rStyle)
    , m_rBaseSpriteSizes(Require_(rGameState.GetGameData().baseSpriteSizes, "base_sprite_sizes"))
    , m_rMapOverlayChannels(
          Require_(rGameState.GetGameData().mapOverlayChannels, "map_overlay_channels"))
    , m_tiles(rSprites, rTileStyle)
    , m_baseArt(rSprites)
    , m_yieldOf([&rGameState](const Tile& rTile) {
        return rGameState.GetTileEffects().ResolveTileYield(rTile).effective;
    })
{
}

UnitMarkerRects_t MapRenderer::Render(Graphics& rGraphics, std::span<const PlacedTile_t> tiles,
                                      const MapAppearance& rAppearance,
                                      const MapContent_t& rContent)
{
    PlacedSet_t placed;
    placed.reserve(tiles.size());
    for (const PlacedTile_t& rPlaced : tiles)
    {
        if (!rPlaced.pTile)
        {
            throw std::invalid_argument("MapRenderer::Render: a placed tile has no tile");
        }
        placed.insert(rPlaced.pTile);
    }

    for (const PlacedTile_t& rPlaced : tiles)
    {
        m_tiles.RenderTerrain(rGraphics, *rPlaced.pTile, rPlaced.shape, rAppearance);
        DrawGrid_(rGraphics, rPlaced, placed, rAppearance);
        m_tiles.RenderObjects(rGraphics, *rPlaced.pTile, rPlaced.shape, rAppearance, m_yieldOf);
    }

    if (rContent.showsBase)
    {
        for (const PlacedTile_t& rPlaced : tiles)
        {
            DrawBase_(rGraphics, rPlaced, rAppearance, rContent);
        }
    }

    UnitMarkerRects_t markers;
    if (rContent.showsUnit)
    {
        for (const PlacedTile_t& rPlaced : tiles)
        {
            DrawUnits_(rGraphics, rPlaced, rAppearance, rContent, markers);
        }
    }
    return markers;
}

Color_t MapRenderer::TileColor(const Tile& rTile, const MapAppearance& rAppearance) const
{
    return m_tiles.FillColor(rTile, rAppearance);
}

// Each placed tile draws its back edges (NW, NE), and its front edges (SE, SW) where the tile in
// front is not placed, so an edge between two placed tiles draws once and a partial scene keeps a
// closed outline. Raised tiles in front cover lines behind. Edges touching water need the ocean
// grid; anything next to shroud takes the land colour so the grid does not reveal coastlines.
void MapRenderer::DrawGrid_(Graphics& rGraphics, const PlacedTile_t& rPlaced,
                            const PlacedSet_t& rPlacedSet, const MapAppearance& rAppearance) const
{
    const Tile& rTile = *rPlaced.pTile;
    const TileShape_t& rShape = rPlaced.shape;
    const bool bOceanGrid = m_rGameState.GetSettings().GetMapDisplay().bOceanGrid;
    const bool bTileShrouded = rAppearance.CoverOf(rTile) == TileCover_t::Shroud;
    const struct
    {
        std::size_t neighborIndex;
        const TileVertex_t* pFrom;
        const TileVertex_t* pTo;
        bool bFront;
    } k_Edges[] = {
        {3, &rShape.west, &rShape.north, false},
        {0, &rShape.north, &rShape.east, false},
        {1, &rShape.east, &rShape.south, true},
        {2, &rShape.south, &rShape.west, true},
    };
    for (const auto& rEdge : k_Edges)
    {
        const LatticeOffset_t& offset = k_EdgeNeighbors[rEdge.neighborIndex];
        const Tile* pNeighbor =
            GetTileAtLatticeOffset(rAppearance.Map(), rTile, offset.p, offset.q);
        if (!pNeighbor || (rEdge.bFront && rPlacedSet.contains(pNeighbor)))
        {
            continue;
        }
        const bool bHidden =
            bTileShrouded || rAppearance.CoverOf(*pNeighbor) == TileCover_t::Shroud;
        const bool bLand = rTile.IsLand() && pNeighbor->IsLand();
        if (!bHidden && !bLand && !bOceanGrid)
        {
            continue;
        }
        const Color_t& rColor = bHidden || bLand ? m_rStyle.gridLandColor : m_rStyle.gridWaterColor;
        rGraphics.DrawLine(rEdge.pFrom->x, rEdge.pFrom->y, rEdge.pTo->x, rEdge.pTo->y, rColor,
                           m_rStyle.gridLineWidth);
    }
}

void MapRenderer::DrawBase_(Graphics& rGraphics, const PlacedTile_t& rPlaced,
                            const MapAppearance& rAppearance, const MapContent_t& rContent)
{
    const Tile& rTile = *rPlaced.pTile;
    if (rAppearance.CoverOf(rTile) == TileCover_t::Shroud)
    {
        return;
    }
    const BaseManager* pBase = m_rGameState.FindBaseAt(rTile.GetX(), rTile.GetY());
    if (!pBase || !rContent.showsBase(*pBase))
    {
        return;
    }
    DrawBaseArt_(rGraphics, *pBase, rPlaced.shape);
    DrawBaseName_(rGraphics, *pBase, rPlaced.shape);
}

// The bare base and its building map overlays, seated like SMAC: from FootprintOrigin down, one
// cell size for every stage (100×75 on a 100×50 footprint). Optional per-stage origin_y_ratio
// lifts stages that sit low in the faction sheet. A base whose art does not load draws none;
// its name still marks it.
void MapRenderer::DrawBaseArt_(Graphics& rGraphics, const BaseManager& rBase,
                               const TileShape_t& rShape)
{
    const float tileWidth = WidthOf_(rShape);
    const float spriteHeight =
        tileWidth * k_IsoHeightRatio * (1.0f + m_rStyle.baseSpriteOverhangRatio);
    const int sizeStage =
        BaseSpriteSizeStage(rBase.GetPopulation().GetSize(), rBase, m_rBaseSpriteSizes);
    const BaseSpriteSizeStage_t& rStage =
        m_rBaseSpriteSizes.sizeStages[static_cast<size_t>(sizeStage - 1)];
    const auto [originX, originY] = FootprintOrigin(rShape);
    const float spriteX = originX;
    const float spriteY = originY + tileWidth * rStage.originYRatio;

    const Faction& rFaction = rBase.GetFaction();
    const auto barePath = m_baseArt.EnsureBareBaseSprite(rFaction, rBase, m_rBaseSpriteSizes);
    if (!barePath)
    {
        return;
    }
    rGraphics.DrawSprite(*barePath, spriteX, spriteY, tileWidth, spriteHeight);
    if (const auto stem = FactionSheetStem(rFaction))
    {
        const auto overlays = ResolveBaseMapOverlays(
            rBase, *stem, rBase.GetTile().IsWater(), sizeStage, m_rMapOverlayChannels);
        for (const std::string& rOverlayPath : m_baseArt.EnsureOverlaySprites(overlays))
        {
            rGraphics.DrawSprite(rOverlayPath, spriteX, spriteY, tileWidth, spriteHeight);
        }
    }
}

// The name in the faction's label colour, cut to fit the tile.
void MapRenderer::DrawBaseName_(Graphics& rGraphics, const BaseManager& rBase,
                                const TileShape_t& rShape)
{
    const MapRendererStyle_t& s = m_rStyle;
    const float tileWidth = WidthOf_(rShape);
    const unsigned int fontSize = static_cast<unsigned int>(tileWidth * s.baseNameFontSizeRatio);

    Color_t nameColor = s.baseNameColor;
    if (const auto colors = m_baseArt.ColorsFor(rBase.GetFaction()))
    {
        nameColor = colors->LabelColor(s.baseNameColor);
    }

    const auto [originX, originY] = FootprintOrigin(rShape);
    // TODO: Show capture animation when base capture is implemented
    // TODO: Show population size below name
    rGraphics.DrawText(
        FitBaseName_(rGraphics, rBase.GetName(), fontSize, tileWidth * s.baseNameWidthRatio),
        originX + tileWidth * s.baseNameOffsetXRatio,
        originY + tileWidth * s.baseNameOffsetYRatio, fontSize, nameColor);
}

// One marker per tile. The selected unit wins when it stands here and the content shows it;
// otherwise the first unit the content shows. A tile whose base is drawn shows nothing unless
// that selected unit is the one. Units keep their own visibility rule, so they draw even on a
// shrouded tile when the content allows — a shrouded base is not drawn, so it does not hide them.
void MapRenderer::DrawUnits_(Graphics& rGraphics, const PlacedTile_t& rPlaced,
                             const MapAppearance& rAppearance, const MapContent_t& rContent,
                             UnitMarkerRects_t& rMarkers) const
{
    const Tile& rTile = *rPlaced.pTile;
    bool bBaseDrawn = false;
    if (rAppearance.CoverOf(rTile) != TileCover_t::Shroud && rContent.showsBase)
    {
        if (const BaseManager* pBase = m_rGameState.FindBaseAt(rTile.GetX(), rTile.GetY()))
        {
            bBaseDrawn = rContent.showsBase(*pBase);
        }
    }

    const Unit* pShown = nullptr;
    for (const Unit* pUnit : rAppearance.Map().GetAllUnitsOnTile(rTile))
    {
        if (!pUnit || !rContent.showsUnit(*pUnit))
        {
            continue;
        }
        if (pUnit == rContent.pSelectedUnit)
        {
            pShown = pUnit;
            break;
        }
        if (!pShown && !bBaseDrawn)
        {
            pShown = pUnit;
        }
    }
    if (!pShown)
    {
        return;
    }

    const float tileWidth = WidthOf_(rPlaced.shape);
    const auto [tileX, tileY] = FootprintOrigin(rPlaced.shape);
    const Rectangle_t marker = UnitMarkerRenderer::MarkerRectOnTile(tileX, tileY, tileWidth, 0);
    rMarkers[pShown->GetUnitId()] = marker;
    UnitMarkerRenderer::DrawMarker(rGraphics, *pShown, marker);
    if (pShown == rContent.pSelectedUnit)
    {
        UnitMarkerRenderer::DrawSelection(rGraphics, marker);
    }
}

} // namespace ac
