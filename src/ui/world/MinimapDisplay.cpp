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
#include <cstdint>
#include <stdexcept>
#include <string>

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

void WritePixel_(std::vector<std::uint8_t>& rPixels, size_t index, const Color_t& rColor)
{
    const size_t offset = index * 4;
    rPixels[offset] = rColor.r;
    rPixels[offset + 1] = rColor.g;
    rPixels[offset + 2] = rColor.b;
    rPixels[offset + 3] = rColor.a;
}

} // namespace

MinimapDisplay::MinimapDisplay(const GameState& rGameState, WindowLayout_t layout,
                               const MapViewport& rViewport,
                               CenterOnTileCallback_t onCenterOnTile)
    : UIElement(layout)
    , m_rGameState(rGameState)
    , m_rViewport(rViewport)
    , m_onCenterOnTile(std::move(onCenterOnTile))
    , m_textureId("minimap:" + std::to_string(reinterpret_cast<std::uintptr_t>(this)))
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

MinimapDisplay::TerrainCacheKey_t MinimapDisplay::CurrentTerrainKey_() const
{
    const WorldMap& rWorldMap = m_rGameState.GetWorldMap();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);
    TerrainCacheKey_t key;
    key.appearanceRevision = rWorldMap.GetAppearanceRevision();
    key.mapWidth = rWorldMap.GetWidth();
    key.mapHeight = rWorldMap.GetHeight();
    key.bHasExplored = fog.pExplored != nullptr;
    key.bHasVisible = fog.pVisible != nullptr;
    if (fog.pExplored)
    {
        key.exploredRevision = fog.pExplored->GetRevision();
    }
    if (fog.pVisible)
    {
        key.visibleRevision = fog.pVisible->GetRevision();
    }
    return key;
}

void MinimapDisplay::EnsureTerrainCache_(Graphics& rGraphics, const MapContentLayout_t& rLayout)
{
    const TerrainCacheKey_t key = CurrentTerrainKey_();
    if (m_bTerrainCacheValid && key == m_terrainCacheKey)
    {
        return;
    }

    const WorldMap& rWorldMap = m_rGameState.GetWorldMap();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);
    const Color_t shroud = Style().worldDisplay.shroudColor;
    const size_t pixelCount =
        static_cast<size_t>(rLayout.mapWidth) * static_cast<size_t>(rLayout.mapHeight);
    m_terrainPixels.assign(pixelCount * 4, 0);

    for (int row = 0; row < rLayout.mapHeight; ++row)
    {
        for (int col = 0; col < rLayout.mapWidth; ++col)
        {
            const Tile* pTile = rWorldMap.GetTile(col, row);
            if (!pTile)
            {
                throw std::runtime_error("MinimapDisplay: missing tile in world map");
            }

            const size_t index =
                static_cast<size_t>(row) * static_cast<size_t>(rLayout.mapWidth)
                + static_cast<size_t>(col);

            if (fog.pExplored && !fog.pExplored->IsExplored(*pTile))
            {
                WritePixel_(m_terrainPixels, index, shroud);
                continue;
            }

            const bool bFogged = fog.pVisible && !fog.pVisible->IsVisible(*pTile);
            WritePixel_(m_terrainPixels, index, TileRenderer::FillColor(*pTile, bFogged));
        }
    }

    if (!rGraphics.UpsertTextureRGBA(m_textureId, static_cast<unsigned int>(rLayout.mapWidth),
                                     static_cast<unsigned int>(rLayout.mapHeight),
                                     m_terrainPixels.data()))
    {
        throw std::runtime_error("MinimapDisplay: failed to upload terrain cache texture");
    }

    m_terrainCacheKey = key;
    m_bTerrainCacheValid = true;
}

void MinimapDisplay::Render(Graphics& rGraphics)
{
    const MapContentLayout_t layout = ComputeMapContentLayout_();

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             Style().worldDisplay.shroudColor);

    EnsureTerrainCache_(rGraphics, layout);

    const float mapPixelW = layout.tileSize * static_cast<float>(layout.mapWidth);
    const float mapPixelH = layout.tileSize * static_cast<float>(layout.mapHeight);
    if (!rGraphics.DrawSprite(m_textureId, layout.originX, layout.originY, mapPixelW, mapPixelH))
    {
        throw std::runtime_error("MinimapDisplay: failed to draw terrain cache texture");
    }

    RenderViewportFrame_(rGraphics, layout);
}

} // namespace ac
