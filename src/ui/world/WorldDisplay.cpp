#include "ui/world/WorldDisplay.h"
#include <algorithm>
#include <sstream>

namespace ac
{

WorldDisplay::WorldDisplay(Graphics& rGraphics)
    : m_rGraphics(rGraphics)
{
}

void WorldDisplay::SetWorldMap(const WorldMap* pWorldMap)
{
    m_pWorldMap = pWorldMap;
}

void WorldDisplay::SetBasePositions(const std::vector<std::pair<int, int>>& basePositions)
{
    m_basePositions = basePositions;
}

void WorldDisplay::Render(float x, float y, float tileSize)
{
    if (!m_pWorldMap)
    {
        return;
    }

    const int width = m_pWorldMap->GetWidth();
    const int height = m_pWorldMap->GetHeight();

    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            const Tile* pTile = m_pWorldMap->GetTile(col, row);
            if (pTile)
            {
                float tileX = x + (col * tileSize);
                float tileY = y + (row * tileSize);
                RenderTile_(*pTile, tileX, tileY, tileSize);
            }
        }
    }

    // Draw BASE labels on tiles that contain a base
    for (const auto& pos : m_basePositions)
    {
        float baseX = x + (pos.first * tileSize);
        float baseY = y + (pos.second * tileSize);
        float textOffsetX = tileSize * 0.1f;
        float textOffsetY = tileSize * 0.05f;
        m_rGraphics.DrawText("BASE", baseX + textOffsetX, baseY + textOffsetY, 14, Color::Yellow());
    }
}

void WorldDisplay::Render(float x, float y, float w, float h)
{
    if (!m_pWorldMap)
    {
        return;
    }

    const float mapWidth = static_cast<float>(m_pWorldMap->GetWidth());
    const float mapHeight = static_cast<float>(m_pWorldMap->GetHeight());

    if (mapWidth <= 0.f || mapHeight <= 0.f)
    {
        return;
    }

    const float tileSize = std::min(w / mapWidth, h / mapHeight);
    Render(x, y, tileSize);
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

void WorldDisplay::RenderTile_(const Tile& rTile, float x, float y, float size)
{
    // Draw tile border (negative thickness draws inward for shared borders)
    m_rGraphics.DrawRect(x, y, size, size, Color{80, 80, 80, 255}, -1.0f);

    int moisture = MoistureToInt_(rTile.GetMoisture());
    int rockiness = RockinessToInt_(rTile.GetRockiness());
    int elevationKm = rTile.GetElevation() / 1000;

    std::ostringstream oss;
    oss << moisture << " " << rockiness << " " << elevationKm;

    // Center text in tile (approximate with font size 14)
    const unsigned int fontSize = 14;
    float textOffsetX = size * 0.1f;
    float textOffsetY = size * 0.3f;

    m_rGraphics.DrawText(oss.str(), x + textOffsetX, y + textOffsetY, fontSize);
}

} // namespace ac
