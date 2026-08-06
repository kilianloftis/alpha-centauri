#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include <sstream>

namespace ac
{

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

    const auto& style = Style().baseWorkableAreaDisplay;

    m_tileSize = std::min(m_layout.width, m_layout.height) / static_cast<float>(style.gridDimension);
    const float gridWidth  = static_cast<float>(style.gridDimension) * m_tileSize;
    const float gridHeight = static_cast<float>(style.gridDimension) * m_tileSize;
    m_startX = m_layout.x + (m_layout.width  - gridWidth)  / style.gridCenterOffset;
    m_startY = m_layout.y + (m_layout.height - gridHeight) / style.gridCenterOffset;

    const Tile& rBaseTile = m_pBase->GetTile();
    const int mapWidth = m_pBase->GetTileEffects().GetWorldMap().GetWidth();

    for (const Tile* pTile : m_pBase->GetWorkerAssignments().GetWorkableTiles())
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
        // "Worked by this base", not "worked by anyone": RenderTile_ picks GetWorkedTileYield
        // for a worked tile, and that only resolves against this base's pops. Asking the
        // world-wide predicate painted a neighbour's (or an enemy's) tile as worked and then
        // displayed 0 0 0 for it.
        RenderTile_(rGraphics, *entry.pTile, entry.rect.x, entry.rect.y, m_tileSize,
                    m_pBase->GetWorkerAssignments().IsTileWorkedByThisBase(entry.pTile));
    }

    const float centerX = m_startX + style.gridCenterOffset * m_tileSize;
    const float centerY = m_startY + style.gridCenterOffset * m_tileSize;
    rGraphics.DrawRect(centerX, centerY, m_tileSize, m_tileSize, style.tileBorderColor, style.tileBorderWidth);
    rGraphics.DrawText("BASE", centerX, centerY, style.baseLabelFontSize, style.baseLabelColor);
}

void BaseWorkableAreaDisplay::RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size, bool bIsWorked)
{
    const auto& style = Style().baseWorkableAreaDisplay;

    rGraphics.DrawRect(x, y, size, size, style.tileBorderColor, style.tileBorderWidth);

    const TileYieldView_t yield = bIsWorked
        ? m_pBase->GetWorkedTileYield(rTile)
        : m_pBase->GetPreviewTileYield(rTile);
    // Display the collectable (capped) totals; potential remains available on the view for
    // restriction callouts / tooltips.
    const int nutrients = yield.effective.nutrients;
    const int minerals = yield.effective.minerals;
    const int energy = yield.effective.energy;

    std::ostringstream oss;
    oss << nutrients << " " << minerals << " " << energy;

    float textOffsetX = size * style.tileTextOffsetXRatio;
    float textOffsetY = size * style.tileTextOffsetYRatio;

    Color_t textColor = bIsWorked ? style.workedTileTextColor : style.unworkedTileTextColor;
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
