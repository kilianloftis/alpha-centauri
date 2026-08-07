#pragma once

#include "input/Input.h"
#include "ui/UIElement.h"

#include <optional>

namespace ac
{

class WorldDisplay;
class WorldMap;

class CameraInputController
{
public:
    CameraInputController(WorldDisplay& rWorldDisplay, const WorldMap& rWorldMap, const WindowLayout_t& mapLayout);

    bool HandleKey(const KeyEvent_t& rEvent);

    // mousePosition is empty when the backend has not seen the pointer yet; edge-scrolling is
    // then idle rather than guessing a position.
    void Update(bool bEnabled, std::optional<MousePosition_t> mousePosition);
    void CenterOnTile(int tileX, int tileY);

private:
    // Vertical only — X wraps continuously around the map width.
    int ComputeMaxCameraY_() const;
    void ApplyEdgeScroll_(int mouseX, int mouseY);

    WorldDisplay& m_rWorldDisplay;
    const WorldMap& m_rWorldMap;
    const WindowLayout_t m_mapLayout;
    float m_edgeScrollAccumulatorX = 0.0f;
    float m_edgeScrollAccumulatorY = 0.0f;
    float m_edgeScrollSpeed;
};

} // namespace ac
