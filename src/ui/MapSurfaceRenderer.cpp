#include "ui/MapSurfaceRenderer.h"

#include "game/Faction.h"
#include "game/buildings/BaseSpriteSizesConfig.h"
#include "game/buildings/MapOverlayChannelsConfig.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "ui/style/UiStyle.h"

namespace ac
{

namespace
{

constexpr float k_IsoHeightRatio = 0.5f;

} // namespace

MapSurfaceRenderer::MapSurfaceRenderer(const BaseSpriteSizesConfig_t& rBaseSpriteSizes,
                                       const MapOverlayChannelsConfig_t& rMapOverlayChannels)
    : m_rBaseSpriteSizes(rBaseSpriteSizes)
    , m_rMapOverlayChannels(rMapOverlayChannels)
{
}

void MapSurfaceRenderer::DrawFullDiamondGrid_(Graphics& rGraphics, const Tile& rTile,
                                              const TileShape_t& rShape) const
{
    const auto& s = Style().tileRenderer;
    const Color_t& rColor = rTile.IsLand() ? s.gridLandColor : s.gridWaterColor;
    const struct
    {
        const TileVertex_t* pFrom;
        const TileVertex_t* pTo;
    } k_Edges[] = {
        {&rShape.west, &rShape.north},
        {&rShape.north, &rShape.east},
        {&rShape.east, &rShape.south},
        {&rShape.south, &rShape.west},
    };
    for (const auto& rEdge : k_Edges)
    {
        rGraphics.DrawLine(rEdge.pFrom->x, rEdge.pFrom->y, rEdge.pTo->x, rEdge.pTo->y, rColor,
                           s.gridLineWidth);
    }
}

void MapSurfaceRenderer::DrawWorldMapGridEdges_(Graphics& rGraphics, const Tile& rTile,
                                                const TileShape_t& rShape, const WorldMap& rMap,
                                                bool bOceanGrid,
                                                const ExploredFn_t& rExplored) const
{
    // NW and NE only so every edge is drawn once; raised tiles in front cover lines behind.
    // Edges touching water need the ocean grid; anything next to unexplored ground uses the
    // land colour so the grid does not reveal coastlines.
    const auto& s = Style().tileRenderer;
    const auto explored = [&rExplored](const Tile& rAny) {
        return !rExplored || rExplored(rAny);
    };
    const struct
    {
        int dx;
        int dy;
        const TileVertex_t* pFrom;
        const TileVertex_t* pTo;
    } k_Edges[] = {
        {-1, 0, &rShape.west, &rShape.north},
        {0, -1, &rShape.north, &rShape.east},
    };
    for (const auto& rEdge : k_Edges)
    {
        const Tile* pNeighbor = GetTileAtLatticeOffset(rMap, rTile, rEdge.dx, rEdge.dy);
        if (!pNeighbor)
        {
            continue;
        }
        const bool bHidden = !explored(rTile) || !explored(*pNeighbor);
        const bool bLand = rTile.IsLand() && pNeighbor->IsLand();
        if (!bHidden && !bLand && !bOceanGrid)
        {
            continue;
        }
        const Color_t& rColor = bHidden || bLand ? s.gridLandColor : s.gridWaterColor;
        rGraphics.DrawLine(rEdge.pFrom->x, rEdge.pFrom->y, rEdge.pTo->x, rEdge.pTo->y, rColor,
                           s.gridLineWidth);
    }
}

void MapSurfaceRenderer::RenderTile(Graphics& rGraphics, const Tile& rTile,
                                   const TileShape_t& rShape, const WorldMap& rMap, bool bFogged,
                                   bool bShrouded, const YieldLookup_t& rYieldOf,
                                   MapGridStyle_t gridStyle, bool bOceanGrid,
                                   const ExploredFn_t& rExplored)
{
    if (bShrouded)
    {
        rGraphics.FillTileShape(rShape, Style().worldDisplay.shroudColor);
    }
    else
    {
        TileRenderer::RenderTerrain(rGraphics, rTile, rShape, bFogged, &rMap);
    }

    switch (gridStyle)
    {
    case MapGridStyle_t::None:
        break;
    case MapGridStyle_t::FullDiamond:
        DrawFullDiamondGrid_(rGraphics, rTile, rShape);
        break;
    case MapGridStyle_t::WorldMapEdges:
        DrawWorldMapGridEdges_(rGraphics, rTile, rShape, rMap, bOceanGrid, rExplored);
        break;
    }

    if (!bShrouded)
    {
        TileRenderer::RenderObjects(rGraphics, rTile, rShape, rYieldOf);
    }
}

bool MapSurfaceRenderer::RenderBase(Graphics& rGraphics, const BaseManager& rBase,
                                    const TileShape_t& rShape)
{
    const float tileWidth = rShape.east.x - rShape.west.x;
    const float tileHeight = tileWidth * k_IsoHeightRatio;
    const float seatY =
        (rShape.west.y + rShape.north.y + rShape.east.y + rShape.south.y) * 0.25f;
    const float spriteX = rShape.west.x;
    const float spriteY = seatY - tileHeight * 0.5f;
    const float spriteHeight =
        tileHeight * (1.0f + Style().worldDisplay.baseSpriteOverhangRatio);

    const Faction& rFaction = rBase.GetFaction();
    const auto barePath =
        m_art.EnsureBareBaseSprite(rGraphics, rFaction, rBase, m_rBaseSpriteSizes);
    if (!barePath)
    {
        return false;
    }
    rGraphics.DrawSprite(*barePath, spriteX, spriteY, tileWidth, spriteHeight);
    if (const auto stem = FactionSheetStem(rFaction))
    {
        const int sizeStage =
            BaseSpriteSizeStage(rBase.GetPopulation().GetSize(), rBase, m_rBaseSpriteSizes);
        const auto overlays = ResolveBaseMapOverlays(
            rBase, *stem, rBase.GetTile().IsWater(), sizeStage, m_rMapOverlayChannels);
        for (const std::string& rOverlayPath : m_art.EnsureOverlaySprites(rGraphics, overlays))
        {
            rGraphics.DrawSprite(rOverlayPath, spriteX, spriteY, tileWidth, spriteHeight);
        }
    }
    return true;
}

std::optional<FactionColors_t> MapSurfaceRenderer::ColorsFor(const Faction& rFaction)
{
    return m_art.ColorsFor(rFaction);
}

} // namespace ac
