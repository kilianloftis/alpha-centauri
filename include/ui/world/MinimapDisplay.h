#pragma once

#include "ui/UIElement.h"

#include <functional>
#include <optional>
#include <utility>

namespace ac
{

class GameState;
class Graphics;
class MapViewport;

// Full-map terrain overview in the world dashboard right panel. Same elevation /
// fog / shroud colours as WorldDisplay, without per-tile labels or overlays.
// Left-click centers the world camera on the corresponding tile. Draws the current
// MapViewport as a border (split across the east/west seam when the camera wraps).
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

    MapContentLayout_t ComputeMapContentLayout_() const;
    std::optional<std::pair<int, int>> HitTestTile_(float x, float y) const;
    void RenderViewportFrame_(Graphics& rGraphics, const MapContentLayout_t& rLayout) const;

    const GameState& m_rGameState;
    const MapViewport& m_rViewport;
    CenterOnTileCallback_t m_onCenterOnTile;
};

} // namespace ac
