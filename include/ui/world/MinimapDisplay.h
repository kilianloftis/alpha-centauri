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
class MapViewport;

// Full-map terrain overview in the world dashboard right panel. Same elevation /
// fog / shroud colours as WorldDisplay, without per-tile labels or overlays.
// Left-click centers the world camera on the corresponding tile. Draws the current
// MapViewport as a border (split across the east/west seam when the camera wraps).
// Terrain+fog is cached as one RGBA texture (1 texel per tile) and only rebuilt when
// appearance/fog revisions or map size change; the viewport frame is always live.
class MinimapDisplay : public UIElement
{
public:
    using CenterOnTileCallback_t = std::function<void(int tileX, int tileY)>;

    MinimapDisplay(const GameState& rGameState, WindowLayout_t layout,
                   const MapViewport& rViewport,
                   CenterOnTileCallback_t onCenterOnTile);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    struct MapContentLayout_t
    {
        float originX;
        float originY;
        float tileSize;
        int mapWidth;
        int mapHeight;
    };

    struct TerrainCacheKey_t
    {
        uint64_t appearanceRevision = 0;
        uint64_t exploredRevision = 0;
        uint64_t visibleRevision = 0;
        int mapWidth = 0;
        int mapHeight = 0;
        bool bHasExplored = false;
        bool bHasVisible = false;

        bool operator==(const TerrainCacheKey_t&) const = default;
    };

    MapContentLayout_t ComputeMapContentLayout_() const;
    std::optional<std::pair<int, int>> HitTestTile_(float x, float y) const;
    void RenderViewportFrame_(Graphics& rGraphics, const MapContentLayout_t& rLayout) const;
    TerrainCacheKey_t CurrentTerrainKey_() const;
    void EnsureTerrainCache_(Graphics& rGraphics, const MapContentLayout_t& rLayout);

    const GameState& m_rGameState;
    const MapViewport& m_rViewport;
    CenterOnTileCallback_t m_onCenterOnTile;
    const std::string m_textureId;
    std::vector<std::uint8_t> m_terrainPixels;
    TerrainCacheKey_t m_terrainCacheKey{};
    bool m_bTerrainCacheValid = false;
};

} // namespace ac
