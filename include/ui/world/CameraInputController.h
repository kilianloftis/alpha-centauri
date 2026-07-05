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
    bool HandleMouse(const MouseEvent_t& rEvent);

private:
    struct CameraBounds_t
    {
        int maxCamX;
        int maxCamY;
    };

    CameraBounds_t ComputeMaxCamera_() const;

    WorldDisplay& m_rWorldDisplay;
    const WorldMap& m_rWorldMap;
    const WindowLayout_t m_mapLayout;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
    float m_edgeScrollAccumulatorX = 0.0f;
    float m_edgeScrollAccumulatorY = 0.0f;
    float m_edgeScrollSpeed;
};

} // namespace ac
