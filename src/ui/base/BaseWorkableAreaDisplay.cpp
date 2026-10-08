#include "ui/base/BaseWorkableAreaDisplay.h"
#include "game/buildings/BaseSpriteSizesConfig.h"
#include "game/buildings/MapOverlayChannelsConfig.h"
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

void DrawCenteredTileText_(Graphics& rGraphics, const TileShape_t& rShape, const std::string& rText,
                           unsigned int fontSize, const Color_t& rColor, float charWidthRatio)
{
    const float centerX = (rShape.west.x + rShape.east.x) * 0.5f;
    const float centerY = (rShape.north.y + rShape.south.y) * 0.5f;
    const float size = static_cast<float>(fontSize);
    const float textWidth = static_cast<float>(rText.size()) * size * charWidthRatio;
    rGraphics.DrawText(rText, centerX - textWidth * 0.5f, centerY - size * 0.5f, fontSize, rColor);
}

} // namespace

BaseWorkableAreaDisplay::BaseWorkableAreaDisplay(const BaseManager& rBase,
                                                 const BaseDisplaySnapshot_t& rSnapshot,
                                                 WindowLayout_t layout,
                                                 const BaseSpriteSizesConfig_t& rBaseSpriteSizes,
                                                 const MapOverlayChannelsConfig_t& rMapOverlayChannels,
                                                 TileClickCallback_t onTileClicked,
                                                 BaseClickCallback_t onBaseClicked)
    : UIElement(layout)
    , m_rBase(rBase)
    , m_rSnapshot(rSnapshot)
    , m_onTileClicked(std::move(onTileClicked))
    , m_onBaseClicked(std::move(onBaseClicked))
    , m_mapSurface(rBaseSpriteSizes, rMapOverlayChannels)
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

void BaseWorkableAreaDisplay::RenderYieldLabel_(Graphics& rGraphics,
                                                const TileDiamond_t& rEntry) const
{
    const auto& style = Style().baseWorkableAreaDisplay;
    if (rEntry.bIsBase)
    {
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
    DrawCenteredTileText_(rGraphics, rEntry.shape, oss.str(), style.tileFontSize, textColor,
                          style.tileTextCharWidthRatio);
}

void BaseWorkableAreaDisplay::Render(Graphics& rGraphics)
{
    const auto& style = Style().baseWorkableAreaDisplay;
    const WorldMap& rWorldMap = m_rBase.GetTileEffects().GetWorldMap();
    const MapSurfaceRenderer::YieldLookup_t yieldOf = [this](const Tile& rTile) {
        const auto it = m_rSnapshot.tiles.find(&rTile);
        if (it == m_rSnapshot.tiles.end())
        {
            return TileResources_t{};
        }
        return it->second.yield.effective;
    };

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             style.backgroundColor);

    // Same layering as WorldDisplay: all tile surfaces first, bases after so overhang is not
    // covered by front-tile terrain, then BaseView-only yield / placeholder labels.
    const TileDiamond_t* pBaseEntry = nullptr;
    for (const TileDiamond_t& rEntry : m_tileDiamonds)
    {
        m_mapSurface.RenderTile(rGraphics, *rEntry.pTile, rEntry.shape, rWorldMap,
                                /*bFogged=*/false, /*bShrouded=*/false, yieldOf,
                                MapGridStyle_t::FullDiamond);
        rGraphics.DrawDiamond(ShapeAabbLeft_(rEntry.shape), ShapeAabbTop_(rEntry.shape),
                              ShapeAabbWidth_(rEntry.shape), ShapeAabbHeight_(rEntry.shape),
                              style.tileBorderColor, style.tileBorderWidth);
        if (rEntry.bIsBase)
        {
            pBaseEntry = &rEntry;
        }
    }

    if (pBaseEntry)
    {
        if (!m_mapSurface.RenderBase(rGraphics, m_rBase, pBaseEntry->shape))
        {
            DrawCenteredTileText_(rGraphics, pBaseEntry->shape, "BASE", style.baseLabelFontSize,
                                  style.baseLabelColor, style.tileTextCharWidthRatio);
        }
    }

    for (const TileDiamond_t& rEntry : m_tileDiamonds)
    {
        RenderYieldLabel_(rGraphics, rEntry);
    }
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
