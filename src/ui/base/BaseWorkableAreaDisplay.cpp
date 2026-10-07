#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/style/UiStyle.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace ac
{

namespace
{

// Workable disk (lattice radius 2) reaches |map dx|, |map dy| ≤ 3; plus the tile's 2 map-unit
// footprint → 8 half-steps = 4 tile-widths by 2 tile-heights.
constexpr float k_ClusterWidthInTiles = 4.0f;
constexpr float k_ClusterHeightInTiles = 2.0f;

float ShapeAabbLeft_(const TileShape_t& rShape)
{
    return rShape.west.x;
}

float ShapeAabbTop_(const TileShape_t& rShape)
{
    return rShape.north.y;
}

float ShapeAabbWidth_(const TileShape_t& rShape)
{
    return rShape.east.x - rShape.west.x;
}

float ShapeAabbHeight_(const TileShape_t& rShape)
{
    return rShape.south.y - rShape.north.y;
}

} // namespace

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
    CacheTileDiamonds_();
}

void BaseWorkableAreaDisplay::CacheTileDiamonds_()
{
    m_tileWidth =
        std::min(m_layout.width / k_ClusterWidthInTiles, m_layout.height / k_ClusterHeightInTiles);
    const float tileHeight = m_tileWidth * 0.5f;
    const float halfW = m_tileWidth * 0.5f;
    const float halfH = tileHeight * 0.5f;
    const float clusterW = k_ClusterWidthInTiles * m_tileWidth;
    // 2 tile-heights = 1 tile-width when height = width / 2.
    const float clusterH = k_ClusterHeightInTiles * m_tileWidth;
    // Cluster is 8 half-steps; base footprint origin is inset by 3 (max |map delta|).
    const float originX = m_layout.x + (m_layout.width - clusterW) * 0.5f + 3.0f * halfW;
    const float originY = m_layout.y + (m_layout.height - clusterH) * 0.5f + 3.0f * halfH;

    const Tile& rBaseTile = m_rBase.GetTile();
    const int mapWidth = m_rBase.GetTileEffects().GetWorldMap().GetWidth();

    m_tileDiamonds.clear();
    m_tileDiamonds.push_back(TileDiamond_t{
        TileRenderer::FlatTileShape(originX, originY, m_tileWidth),
        &rBaseTile,
        0,
        0,
        true,
    });

    for (const Tile* pTile : m_rBase.GetWorkerAssignments().GetWorkableTiles())
    {
        if (!pTile)
        {
            continue;
        }

        const LatticeDelta_t d = LatticeDelta(rBaseTile, *pTile, mapWidth);
        const int mapDx = d.p - d.q;
        const int mapDy = d.p + d.q;
        const float aabbX = originX + static_cast<float>(mapDx) * halfW;
        const float aabbY = originY + static_cast<float>(mapDy) * halfH;
        m_tileDiamonds.push_back(TileDiamond_t{
            TileRenderer::FlatTileShape(aabbX, aabbY, m_tileWidth),
            pTile,
            mapDx,
            mapDy,
            false,
        });
    }

    std::sort(m_tileDiamonds.begin(), m_tileDiamonds.end(),
              [](const TileDiamond_t& a, const TileDiamond_t& b) {
                  if (a.mapDy != b.mapDy)
                  {
                      return a.mapDy < b.mapDy;
                  }
                  return a.mapDx < b.mapDx;
              });
}

void BaseWorkableAreaDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().baseWorkableAreaDisplay;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             style.backgroundColor);

    for (const TileDiamond_t& rEntry : m_tileDiamonds)
    {
        RenderTile_(rGraphics, rEntry);
    }
}

void BaseWorkableAreaDisplay::RenderTile_(Graphics& rGraphics, const TileDiamond_t& rEntry) const
{
    const auto& style = Style().baseWorkableAreaDisplay;
    const float x = ShapeAabbLeft_(rEntry.shape);
    const float y = ShapeAabbTop_(rEntry.shape);
    const float w = ShapeAabbWidth_(rEntry.shape);
    const float h = ShapeAabbHeight_(rEntry.shape);

    rGraphics.DrawDiamond(x, y, w, h, style.tileBorderColor, style.tileBorderWidth);

    if (rEntry.bIsBase)
    {
        rGraphics.DrawText("BASE", x + w * style.tileTextOffsetXRatio,
                           y + h * style.tileTextOffsetYRatio, style.baseLabelFontSize,
                           style.baseLabelColor);
        return;
    }

    const auto it = m_rSnapshot.tiles.find(rEntry.pTile);
    if (it == m_rSnapshot.tiles.end())
    {
        // The snapshot walks the same workable-tile list this panel cached, so a miss means
        // the two disagree about the base's radius.
        throw std::runtime_error("BaseWorkableAreaDisplay: workable tile missing from snapshot");
    }

    const TileDisplay_t& rTile = it->second;
    const bool bIsWorked = rTile.workState == TileWorkState_t::WorkedByThisBase;
    const int nutrients = rTile.yield.effective.nutrients;
    const int minerals = rTile.yield.effective.minerals;
    const int energy = rTile.yield.effective.energy;

    std::ostringstream oss;
    oss << nutrients << " " << minerals << " " << energy;

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
    rGraphics.DrawText(oss.str(), x + w * style.tileTextOffsetXRatio,
                       y + h * style.tileTextOffsetYRatio, style.tileFontSize, textColor);
}

void BaseWorkableAreaDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const float mouseX = static_cast<float>(rEvent.x);
    const float mouseY = static_cast<float>(rEvent.y);

    // Front first (higher mapDy), matching the world map's raised-tile pick order.
    for (auto it = m_tileDiamonds.rbegin(); it != m_tileDiamonds.rend(); ++it)
    {
        if (!TileRenderer::ShapeContains(it->shape, mouseX, mouseY))
        {
            continue;
        }
        if (it->bIsBase)
        {
            if (m_onBaseClicked)
            {
                m_onBaseClicked();
            }
            return;
        }
        if (m_onTileClicked)
        {
            m_onTileClicked(it->pTile);
        }
        return;
    }
}

} // namespace ac
