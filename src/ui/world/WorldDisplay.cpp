#include "ui/world/WorldDisplay.h"
#include <algorithm>
#include <sstream>
#include <string>

namespace ac
{

void WorldDisplay::SetWorldMap(const WorldMap* pWorldMap)
{
    m_pWorldMap = pWorldMap;
}

void WorldDisplay::SetBaseInfo(const std::vector<BaseInfo_t>& baseInfo)
{
    m_baseInfo = baseInfo;
}

void WorldDisplay::Render(Graphics& rGraphics, float x, float y, float tileSize)
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
                RenderTile_(rGraphics, *pTile, tileX, tileY, tileSize);
            }
        }
    }

    RenderBases_(rGraphics, x, y, tileSize);
}

void WorldDisplay::RenderBases_(Graphics& rGraphics, float originX, float originY, float tileSize)
{
    // Calculate font size to fit within tile (roughly 30% of tile height)
    const unsigned int fontSize = static_cast<unsigned int>(tileSize * 0.25f);
    // if (fontSize < 8) return;  // Too small to render legibly

    const float textOffsetX = tileSize * 0.1f;

    for (const auto& base : m_baseInfo)
    {
        float baseX = originX + (base.x * tileSize);
        float baseY = originY + (base.y * tileSize);

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

void WorldDisplay::Render(Graphics& rGraphics, float x, float y, float w, float h)
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
    Render(rGraphics, x, y, tileSize);
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
