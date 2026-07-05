#include "ui/world/WorldDisplay.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include <algorithm>
#include <sstream>
#include <string>

namespace ac
{

WorldDisplay::WorldDisplay(WindowLayout_t layout)
    : m_layout(layout)
{
    m_effectiveTileSize = m_tileSize > 0.0f ? m_tileSize : m_layout.height * k_DefaultTileScale;
    m_visibleCols = static_cast<int>(m_layout.width  / m_effectiveTileSize);
    m_visibleRows = static_cast<int>(m_layout.height / m_effectiveTileSize);}

void WorldDisplay::SetWorldMap(const WorldMap* pWorldMap)
{
    m_pWorldMap = pWorldMap;
}

void WorldDisplay::SetBaseInfo(const std::vector<BaseInfo_t>& baseInfo)
{
    m_baseInfo = baseInfo;
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

    // Calculate font size to fit within tile (roughly 30% of tile height)
    const unsigned int fontSize = static_cast<unsigned int>(tileSize * 0.25f);
    // if (fontSize < 8) return;  // Too small to render legibly

    const float textOffsetX = tileSize * 0.1f;

    for (const auto& base : m_baseInfo)
    {
        if (base.x < colStart || base.x >= colEnd || base.y < rowStart || base.y >= rowEnd)
        {
            continue;
        }

        float baseX = m_layout.x + ((base.x - colStart) * tileSize);
        float baseY = m_layout.y + ((base.y - rowStart) * tileSize);

        // Abbreviate name to fit within tile width (approx 3 chars per line at this font size)
        const size_t maxChars = static_cast<size_t>((tileSize * 0.8f) / (fontSize * 0.5f));
        std::string displayName = base.name;
        if (displayName.length() > maxChars && maxChars > 3)
        {
            displayName = displayName.substr(0, maxChars - 1) + ".";
        }
        else if (displayName.length() > maxChars)
        {
            displayName = displayName.substr(0, maxChars);
        }

        // Render name centered vertically in upper portion of tile
        float textOffsetY = tileSize * 0.1f;

        // TODO: Use faction color for base marker based on base.factionId
        // TODO: Show capture animation if base.previousFactionId.has_value()
        // TODO: Show population size (base.populationSize) below name
        rGraphics.DrawText(displayName, baseX + textOffsetX, baseY + textOffsetY, fontSize, Color::Yellow());
    }
}

void WorldDisplay::RenderUnits_(Graphics& rGraphics, int colStart, int rowStart, int colEnd, int rowEnd)
{
    const float tileSize = GetEffectiveTileSize();

    const unsigned int fontSize = static_cast<unsigned int>(tileSize * 0.2f);
    const float markerWidth = tileSize * 0.22f;
    const float markerHeight = tileSize * 0.22f;
    const float spacing = tileSize * 0.03f;

    for (int row = rowStart; row < rowEnd; ++row)
    {
        for (int col = colStart; col < colEnd; ++col)
        {
            const Tile* pTile = m_pWorldMap->GetTile(col, row);
            if (!pTile)
            {
                continue;
            }

            const std::vector<Unit*>& units = m_pWorldMap->GetUnitsOnTile(*pTile);
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
                    Color{0, 220, 255, 255});

                if (pUnit == m_pSelectedUnit)
                {
                    rGraphics.DrawRect(
                        tileX + offsetX - 1.0f,
                        tileY + offsetY - 1.0f,
                        markerWidth + 2.0f,
                        markerHeight + 2.0f,
                        Color::Yellow(),
                        2.0f);
                }

                const std::string& unitName = pUnit->GetDesign().GetName();
                if (!unitName.empty())
                {
                    rGraphics.DrawText(
                        unitName.substr(0, 1),
                        tileX + offsetX + spacing,
                        tileY + offsetY + spacing,
                        fontSize,
                        Color::Black());
                }
            }
        }
    }
}

void WorldDisplay::Render(Graphics& rGraphics)
{
    if (!m_pWorldMap)
    {
        return;
    }

    const int mapWidth = m_pWorldMap->GetWidth();
    const int mapHeight = m_pWorldMap->GetHeight();

    if (mapWidth <= 0 || mapHeight <= 0)
    {
        return;
    }

    const int colStart = std::max(0, m_cameraX);
    const int rowStart = std::max(0, m_cameraY);
    const int colEnd   = std::min(mapWidth,  colStart + m_visibleCols);
    const int rowEnd   = std::min(mapHeight, rowStart + m_visibleRows);

    for (int row = rowStart; row < rowEnd; ++row)
    {
        for (int col = colStart; col < colEnd; ++col)
        {
            const Tile* pTile = m_pWorldMap->GetTile(col, row);
            if (pTile)
            {
                float tileX = m_layout.x + ((col - colStart) * m_effectiveTileSize);
                float tileY = m_layout.y + ((row - rowStart) * m_effectiveTileSize);
                RenderTile_(rGraphics, *pTile, tileX, tileY, m_effectiveTileSize);
            }
        }
    }

    RenderBases_(rGraphics, colStart, rowStart, colEnd, rowEnd);
    RenderUnits_(rGraphics, colStart, rowStart, colEnd, rowEnd);
}

int WorldDisplay::MoistureToInt_(Moisture moisture) const
{
    switch (moisture)
    {
        case Moisture::Wet:
            return 2;
        case Moisture::Moist:
            return 1;
        case Moisture::Arid:
        default:
            return 0;
    }
}

int WorldDisplay::RockinessToInt_(Rockiness rockiness) const
{
    switch (rockiness)
    {
        case Rockiness::Rocky:
            return 2;
        case Rockiness::Rolling:
            return 1;
        case Rockiness::Flat:
        default:
            return 0;
    }
}

void WorldDisplay::RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size)
{
    // Draw tile border (negative thickness draws inward for shared borders)
    rGraphics.DrawRect(x, y, size, size, Color{80, 80, 80, 255}, -1.0f);

    int moisture = MoistureToInt_(rTile.GetMoisture());
    int rockiness = RockinessToInt_(rTile.GetRockiness());
    int elevationKm = rTile.GetElevation() / 1000;

    std::ostringstream oss;
    oss << moisture << " " << rockiness << " " << elevationKm;

    // Center text in tile (approximate with font size 14)
    const unsigned int fontSize = 14;
    float textOffsetX = size * 0.1f;
    float textOffsetY = size * 0.3f;

    rGraphics.DrawText(oss.str(), x + textOffsetX, y + textOffsetY, fontSize);
}

} // namespace ac
