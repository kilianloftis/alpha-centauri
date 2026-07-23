#pragma once

#include "ui/UIElement.h"

namespace ac
{

class WorldDisplay;
class WorldMap;

class CameraInputController
{
public:
    CameraInputController(WorldDisplay& rWorldDisplay, const WorldMap& rWorldMap, const WindowLayout_t& mapLayout);

    bool HandleKey(const KeyEvent_t& rEvent);
    void Update(bool bEnabled);
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
