#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "graphics/Graphics.h"
#include "ui/TileHitTester.h"
#include <sstream>

namespace ac
{

BaseWorkableAreaDisplay::BaseWorkableAreaDisplay(const BaseManager* pBase, WindowLayout_t layout, TileClickCallback_t onTileClicked)
    : UIElement(layout)
    , m_pBase(pBase)
    , m_onTileClicked(std::move(onTileClicked))
{}

void BaseWorkableAreaDisplay::Render(Graphics& rGraphics)
{
    if (!m_pBase)
    {
        throw std::runtime_error("BaseWorkableAreaDisplay: BaseManager is null");
    }

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{20, 20, 20, 255});
    // Get the workable tiles (5x5 grid with corners removed, excluding center)
    auto workableTiles = m_pBase->GetWorkableTilePositions();
    
    // Get the base position to center the grid
    int baseX = m_pBase->GetX();
    int baseY = m_pBase->GetY();

    // Grid is centered around base (relative coords from -2 to +2)
    // We'll lay it out in a 5x5 visual grid
    const float tileSize = GetTileSize_();
    const float gridWidth = 5 * tileSize;
    const float gridHeight = 5 * tileSize;
    
    // Center the grid within the layout rectangle
    float startX = m_layout.x + (m_layout.width - gridWidth) / 2.f;
    float startY = m_layout.y + (m_layout.height - gridHeight) / 2.f;

    for (const Tile* pTile : workableTiles)
    {
        if (!pTile)
        {
            continue;
        }

        int tileX = pTile->GetX();
        int tileY = pTile->GetY();

        // Calculate relative position from base (-2 to +2)
        int relX = tileX - baseX;
        int relY = tileY - baseY;

        // Calculate screen position (relX, relY range from -2 to +2)
        float screenX = startX + (relX + 2) * tileSize;
        float screenY = startY + (relY + 2) * tileSize;

        // Check if tile is being worked
        bool bIsWorked = m_pBase->GetWorkerAssignments().IsTileAssigned(tileX, tileY);

        RenderTile_(rGraphics, *pTile, screenX, screenY, tileSize, bIsWorked);
    }
    
    // Draw the base itself at center
    float centerX = startX + 2 * tileSize;
    float centerY = startY + 2 * tileSize;
    rGraphics.DrawRect(centerX, centerY, tileSize, tileSize, Color{80, 80, 80, 255}, -1.0f);
    rGraphics.DrawText("BASE", centerX, centerY, 14, Color::Yellow());
}

void BaseWorkableAreaDisplay::RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size, bool bIsWorked)
{
    // Draw tile border (negative thickness draws inward for shared borders)
    rGraphics.DrawRect(x, y, size, size, Color{80, 80, 80, 255}, -1.0f);

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
    
    rGraphics.DrawText(oss.str(), x + textOffsetX, y + textOffsetY, fontSize, textColor);
}

float BaseWorkableAreaDisplay::GetTileSize_() const
{
    return std::min(m_layout.width, m_layout.height) / 5.f;
}

void BaseWorkableAreaDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    auto tile = TileHitTester::HitTestBaseWorkableArea(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y),
        m_layout.x + m_layout.width / 2, m_layout.y + m_layout.height / 2,
        GetTileSize_(),
        m_pBase->GetX(), m_pBase->GetY());

    if (!tile)
    {
        return;
    }

    if (m_onTileClicked)
    {
        m_onTileClicked(tile->first, tile->second);
    }
}
} // namespace ac
