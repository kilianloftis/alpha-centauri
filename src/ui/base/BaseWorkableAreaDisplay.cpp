#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/MapRenderer.h"
#include "ui/style/UiStyle.h"
#include "ui/world/MapAppearance.h"
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

void DrawCenteredTileText_(Graphics& rGraphics, const TileShape_t& rShape, const std::string& rText,
                           unsigned int fontSize, const Color_t& rColor)
{
    const float centerX = (rShape.west.x + rShape.east.x) * 0.5f;
    const float centerY = (rShape.north.y + rShape.south.y) * 0.5f;
    const float size = static_cast<float>(fontSize);
    const float textWidth = rGraphics.MeasureTextWidth(rText, fontSize);
    rGraphics.DrawText(rText, centerX - textWidth * 0.5f, centerY - size * 0.5f, fontSize, rColor);
}

} // namespace

BaseWorkableAreaDisplay::BaseWorkableAreaDisplay(const BaseManager& rBase,
                                                 const BaseDisplaySnapshot_t& rSnapshot,
                                                 WindowLayout_t layout,
                                                 MapRenderer& rMapRenderer,
                                                 TileClickCallback_t onTileClicked,
                                                 BaseClickCallback_t onBaseClicked)
    : UIElement(layout)
    , m_rBase(rBase)
    , m_rSnapshot(rSnapshot)
    , m_rMapRenderer(rMapRenderer)
    , m_onTileClicked(std::move(onTileClicked))
    , m_onBaseClicked(std::move(onBaseClicked))
{
    PlaceTiles_();
}

void BaseWorkableAreaDisplay::PlaceTiles_()
{
    const float tileWidth =
        std::min(m_layout.width / k_ClusterWidthInTiles, m_layout.height / k_ClusterHeightInTiles);
    const float halfW = tileWidth * 0.5f;
    const float halfH = tileWidth * k_IsoHeightRatio * 0.5f;
    const float clusterW = k_ClusterWidthInTiles * tileWidth;
    // 2 tile-heights = 1 tile-width when height = width / 2.
    const float clusterH = k_ClusterHeightInTiles * tileWidth;
    // Cluster is 8 half-steps; base footprint origin is inset by 3 (max |map delta|).
    const float originX = m_layout.x + (m_layout.width - clusterW) * 0.5f + 3.0f * halfW;
    const float originY = m_layout.y + (m_layout.height - clusterH) * 0.5f + 3.0f * halfH;

    const Tile& rBaseTile = m_rBase.GetTile();
    const int mapWidth = m_rBase.GetTileEffects().GetWorldMap().GetWidth();

    // Map delta from the base; higher map rows draw and hit in front.
    struct Placement_t
    {
        int mapDy = 0;
        int mapDx = 0;
        PlacedTile_t placed;
    };
    std::vector<Placement_t> placements{
        Placement_t{0, 0, PlacedTile_t{&rBaseTile, FlatTileShape(originX, originY, tileWidth)}}};
    for (const Tile* pTile : m_rBase.GetWorkerAssignments().GetWorkableTiles())
    {
        if (!pTile)
        {
            continue;
        }
        const LatticeDelta_t d = LatticeDelta(rBaseTile, *pTile, mapWidth);
        const int mapDx = d.p - d.q;
        const int mapDy = d.p + d.q;
        placements.push_back(Placement_t{
            mapDy, mapDx,
            PlacedTile_t{pTile, FlatTileShape(originX + static_cast<float>(mapDx) * halfW,
                                              originY + static_cast<float>(mapDy) * halfH,
                                              tileWidth)}});
    }

    std::ranges::sort(placements, [](const Placement_t& a, const Placement_t& b) {
        if (a.mapDy != b.mapDy)
        {
            return a.mapDy < b.mapDy;
        }
        return a.mapDx < b.mapDx;
    });
    m_tiles.clear();
    for (const Placement_t& rPlacement : placements)
    {
        m_tiles.push_back(rPlacement.placed);
    }
}

void BaseWorkableAreaDisplay::RenderYieldLabel_(Graphics& rGraphics,
                                                const PlacedTile_t& rPlaced) const
{
    const auto& style = Style().baseWorkableAreaDisplay;
    const auto it = m_rSnapshot.tiles.find(rPlaced.pTile);
    if (it == m_rSnapshot.tiles.end())
    {
        // The snapshot walks the same workable-tile list this panel placed, so a miss means
        // the two disagree about the base's radius.
        throw std::runtime_error("BaseWorkableAreaDisplay: workable tile missing from snapshot");
    }

    const TileDisplay_t& rTile = it->second;
    const bool bIsWorked = rTile.workState == TileWorkState_t::WorkedByThisBase;
    std::ostringstream oss;
    oss << rTile.yield.effective.nutrients << " " << rTile.yield.effective.minerals << " "
        << rTile.yield.effective.energy;

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
    DrawCenteredTileText_(rGraphics, rPlaced.shape, oss.str(), style.tileFontSize, textColor);
}

void BaseWorkableAreaDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().baseWorkableAreaDisplay;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             style.backgroundColor);

    MapContent_t content;
    content.showsBase = [](const BaseManager&) { return true; };
    m_rMapRenderer.Render(
        rGraphics, m_tiles,
        MapAppearance::Clear(m_rBase.GetTileEffects().GetWorldMap(), &m_rBase.GetFaction()),
        content);

    for (const PlacedTile_t& rPlaced : m_tiles)
    {
        if (rPlaced.pTile != &m_rBase.GetTile())
        {
            RenderYieldLabel_(rGraphics, rPlaced);
        }
    }
}

void BaseWorkableAreaDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const float mouseX = static_cast<float>(rEvent.x);
    const float mouseY = static_cast<float>(rEvent.y);

    // Front first, matching the world map's raised-tile pick order.
    for (auto it = m_tiles.rbegin(); it != m_tiles.rend(); ++it)
    {
        if (!ShapeContains(it->shape, mouseX, mouseY))
        {
            continue;
        }
        if (it->pTile == &m_rBase.GetTile())
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
