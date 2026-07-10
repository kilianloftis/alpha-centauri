#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "graphics/Graphics.h"
#include "game/effects/TileEffectsContext.h"
#include <sstream>

namespace ac
{

namespace
{

constexpr int   k_GridDimension             = 5;
constexpr float k_GridCenterOffset          = 2.0f;
constexpr Color_t k_BackgroundColor           {20, 20, 20, 255};
constexpr Color_t k_TileBorderColor           {80, 80, 80, 255};
constexpr float k_TileBorderWidth           = -1.0f;
constexpr unsigned int k_BaseLabelFontSize  = 14;
constexpr unsigned int k_TileFontSize         = 12;
constexpr float k_TileTextOffsetXRatio      = 0.05f;
constexpr float k_TileTextOffsetYRatio      = 0.35f;

} // namespace

BaseWorkableAreaDisplay::BaseWorkableAreaDisplay(const BaseManager* pBase,
                                                 WindowLayout_t layout,
                                                 TileClickCallback_t onTileClicked,
                                                 BaseClickCallback_t onBaseClicked)
    : UIElement(layout)
    , m_pBase(pBase)
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

    m_tileSize = std::min(m_layout.width, m_layout.height) / static_cast<float>(k_GridDimension);
    const float gridWidth  = static_cast<float>(k_GridDimension) * m_tileSize;
    const float gridHeight = static_cast<float>(k_GridDimension) * m_tileSize;
    m_startX = m_layout.x + (m_layout.width  - gridWidth)  / k_GridCenterOffset;
    m_startY = m_layout.y + (m_layout.height - gridHeight) / k_GridCenterOffset;

    const int baseX = m_pBase->GetX();
    const int baseY = m_pBase->GetY();

    for (const Tile* pTile : m_pBase->GetWorkerAssignments().GetWorkableTiles())
    {
        if (!pTile)
        {
            continue;
        }

        const int relX = pTile->GetX() - baseX;
        const int relY = pTile->GetY() - baseY;
        const float screenX = m_startX + (static_cast<float>(relX) + k_GridCenterOffset) * m_tileSize;
        const float screenY = m_startY + (static_cast<float>(relY) + k_GridCenterOffset) * m_tileSize;

        m_tileRects.push_back(TileRect_t{
            Rectangle_t{screenX, screenY, m_tileSize, m_tileSize},
            pTile
        });
    }
}

void BaseWorkableAreaDisplay::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);

    for (const TileRect_t& entry : m_tileRects)
    {
        RenderTile_(rGraphics, *entry.pTile, entry.rect.x, entry.rect.y, m_tileSize,
                    m_pBase->GetWorkerAssignments().IsTileAssigned(entry.pTile));
    }

    const float centerX = m_startX + k_GridCenterOffset * m_tileSize;
    const float centerY = m_startY + k_GridCenterOffset * m_tileSize;
    rGraphics.DrawRect(centerX, centerY, m_tileSize, m_tileSize, k_TileBorderColor, k_TileBorderWidth);
    rGraphics.DrawText("BASE", centerX, centerY, k_BaseLabelFontSize, Color_t::Yellow());
}

void BaseWorkableAreaDisplay::RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size, bool bIsWorked)
{
    rGraphics.DrawRect(x, y, size, size, k_TileBorderColor, k_TileBorderWidth);

    const TileResources_t yield = bIsWorked
        ? m_pBase->GetWorkedTileYield(rTile)
        : m_pBase->GetTileEffects().ResolveTileYield(rTile);
    const int nutrients = yield.nutrients;
    const int minerals = yield.minerals;
    const int energy = yield.energy;

    std::ostringstream oss;
    oss << nutrients << " " << minerals << " " << energy;

    float textOffsetX = size * k_TileTextOffsetXRatio;
    float textOffsetY = size * k_TileTextOffsetYRatio;

    Color_t textColor = bIsWorked ? Color_t::Green() : Color_t::White();
    rGraphics.DrawText(oss.str(), x + textOffsetX, y + textOffsetY, k_TileFontSize, textColor);
}

void BaseWorkableAreaDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const float mouseX = static_cast<float>(rEvent.x);
    const float mouseY = static_cast<float>(rEvent.y);

    const float baseTileX = m_startX + k_GridCenterOffset * m_tileSize;
    const float baseTileY = m_startY + k_GridCenterOffset * m_tileSize;

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
