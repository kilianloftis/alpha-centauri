#include "ui/world/WorldDisplay.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Pathfinder.h"
#include "game/units/Unit.h"
#include "ui/TileRenderer.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

namespace
{

constexpr float k_DefaultTileScale              = 1.0f / 10.0f;
constexpr float k_BaseNameFontSizeRatio         = 0.25f;
constexpr float k_BaseTextOffsetRatio           = 0.1f;
constexpr float k_BaseNameWidthRatio            = 0.8f;
constexpr float k_BaseNameCharWidthRatio        = 0.5f;
constexpr size_t k_BaseNameMinTruncChars        = 3;
constexpr Color_t k_SensorMarkerColor             {40, 220, 120, 255};
constexpr float k_SensorMarkerFontSizeRatio       = 0.22f;
constexpr float k_SensorMarkerWidthRatio          = 0.28f;
constexpr float k_SensorMarkerHeightRatio         = 0.28f;
constexpr float k_SensorMarkerInsetRatio          = 0.04f;
constexpr Color_t k_ShroudColor                   {0, 0, 0, 255};
constexpr Color_t k_PathPreviewColor              {255, 0, 0, 220};
constexpr float k_PathPreviewLineThicknessRatio  = 0.08f;

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
    , m_layout(layout)
{
    m_effectiveTileSize = m_tileSize > 0.0f ? m_tileSize : m_layout.height * k_DefaultTileScale;
    m_visibleCols = static_cast<int>(m_layout.width  / m_effectiveTileSize);
    m_visibleRows = static_cast<int>(m_layout.height / m_effectiveTileSize);
}

const WorldMap& WorldDisplay::GetWorldMap_() const
{
    return m_rGameState.GetWorldMap();
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

void WorldDisplay::SetCameraOffset(int tileX, int tileY)
{
    m_cameraX = tileX;
    m_cameraY = tileY;
}

float WorldDisplay::GetEffectiveTileSize() const
{
    return m_effectiveTileSize;
}

int WorldDisplay::GetCameraX() const
{
    return m_cameraX;
}

int WorldDisplay::GetCameraY() const
{
    return m_cameraY;
}

int WorldDisplay::GetVisibleCols() const
{
    return m_visibleCols;
}

int WorldDisplay::GetVisibleRows() const
{
    return m_visibleRows;
}

void WorldDisplay::RenderBases_(Graphics& rGraphics, int colStart, int rowStart, int colEnd, int rowEnd)
{
    const float tileSize = GetEffectiveTileSize();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    const unsigned int fontSize = static_cast<unsigned int>(tileSize * k_BaseNameFontSizeRatio);
    const float textOffsetX = tileSize * k_BaseTextOffsetRatio;

    for (const Faction& rFaction : m_rGameState.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            const int baseXTile = rBase.GetX();
            const int baseYTile = rBase.GetY();
            if (baseXTile < colStart || baseXTile >= colEnd
                || baseYTile < rowStart || baseYTile >= rowEnd)
            {
                continue;
            }

            // Shroud hides bases entirely; fog still shows last-known bases.
            if (fog.explored && !fog.explored->IsExplored(baseXTile, baseYTile))
            {
                continue;
            }

            const float baseX = m_layout.x + ((baseXTile - colStart) * tileSize);
            const float baseY = m_layout.y + ((baseYTile - rowStart) * tileSize);

            const size_t maxChars = static_cast<size_t>(
                (tileSize * k_BaseNameWidthRatio) / (fontSize * k_BaseNameCharWidthRatio));
            std::string displayName = rBase.GetName();
            if (displayName.length() > maxChars && maxChars > k_BaseNameMinTruncChars)
            {
                displayName = displayName.substr(0, maxChars - 1) + ".";
            }
            else if (displayName.length() > maxChars)
            {
                displayName = displayName.substr(0, maxChars);
            }

            const float textOffsetY = tileSize * k_BaseTextOffsetRatio;

            // TODO: Use faction color for base marker based on rBase.GetFactionId()
            // TODO: Show capture animation when base capture is implemented
            // TODO: Show population size below name
            rGraphics.DrawText(displayName, baseX + textOffsetX, baseY + textOffsetY, fontSize, Color_t::Yellow());
        }
    }
}

void WorldDisplay::RenderSensors_(Graphics& rGraphics, int colStart, int rowStart, int colEnd, int rowEnd)
{
    const WorldMap& rWorldMap = GetWorldMap_();
    const float tileSize = GetEffectiveTileSize();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    const unsigned int fontSize = static_cast<unsigned int>(tileSize * k_SensorMarkerFontSizeRatio);
    const float markerWidth = tileSize * k_SensorMarkerWidthRatio;
    const float markerHeight = tileSize * k_SensorMarkerHeightRatio;
    const float inset = tileSize * k_SensorMarkerInsetRatio;

    for (int row = rowStart; row < rowEnd; ++row)
    {
        for (int col = colStart; col < colEnd; ++col)
        {
            const Tile* pTile = rWorldMap.GetTile(col, row);
            if (!pTile || !pTile->HasImprovement("Sensor"))
            {
                continue;
            }

            // Shroud hides sensors; fog still shows last-known towers (same as bases).
            if (fog.explored && !fog.explored->IsExplored(*pTile))
            {
                continue;
            }

            const float tileX = m_layout.x + ((col - colStart) * tileSize);
            const float tileY = m_layout.y + ((row - rowStart) * tileSize);
            // Top-right corner so the marker stays clear of base names and unit chips.
            const float markerX = tileX + tileSize - markerWidth - inset;
            const float markerY = tileY + inset;

            rGraphics.DrawFilledRect(markerX, markerY, markerWidth, markerHeight, k_SensorMarkerColor);
            rGraphics.DrawText("S", markerX + inset, markerY + inset, fontSize, Color_t::Black());
        }
    }
}

void WorldDisplay::RenderPathPreview_(Graphics& rGraphics, int colStart, int rowStart)
{
    if (!m_pPathPreview || m_pPathPreview->tiles.empty())
    {
        return;
    }

    const float halfTile = m_effectiveTileSize * 0.5f;
    const auto tileCenter = [&](const Tile& rTile) -> std::pair<float, float> {
        return {
            m_layout.x + ((rTile.GetX() - colStart) * m_effectiveTileSize) + halfTile,
            m_layout.y + ((rTile.GetY() - rowStart) * m_effectiveTileSize) + halfTile,
        };
    };

    // Path_t excludes the origin; start from the selected unit so the line begins on its tile.
    std::vector<std::pair<float, float>> centers;
    centers.reserve(m_pPathPreview->tiles.size() + 1);
    if (m_pSelectedUnit)
    {
        centers.push_back(tileCenter(m_pSelectedUnit->GetTile()));
    }
    for (const Tile* pTile : m_pPathPreview->tiles)
    {
        if (pTile)
        {
            centers.push_back(tileCenter(*pTile));
        }
    }

    if (centers.size() < 2)
    {
        return;
    }

    const float thickness = std::max(1.0f, m_effectiveTileSize * k_PathPreviewLineThicknessRatio);
    for (size_t i = 1; i < centers.size(); ++i)
    {
        rGraphics.DrawLine(centers[i - 1].first, centers[i - 1].second,
                           centers[i].first, centers[i].second,
                           k_PathPreviewColor, thickness);
    }
}

void WorldDisplay::Render(Graphics& rGraphics)
{
    const WorldMap& rWorldMap = GetWorldMap_();
    const int mapWidth = rWorldMap.GetWidth();
    const int mapHeight = rWorldMap.GetHeight();

    if (mapWidth <= 0 || mapHeight <= 0)
    {
        return;
    }

    const int colStart = std::max(0, m_cameraX);
    const int rowStart = std::max(0, m_cameraY);
    const int colEnd   = std::min(mapWidth,  colStart + m_visibleCols);
    const int rowEnd   = std::min(mapHeight, rowStart + m_visibleRows);
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    for (int row = rowStart; row < rowEnd; ++row)
    {
        for (int col = colStart; col < colEnd; ++col)
        {
            const Tile* pTile = rWorldMap.GetTile(col, row);
            if (!pTile)
            {
                continue;
            }

            float tileX = m_layout.x + ((col - colStart) * m_effectiveTileSize);
            float tileY = m_layout.y + ((row - rowStart) * m_effectiveTileSize);

            if (fog.explored && !fog.explored->IsExplored(*pTile))
            {
                rGraphics.DrawFilledRect(tileX, tileY, m_effectiveTileSize, m_effectiveTileSize, k_ShroudColor);
                continue;
            }

            const bool bFogged = fog.visible && !fog.visible->IsVisible(*pTile);
            TileRenderer::Render(rGraphics, *pTile, tileX, tileY, m_effectiveTileSize, bFogged);
        }
    }

    RenderBases_(rGraphics, colStart, rowStart, colEnd, rowEnd);
    RenderPathPreview_(rGraphics, colStart, rowStart);
    RenderSensors_(rGraphics, colStart, rowStart, colEnd, rowEnd);
    m_unitMarkers.Render(rGraphics, m_rGameState, m_layout, m_effectiveTileSize,
                         colStart, rowStart, colEnd, rowEnd);
}

} // namespace ac
