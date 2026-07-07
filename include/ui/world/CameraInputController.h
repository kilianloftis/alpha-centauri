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

private:
    struct CameraBounds_t
    {
        int maxCamX;
        int maxCamY;
    };

    CameraBounds_t ComputeMaxCamera_() const;
    void ApplyEdgeScroll_(int mouseX, int mouseY);

    WorldDisplay& m_rWorldDisplay;
    const WorldMap& m_rWorldMap;
    const WindowLayout_t m_mapLayout;
    float m_edgeScrollAccumulatorX = 0.0f;
    float m_edgeScrollAccumulatorY = 0.0f;
    float m_edgeScrollSpeed;
};

} // namespace ac
