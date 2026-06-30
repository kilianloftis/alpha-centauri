#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/ImprovementRegistry.h"
#include "graphics/Graphics.h"
#include "lib/effects/ActiveEffect.h"
#include <sstream>

namespace ac
{

BaseWorkableAreaDisplay::BaseWorkableAreaDisplay(const BaseManager* pBase,
                                                 const ImprovementRegistry& rImprovements,
                                                 WindowLayout_t layout,
                                                 TileClickCallback_t onTileClicked,
                                                 BaseClickCallback_t onBaseClicked)
    : UIElement(layout)
    , m_pBase(pBase)
    , m_rImprovements(rImprovements)
    , m_onTileClicked(std::move(onTileClicked))
    , m_onBaseClicked(std::move(onBaseClicked))
{
    CacheTileRects_();
}

void BaseWorkableAreaDisplay::CacheTileRects_()
{
    if (!m_pBase)
    {
        throw std::runtime_error("BaseWorkableAreaDisplay: BaseManager is null");
    }

    m_tileSize = std::min(m_layout.width, m_layout.height) / 5.f;
    const float gridWidth  = 5 * m_tileSize;
    const float gridHeight = 5 * m_tileSize;
    m_startX = m_layout.x + (m_layout.width  - gridWidth)  / 2.f;
    m_startY = m_layout.y + (m_layout.height - gridHeight) / 2.f;

    const int baseX = m_pBase->GetX();
    const int baseY = m_pBase->GetY();

    for (const Tile* pTile : m_pBase->GetWorkableTilePositions())
    {
        if (!pTile)
        {
            continue;
        }

        const int relX = pTile->GetX() - baseX;
        const int relY = pTile->GetY() - baseY;
        const float screenX = m_startX + (relX + 2) * m_tileSize;
        const float screenY = m_startY + (relY + 2) * m_tileSize;

        m_tileRects.push_back(TileRect_t{
            Rectangle_t{screenX, screenY, m_tileSize, m_tileSize},
            pTile
        });
    }
}

void BaseWorkableAreaDisplay::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{20, 20, 20, 255});

    for (const TileRect_t& entry : m_tileRects)
    {
        RenderTile_(rGraphics, *entry.pTile, entry.rect.x, entry.rect.y, m_tileSize, entry.pTile->IsWorked());
    }

    // Draw the base itself at center
    const float centerX = m_startX + 2 * m_tileSize;
    const float centerY = m_startY + 2 * m_tileSize;
    rGraphics.DrawRect(centerX, centerY, m_tileSize, m_tileSize, Color{80, 80, 80, 255}, -1.0f);
    rGraphics.DrawText("BASE", centerX, centerY, 14, Color::Yellow());
}

void BaseWorkableAreaDisplay::RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size, bool bIsWorked)
{
    // Draw tile border (negative thickness draws inward for shared borders)
    rGraphics.DrawRect(x, y, size, size, Color{80, 80, 80, 255}, -1.0f);

    const TileResources_t yield = ResolveTileYield(rTile, m_rImprovements);
    int nutrients = yield.nutrients;
    int minerals = yield.minerals;
    int energy = yield.energy;

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

void BaseWorkableAreaDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const float mouseX = static_cast<float>(rEvent.x);
    const float mouseY = static_cast<float>(rEvent.y);

    const float baseTileX = m_startX + 2 * m_tileSize;
    const float baseTileY = m_startY + 2 * m_tileSize;

    if (ContainsMouseCoord(Rectangle_t{baseTileX, baseTileY, m_tileSize, m_tileSize}, mouseX, mouseY))
    {
        if (m_onBaseClicked)
        {
            m_onBaseClicked();
        }
        return;
    }

    for (const TileRect_t& entry : m_tileRects)
    {
        if (ContainsMouseCoord(entry.rect, mouseX, mouseY))
        {
            if (m_onTileClicked)
            {
                m_onTileClicked(entry.pTile);
            }
            return;
        }
    }
}
} // namespace ac
