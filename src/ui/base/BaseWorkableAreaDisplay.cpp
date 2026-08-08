#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include <sstream>
#include <stdexcept>

namespace ac
{

BaseWorkableAreaDisplay::BaseWorkableAreaDisplay(const BaseManager& rBase,
                                                 const BaseDisplaySnapshot_t& rSnapshot,
                                                 WindowLayout_t layout,
                                                 TileClickCallback_t onTileClicked,
                                                 BaseClickCallback_t onBaseClicked)
    : UIElement(layout)
    , m_rBase(rBase)
    , m_rSnapshot(rSnapshot)
    , m_onTileClicked(std::move(onTileClicked))
    , m_onBaseClicked(std::move(onBaseClicked))
{
    CacheTileRects_();
}

void BaseWorkableAreaDisplay::CacheTileRects_()
{
    const auto& style = Style().baseWorkableAreaDisplay;

    m_tileSize = std::min(m_layout.width, m_layout.height) / static_cast<float>(style.gridDimension);
    const float gridWidth  = static_cast<float>(style.gridDimension) * m_tileSize;
    const float gridHeight = static_cast<float>(style.gridDimension) * m_tileSize;
    m_startX = m_layout.x + (m_layout.width  - gridWidth)  / style.gridCenterOffset;
    m_startY = m_layout.y + (m_layout.height - gridHeight) / style.gridCenterOffset;

    const Tile& rBaseTile = m_rBase.GetTile();
    const int mapWidth = m_rBase.GetTileEffects().GetWorldMap().GetWidth();

    for (const Tile* pTile : m_rBase.GetWorkerAssignments().GetWorkableTiles())
    {
        if (!pTile)
        {
            continue;
        }

        const int relX = DeltaX(rBaseTile.GetX(), pTile->GetX(), mapWidth);
        const int relY = pTile->GetY() - rBaseTile.GetY();
        const float screenX = m_startX + (static_cast<float>(relX) + style.gridCenterOffset) * m_tileSize;
        const float screenY = m_startY + (static_cast<float>(relY) + style.gridCenterOffset) * m_tileSize;

        m_tileRects.push_back(TileRect_t{
            Rectangle_t{screenX, screenY, m_tileSize, m_tileSize},
            pTile
        });
    }
}

void BaseWorkableAreaDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().baseWorkableAreaDisplay;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);

    for (const TileRect_t& entry : m_tileRects)
    {
        const auto it = m_rSnapshot.tiles.find(entry.pTile);
        if (it == m_rSnapshot.tiles.end())
        {
            // The snapshot walks the same workable-tile list this panel cached, so a miss means
            // the two disagree about the base's radius.
            throw std::runtime_error("BaseWorkableAreaDisplay: workable tile missing from snapshot");
        }
        RenderTile_(rGraphics, entry.rect.x, entry.rect.y, m_tileSize, it->second);
    }

    const float centerX = m_startX + style.gridCenterOffset * m_tileSize;
    const float centerY = m_startY + style.gridCenterOffset * m_tileSize;
    rGraphics.DrawRect(centerX, centerY, m_tileSize, m_tileSize, style.tileBorderColor, style.tileBorderWidth);
    rGraphics.DrawText("BASE", centerX, centerY, style.baseLabelFontSize, style.baseLabelColor);
}

void BaseWorkableAreaDisplay::RenderTile_(Graphics& rGraphics, float x, float y, float size,
                                          const TileDisplay_t& rTile)
{
    const auto& style = Style().baseWorkableAreaDisplay;

    rGraphics.DrawRect(x, y, size, size, style.tileBorderColor, style.tileBorderWidth);

    const bool bIsWorked = rTile.workState == TileWorkState_t::WorkedByThisBase;
    // Display the collectable (capped) totals; potential remains available on the view for
    // restriction callouts / tooltips.
    const int nutrients = rTile.yield.effective.nutrients;
    const int minerals = rTile.yield.effective.minerals;
    const int energy = rTile.yield.effective.energy;

    std::ostringstream oss;
    oss << nutrients << " " << minerals << " " << energy;

    float textOffsetX = size * style.tileTextOffsetXRatio;
    float textOffsetY = size * style.tileTextOffsetYRatio;

    // Three states, not two. A tile held by a neighbouring base, another faction, or a supply
    // crawler is workable-in-principle but not available to this base: showing it in the
    // unworked colour with a full preview yield makes it look free, and clicking it is then
    // silently refused. Dim it so the refusal is predictable.
    Color_t textColor = style.unworkedTileTextColor;
    if (bIsWorked)
    {
        textColor = style.workedTileTextColor;
    }
    else if (rTile.workState == TileWorkState_t::WorkedByOther)
    {
        textColor = style.unavailableTileTextColor;
    }
    rGraphics.DrawText(oss.str(), x + textOffsetX, y + textOffsetY, style.tileFontSize, textColor);
}

void BaseWorkableAreaDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const auto& style = Style().baseWorkableAreaDisplay;

    const float mouseX = static_cast<float>(rEvent.x);
    const float mouseY = static_cast<float>(rEvent.y);

    const float baseTileX = m_startX + style.gridCenterOffset * m_tileSize;
    const float baseTileY = m_startY + style.gridCenterOffset * m_tileSize;

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
