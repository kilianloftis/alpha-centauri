#include "ui/world/WorldDisplay.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include <algorithm>
#include <sstream>
#include <string>

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
constexpr float k_UnitMarkerFontSizeRatio       = 0.2f;
constexpr float k_UnitMarkerWidthRatio          = 0.22f;
constexpr float k_UnitMarkerHeightRatio         = 0.22f;
constexpr float k_UnitMarkerSpacingRatio        = 0.03f;
constexpr Color_t k_UnitMarkerColor               {0, 220, 255, 255};
constexpr float k_SelectionBorderOffset         = 1.0f;
constexpr float k_SelectionBorderExpansion      = 2.0f;
constexpr float k_SelectionBorderWidth          = 2.0f;
constexpr size_t k_UnitNameFirstCharCount       = 1;
constexpr int   k_MoistureWetValue              = 2;
constexpr int   k_MoistureMoistValue            = 1;
constexpr int   k_MoistureAridValue             = 0;
constexpr int   k_RockinessRockyValue           = 2;
constexpr int   k_RockinessRollingValue         = 1;
constexpr int   k_RockinessFlatValue            = 0;
constexpr Color_t k_TileBorderColor               {80, 80, 80, 255};
constexpr Color_t k_ShroudColor                   {0, 0, 0, 255};
constexpr Color_t k_FogTerrainColor               {110, 110, 110, 255};
constexpr float k_TileBorderWidth               = -1.0f;
constexpr int   k_ElevationMetersPerKm          = 1000;
constexpr unsigned int k_TileFontSize           = 14;
constexpr float k_TileTextOffsetXRatio          = 0.1f;
constexpr float k_TileTextOffsetYRatio          = 0.3f;

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

void WorldDisplay::SetSelectedUnit(const Unit* pUnit)
{
    m_pSelectedUnit = pUnit;
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

void WorldDisplay::RenderUnits_(Graphics& rGraphics, int colStart, int rowStart, int colEnd, int rowEnd)
{
    const WorldMap& rWorldMap = GetWorldMap_();
    const float tileSize = GetEffectiveTileSize();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    const unsigned int fontSize = static_cast<unsigned int>(tileSize * k_UnitMarkerFontSizeRatio);
    const float markerWidth = tileSize * k_UnitMarkerWidthRatio;
    const float markerHeight = tileSize * k_UnitMarkerHeightRatio;
    const float spacing = tileSize * k_UnitMarkerSpacingRatio;

    for (int row = rowStart; row < rowEnd; ++row)
    {
        for (int col = colStart; col < colEnd; ++col)
        {
            const Tile* pTile = rWorldMap.GetTile(col, row);
            if (!pTile)
            {
                continue;
            }

            // Units only appear on currently-visible tiles (fog hides them).
            if (fog.visible && !fog.visible->IsVisible(*pTile))
            {
                continue;
            }

            const std::vector<Unit*>& units = rWorldMap.GetUnitsOnTile(*pTile);
            if (units.empty())
            {
                continue;
            }

            const float tileX = m_layout.x + ((col - colStart) * tileSize);
            const float tileY = m_layout.y + ((row - rowStart) * tileSize);

            for (size_t i = 0; i < units.size(); ++i)
            {
                const Unit* pUnit = units[i];
                if (!pUnit)
                {
                    continue;
                }

                const float offsetX = spacing + (i * (markerWidth + spacing));
                const float offsetY = tileSize - markerHeight - spacing;

                // TODO: Use faction color based on pUnit->GetFaction().
                rGraphics.DrawFilledRect(
                    tileX + offsetX,
                    tileY + offsetY,
                    markerWidth,
                    markerHeight,
                    k_UnitMarkerColor);

                if (pUnit == m_pSelectedUnit)
                {
                    rGraphics.DrawRect(
                        tileX + offsetX - k_SelectionBorderOffset,
                        tileY + offsetY - k_SelectionBorderOffset,
                        markerWidth + k_SelectionBorderExpansion,
                        markerHeight + k_SelectionBorderExpansion,
                        Color_t::Yellow(),
                        k_SelectionBorderWidth);
                }

                const std::string& unitName = pUnit->GetDesign().GetName();
                if (!unitName.empty())
                {
                    rGraphics.DrawText(
                        unitName.substr(0, k_UnitNameFirstCharCount),
                        tileX + offsetX + spacing,
                        tileY + offsetY + spacing,
                        fontSize,
                        Color_t::Black());
                }
            }
        }
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
            RenderTile_(rGraphics, *pTile, tileX, tileY, m_effectiveTileSize, bFogged);
        }
    }

    RenderBases_(rGraphics, colStart, rowStart, colEnd, rowEnd);
    RenderUnits_(rGraphics, colStart, rowStart, colEnd, rowEnd);
}

int WorldDisplay::MoistureToInt_(Moisture_t moisture) const
{
    switch (moisture)
    {
        case Moisture_t::Wet:
            return k_MoistureWetValue;
        case Moisture_t::Moist:
            return k_MoistureMoistValue;
        case Moisture_t::Arid:
        default:
            return k_MoistureAridValue;
    }
}

int WorldDisplay::RockinessToInt_(Rockiness_t rockiness) const
{
    switch (rockiness)
    {
        case Rockiness_t::Rocky:
            return k_RockinessRockyValue;
        case Rockiness_t::Rolling:
            return k_RockinessRollingValue;
        case Rockiness_t::Flat:
        default:
            return k_RockinessFlatValue;
    }
}

void WorldDisplay::RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                               bool bFogged)
{
    rGraphics.DrawRect(x, y, size, size, k_TileBorderColor, k_TileBorderWidth);

    int moisture = MoistureToInt_(rTile.GetMoisture());
    int rockiness = RockinessToInt_(rTile.GetRockiness());
    int elevationKm = rTile.GetElevation() / k_ElevationMetersPerKm;

    std::ostringstream oss;
    oss << moisture << " " << rockiness << " " << elevationKm;

    float textOffsetX = size * k_TileTextOffsetXRatio;
    float textOffsetY = size * k_TileTextOffsetYRatio;

    const Color_t textColor = bFogged ? k_FogTerrainColor : Color_t::White();
    rGraphics.DrawText(oss.str(), x + textOffsetX, y + textOffsetY, k_TileFontSize, textColor);
}

} // namespace ac
