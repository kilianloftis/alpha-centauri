#include "ui/world/MinimapDisplay.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/style/UiStyle.h"
#include "ui/world/MapViewport.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{

namespace
{

struct PlayerFogMaps_t
{
    const FactionExploredMap* pExplored = nullptr;
    const FactionVisibleMap* pVisible = nullptr;
};

PlayerFogMaps_t PlayerFog_(const GameState& rGameState)
{
    const Faction* pPlayer = rGameState.GetPlayerFaction();
    if (!pPlayer || !pPlayer->GetExploredMap().IsSized() || !pPlayer->GetVisibleMap().IsSized())
    {
        return {};
    }
    return {&pPlayer->GetExploredMap(), &pPlayer->GetVisibleMap()};
}

} // namespace

MinimapDisplay::MinimapDisplay(const GameState& rGameState, WindowLayout_t layout,
                               const MapViewport& rViewport,
                               CenterOnTileCallback_t onCenterOnTile)
    : UIElement(layout)
    , m_rGameState(rGameState)
    , m_rViewport(rViewport)
    , m_onCenterOnTile(std::move(onCenterOnTile))
{
    if (!m_onCenterOnTile)
    {
        throw std::runtime_error("MinimapDisplay: onCenterOnTile callback is required");
    }
}

MinimapDisplay::MapContentLayout_t MinimapDisplay::ComputeMapContentLayout_() const
{
    const WorldMap& rWorldMap = m_rGameState.GetWorldMap();
    const int mapWidth = rWorldMap.GetWidth();
    const int mapHeight = rWorldMap.GetHeight();
    if (mapWidth <= 0 || mapHeight <= 0)
    {
        throw std::runtime_error("MinimapDisplay: world map has zero size");
    }

    // Fit the whole map into the panel while preserving aspect ratio (letterbox).
    const float tileSize = std::min(m_layout.width / static_cast<float>(mapWidth),
                                    m_layout.height / static_cast<float>(mapHeight));
    const float mapPixelW = tileSize * static_cast<float>(mapWidth);
    const float mapPixelH = tileSize * static_cast<float>(mapHeight);
    return MapContentLayout_t{
        m_layout.x + (m_layout.width - mapPixelW) * 0.5f,
        m_layout.y + (m_layout.height - mapPixelH) * 0.5f,
        tileSize,
        mapWidth,
        mapHeight,
    };
}

std::optional<std::pair<int, int>> MinimapDisplay::HitTestTile_(float x, float y) const
{
    const MapContentLayout_t layout = ComputeMapContentLayout_();

    const float localX = x - layout.originX;
    const float localY = y - layout.originY;
    if (localX < 0.0f || localY < 0.0f)
    {
        return std::nullopt;
    }

    const int tileX = static_cast<int>(localX / layout.tileSize);
    const int tileY = static_cast<int>(localY / layout.tileSize);
    if (tileX < 0 || tileX >= layout.mapWidth || tileY < 0 || tileY >= layout.mapHeight)
    {
        return std::nullopt;
    }
    return std::make_pair(tileX, tileY);
}

void MinimapDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    if (const auto tile = HitTestTile_(static_cast<float>(rEvent.x), static_cast<float>(rEvent.y)))
    {
        m_onCenterOnTile(tile->first, tile->second);
    }
}

void MinimapDisplay::RenderViewportFrame_(Graphics& rGraphics,
                                          const MapContentLayout_t& rLayout) const
{
    const int rowStart = m_rViewport.RowStart();
    const int rowEnd = m_rViewport.RowEnd();
    const int viewRows = rowEnd - rowStart;
    const int viewCols = m_rViewport.VisibleCols();
    if (viewRows <= 0 || viewCols <= 0)
    {
        return;
    }

    const auto& style = Style().minimapDisplay;
    const int camX = m_rViewport.CameraX();

    auto drawBox = [&](int tileX, int cols) {
        if (cols <= 0)
        {
            return;
        }
        const float x = rLayout.originX + static_cast<float>(tileX) * rLayout.tileSize;
        const float y = rLayout.originY + static_cast<float>(rowStart) * rLayout.tileSize;
        const float w = static_cast<float>(cols) * rLayout.tileSize;
        const float h = static_cast<float>(viewRows) * rLayout.tileSize;
        rGraphics.DrawRect(x, y, w, h, style.viewportBorderColor, style.viewportBorderWidth);
    };

    // Camera X wraps: near the east edge the FOV straddles the seam as two rectangles.
    if (camX + viewCols <= rLayout.mapWidth)
    {
        drawBox(camX, viewCols);
    }
    else
    {
        const int eastCols = rLayout.mapWidth - camX;
        const int westCols = viewCols - eastCols;
        drawBox(camX, eastCols);
        drawBox(0, westCols);
    }
}

void MinimapDisplay::Render(Graphics& rGraphics)
{
    const MapContentLayout_t layout = ComputeMapContentLayout_();

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             Style().worldDisplay.shroudColor);

    const WorldMap& rWorldMap = m_rGameState.GetWorldMap();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    for (int row = 0; row < layout.mapHeight; ++row)
    {
        for (int col = 0; col < layout.mapWidth; ++col)
        {
            const Tile* pTile = rWorldMap.GetTile(col, row);
            if (!pTile)
            {
                throw std::runtime_error("MinimapDisplay: missing tile in world map");
            }

            const float tileX = layout.originX + static_cast<float>(col) * layout.tileSize;
            const float tileY = layout.originY + static_cast<float>(row) * layout.tileSize;

            if (fog.pExplored && !fog.pExplored->IsExplored(*pTile))
            {
                // Already filled with shroud via the panel background.
                continue;
            }

            const bool bFogged = fog.pVisible && !fog.pVisible->IsVisible(*pTile);
            rGraphics.DrawFilledRect(tileX, tileY, layout.tileSize, layout.tileSize,
                                     TileRenderer::FillColor(*pTile, bFogged));
        }
    }

    RenderViewportFrame_(rGraphics, layout);
}

} // namespace ac
