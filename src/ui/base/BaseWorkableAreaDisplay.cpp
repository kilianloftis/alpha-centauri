#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include <sstream>

namespace ac
{

BaseWorkableAreaDisplay::BaseWorkableAreaDisplay(Graphics& rGraphics, const WorldMap& rWorldMap)
    : m_rGraphics(rGraphics)
    , m_rWorldMap(rWorldMap)
{
}

void BaseWorkableAreaDisplay::SetBase(const BaseManager* pBase)
{
    m_pBase = pBase;
}

void BaseWorkableAreaDisplay::Render(Graphics& /*rGraphics*/)
{
    Render(kBaseAreaCenterX, kBaseAreaCenterY, kBaseTileSize);
}

void BaseWorkableAreaDisplay::Render(float x, float y, float tileSize)
{
    if (!m_pBase)
    {
        return;
    }

    // Get the workable tiles (5x5 grid with corners removed, excluding center)
    auto workableTiles = m_pBase->GetWorkableTilePositions();
    
    // Get the base position to center the grid
    int baseX = m_pBase->GetX();
    int baseY = m_pBase->GetY();

    // Grid is centered around base (relative coords from -2 to +2)
    // We'll lay it out in a 5x5 visual grid
    const float gridWidth = 5 * tileSize;
    const float gridHeight = 5 * tileSize;
    
    // Offset to center the grid at the render position
    float startX = x - (gridWidth / 2) + (tileSize / 2);
    float startY = y - (gridHeight / 2) + (tileSize / 2);

    for (const auto& tileCoord : workableTiles)
    {
        int tileX = tileCoord.first;
        int tileY = tileCoord.second;
        
        // Calculate relative position from base (-2 to +2)
        int relX = tileX - baseX;
        int relY = tileY - baseY;
        
        // Skip center (base position itself, already excluded by GetWorkableTilePositions)
        // and corners (Manhattan distance > 3, already excluded)
        
        const Tile* pTile = m_rWorldMap.GetTile(tileX, tileY);
        if (pTile)
        {
            // Calculate screen position (relX, relY range from -2 to +2)
            float screenX = startX + (relX + 2) * tileSize;
            float screenY = startY + (relY + 2) * tileSize;
            
            // Check if tile is being worked
            bool bIsWorked = m_pBase->GetWorkerAssignments().IsTileAssigned(tileX, tileY);
            
            RenderTile_(*pTile, screenX, screenY, tileSize, bIsWorked);
        }
    }
    
    // Draw the base itself at center
    float centerX = startX + 2 * tileSize;
    float centerY = startY + 2 * tileSize;
    m_rGraphics.DrawRect(centerX, centerY, tileSize, tileSize, Color{80, 80, 80, 255}, -1.0f);
    m_rGraphics.DrawText("BASE", centerX, centerY, 14, Color::Yellow());
}

void BaseWorkableAreaDisplay::RenderTile_(const Tile& rTile, float x, float y, float size, bool bIsWorked)
{
    // Draw tile border (negative thickness draws inward for shared borders)
    m_rGraphics.DrawRect(x, y, size, size, Color{80, 80, 80, 255}, -1.0f);

    int nutrients = rTile.GetNutrientProduction();
    int minerals = rTile.GetMineralProduction();
    int energy = rTile.GetEnergyProduction();

    std::ostringstream oss;
    oss << nutrients << " " << minerals << " " << energy;

    // Center text in tile with smaller font
    const unsigned int fontSize = 12;
    float textOffsetX = size * 0.05f;
    float textOffsetY = size * 0.35f;

    // Use green for worked tiles, white for unworked
    Color textColor = bIsWorked ? Color::Green() : Color::White();
    
    m_rGraphics.DrawText(oss.str(), x + textOffsetX, y + textOffsetY, fontSize, textColor);
}

} // namespace ac
