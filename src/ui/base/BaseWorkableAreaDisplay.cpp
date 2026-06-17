#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include <sstream>

namespace ac
{

BaseWorkableAreaDisplay::BaseWorkableAreaDisplay(Graphics& rGraphics, const BaseManager* pBase, ResolvedLayout_t layout)
    : m_layout(layout)
    , m_rGraphics(rGraphics)
    , m_pBase(pBase)
{}

void BaseWorkableAreaDisplay::Render()
{
    if (!m_pBase)
    {
        throw std::runtime_error("BaseWorkableAreaDisplay: BaseManager is null");
    }

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
    
    // Offset to center the grid at the render position
    float startX = m_layout.x - (gridWidth / 2) + (tileSize / 2);
    float startY = m_layout.y - (gridHeight / 2) + (tileSize / 2);

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

        RenderTile_(*pTile, screenX, screenY, tileSize, bIsWorked);
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

float BaseWorkableAreaDisplay::GetTileSize_() const
{
    return static_cast<float>(m_layout.width) * kTileSizeRatio;
}

void BaseWorkableAreaDisplay::HandleMouse(const MouseEvent_t& rEvent)
{
    auto tile = TileHitTester::HitTestBaseWorkableArea(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y),
        m_layout.x + m_layout.width / 2, m_layout.y + m_layout.height / 2,
        GetTileSize_(),
        m_pBase->GetX(), m_pBase->GetY());

    if (!tile)
    {
        m_lastClickedTile = std::nullopt;
        return;
    }

    m_lastClickedTile = tile;
    int tileX = tile->first;
    int tileY = tile->second;
    auto& rAssignments = m_pBase->GetWorkerAssignments();
    const auto& rPops = m_pBase->GetPopContainer();

    if (rAssignments.IsTileAssigned(tileX, tileY))
    {
        for (const auto& rEntry : rAssignments.GetAssignments())
        {
            if (rEntry.second.first == tileX && rEntry.second.second == tileY)
            {
                rAssignments.UnassignWorker(rEntry.first);
                return;
            }
        }
    }
    else
    {
        const auto& rPopsVec = rPops.GetPops();
        for (int i = static_cast<int>(rPopsVec.size()) - 1; i >= 0; --i)
        {
            const Pop* pPop = rPopsVec[i].get();
            if (pPop->IsWorker() && rAssignments.GetAssignedTile(pPop->GetId()).first == -1)
            {
                rAssignments.UnassignWorker(pPop->GetId());
                if (rAssignments.AssignWorker(pPop->GetId(), tileX, tileY, rPops))
                {
                    return;
                }
            }
        }
    }
}
} // namespace ac
