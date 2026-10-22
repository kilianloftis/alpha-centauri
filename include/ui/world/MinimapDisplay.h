#pragma once

#include "ui/UIElement.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

class GameState;
class Graphics;
class MapRenderer;
class MapViewport;

// The whole map in MapRenderer::TileColor, as the player sees it, two pixels per tile in brick
// layout. Left-click centres the camera.
class MinimapDisplay : public UIElement
{
public:
    using CenterOnTileCallback_t = std::function<void(int tileX, int tileY)>;

    MinimapDisplay(const GameState& rGameState, const MapRenderer& rMapRenderer,
                   WindowLayout_t layout, const MapViewport& rViewport,
                   CenterOnTileCallback_t onCenterOnTile);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    struct MapContentLayout_t
    {
        float originX;
        float originY;
        float pixelW;
        float pixelH;
        int mapWidth;
        int mapHeight;
    };

    struct TerrainCacheKey_t
    {
        uint64_t appearanceRevision = 0;
        uint64_t exploredRevision = 0;
        uint64_t visibleRevision = 0;
        uint64_t memoryRevision = 0;
        int mapWidth = 0;
        int mapHeight = 0;
        bool bHasViewer = false;

        bool operator==(const TerrainCacheKey_t&) const = default;
    };

    MapContentLayout_t ComputeMapContentLayout_() const;
    std::optional<std::pair<int, int>> HitTestTile_(float x, float y) const;
    void RenderViewportFrame_(Graphics& rGraphics, const MapContentLayout_t& rLayout) const;
    TerrainCacheKey_t CurrentTerrainKey_() const;
    void EnsureTerrainCache_(Graphics& rGraphics, const MapContentLayout_t& rLayout);

    const GameState& m_rGameState;
    const MapRenderer& m_rMapRenderer;
    const MapViewport& m_rViewport;
    CenterOnTileCallback_t m_onCenterOnTile;
    const std::string m_textureId;
    std::vector<std::uint8_t> m_terrainPixels;
    TerrainCacheKey_t m_terrainCacheKey{};
    bool m_bTerrainCacheValid = false;
};

} // namespace ac
