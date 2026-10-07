#include "ui/world/WorldDisplay.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/effects/TileEffectsContext.h"
#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Pathfinder.h"
#include "game/units/Unit.h"
#include "ui/TileRenderer.h"
#include "ui/style/UiStyle.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

namespace
{

constexpr size_t k_BaseNameMinTruncChars        = 3;

struct PlayerFogMaps_t
{
    const FactionExploredMap* explored = nullptr;
    const FactionVisibleMap* visible = nullptr;
};

PlayerFogMaps_t PlayerFog_(const GameState& rGameState)
{
    const Faction* pPlayer = rGameState.GetPlayerFaction();
    if (!pPlayer || !pPlayer->GetExploredMap().IsSized() || !pPlayer->GetVisibleMap().IsSized())
    {
        return {};
    }
    return {&pPlayer->GetExploredMap(), &pPlayer->GetVisibleMap()};
}

} // namespace

WorldDisplay::WorldDisplay(const GameState& rGameState, WindowLayout_t layout)
    : m_rGameState(rGameState)
    , m_viewport(rGameState.GetWorldMap(), layout,
                 layout.height * Style().worldDisplay.defaultTileScale)
{
}

void WorldDisplay::SetPathPreview(const Path_t* pPath)
{
    m_pPathPreview = pPath;
}

void WorldDisplay::SetSelectedUnit(const Unit* pUnit)
{
    m_pSelectedUnit = pUnit;
    m_unitMarkers.SetSelectedUnit(pUnit);
}

void WorldDisplay::SetPlaybackVisibleUnits(const std::unordered_set<UnitId_t>* pUnitIds)
{
    m_unitMarkers.SetPlaybackVisibleUnits(pUnitIds);
}

float WorldDisplay::GetEffectiveTileSize() const
{
    return m_viewport.TileSize();
}

int WorldDisplay::GetVisibleRows() const
{
    return m_viewport.VisibleRows();
}

void WorldDisplay::RenderBases_(Graphics& rGraphics)
{
    const auto& s = Style().worldDisplay;
    const float tileSize = m_viewport.TileSize();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    const unsigned int fontSize = static_cast<unsigned int>(tileSize * s.baseNameFontSizeRatio);
    const float textOffsetX = tileSize * s.baseTextOffsetRatio;

    for (const Faction& rFaction : m_rGameState.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            const Tile& rBaseTile = rBase.GetTile();
            const auto origin = m_viewport.PixelOriginOf(rBaseTile.GetX(), rBaseTile.GetY());
            if (!origin)
            {
                continue;
            }

            // Shroud hides bases entirely; fog still shows last-known bases.
            if (fog.explored && !fog.explored->IsExplored(rBaseTile))
            {
                continue;
            }

            const size_t maxChars = static_cast<size_t>(
                (tileSize * s.baseNameWidthRatio) / (fontSize * s.baseNameCharWidthRatio));
            std::string displayName = rBase.GetName();
            if (displayName.length() > maxChars && maxChars > k_BaseNameMinTruncChars)
            {
                displayName = displayName.substr(0, maxChars - 1) + ".";
            }
            else if (displayName.length() > maxChars)
            {
                displayName = displayName.substr(0, maxChars);
            }

            const float textOffsetY = tileSize * s.baseTextOffsetRatio;

            // TODO: Use faction color for base marker based on rBase.GetFactionId()
            // TODO: Show capture animation when base capture is implemented
            // TODO: Show population size below name
            rGraphics.DrawText(displayName, origin->first + textOffsetX, origin->second + textOffsetY,
                               fontSize, s.baseNameColor);
        }
    }
}

void WorldDisplay::RenderPathPreview_(Graphics& rGraphics)
{
    if (!m_pPathPreview || m_pPathPreview->tiles.empty())
    {
        return;
    }

    const auto& s = Style().worldDisplay;

    // Path_t excludes the origin; start from the selected unit so the line begins on its tile.
    std::vector<std::pair<float, float>> centers;
    centers.reserve(m_pPathPreview->tiles.size() + 1);
    if (m_pSelectedUnit)
    {
        if (const auto center = m_viewport.PixelCenterOf(m_pSelectedUnit->GetTile()))
        {
            centers.push_back(*center);
        }
    }
    for (const Tile* pTile : m_pPathPreview->tiles)
    {
        if (!pTile)
        {
            continue;
        }
        if (const auto center = m_viewport.PixelCenterOf(*pTile))
        {
            centers.push_back(*center);
        }
    }

    if (centers.size() < 2)
    {
        return;
    }

    const float thickness = std::max(1.0f, m_viewport.TileSize() * s.pathPreviewLineThicknessRatio);
    for (size_t i = 1; i < centers.size(); ++i)
    {
        rGraphics.DrawLine(centers[i - 1].first, centers[i - 1].second,
                           centers[i].first, centers[i].second,
                           s.pathPreviewColor, thickness);
    }
}

// Grid lines along the tile's NW and NE edges, so every edge is drawn once and raised tiles in
// front cover the lines behind them. Edges touching water need the ocean grid; anything next to
// unexplored ground uses the land colour so the grid does not reveal coastlines.
void WorldDisplay::RenderGridEdges_(Graphics& rGraphics, const Tile& rTile,
                                    const TileShape_t& rShape, bool bOceanGrid) const
{
    const auto& s = Style().tileRenderer;
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);
    const auto explored = [&fog](const Tile& rAny) {
        return !fog.explored || fog.explored->IsExplored(rAny);
    };
    const WorldMap& rWorldMap = m_viewport.GetWorldMap();
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
        const Tile* pNeighbor = GetTileAtLatticeOffset(rWorldMap, rTile, rEdge.dx, rEdge.dy);
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

void WorldDisplay::Render(Graphics& rGraphics)
{
    const WorldMap& rWorldMap = m_viewport.GetWorldMap();
    if (rWorldMap.GetWidth() <= 0 || rWorldMap.GetHeight() <= 0)
    {
        return;
    }

    const MapDisplayConfig_t& rDisplay = m_rGameState.GetSettings().GetMapDisplay();
    m_viewport.SetRelief(rDisplay.relief, Style().tileRenderer.relief);
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);
    const TileRenderer::YieldLookup_t yieldOf = [this](const Tile& rTile) {
        return m_rGameState.GetTileEffects().ResolveTileYield(rTile).effective;
    };

    m_viewport.ForEachVisibleTile([&](const Tile& rTile, const TileShape_t& rShape) {
        if (fog.explored && !fog.explored->IsExplored(rTile))
        {
            rGraphics.FillTileShape(rShape, Style().worldDisplay.shroudColor);
            RenderGridEdges_(rGraphics, rTile, rShape, rDisplay.bOceanGrid);
            return;
        }
        // SMAC draws a tile's grid lines over its terrain and under its objects.
        const bool bFogged = fog.visible && !fog.visible->IsVisible(rTile);
        TileRenderer::RenderTerrain(rGraphics, rTile, rShape, bFogged, &rWorldMap);
        RenderGridEdges_(rGraphics, rTile, rShape, rDisplay.bOceanGrid);
        TileRenderer::RenderObjects(rGraphics, rTile, rShape, yieldOf);
    });

    RenderBases_(rGraphics);
    RenderPathPreview_(rGraphics);
    m_unitMarkers.Render(rGraphics, m_rGameState, m_viewport);
}

} // namespace ac
