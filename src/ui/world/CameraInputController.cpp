#include "ui/world/CameraInputController.h"

#include "game/map/WorldMap.h"
#include "ui/world/WorldDisplay.h"

#include <algorithm>

namespace ac
{

CameraInputController::CameraInputController(WorldDisplay& rWorldDisplay, const WorldMap& rWorldMap, const WindowLayout_t& mapLayout)
    : m_rWorldDisplay(rWorldDisplay)
    , m_rWorldMap(rWorldMap)
    , m_mapLayout(mapLayout)
{
}

CameraInputController::CameraBounds_t CameraInputController::ComputeMaxCamera_() const
{
    return {
        std::max(0, m_rWorldMap.GetWidth()  - m_rWorldDisplay.GetVisibleCols()),
        std::max(0, m_rWorldMap.GetHeight() - m_rWorldDisplay.GetVisibleRows())
    };
}

bool CameraInputController::HandleKey(const KeyEvent_t& rEvent)
{
    const auto [maxCamX, maxCamY] = ComputeMaxCamera_();

    int camX = m_rWorldDisplay.GetCameraX();
    int camY = m_rWorldDisplay.GetCameraY();

    if (rEvent.key == Key_t::ArrowLeft)
    {
        m_rWorldDisplay.SetCameraOffset(std::max(0, camX - 1), camY);
        return true;
    }
    else if (rEvent.key == Key_t::ArrowRight)
    {
        m_rWorldDisplay.SetCameraOffset(std::min(maxCamX, camX + 1), camY);
        return true;
    }
    else if (rEvent.key == Key_t::ArrowUp)
    {
        m_rWorldDisplay.SetCameraOffset(camX, std::max(0, camY - 1));
        return true;
    }
    else if (rEvent.key == Key_t::ArrowDown)
    {
        m_rWorldDisplay.SetCameraOffset(camX, std::min(maxCamY, camY + 1));
        return true;
    }

    return false;
}

bool CameraInputController::HandleMouse(const MouseEvent_t& rEvent)
{
    m_lastMouseX = rEvent.x;
    m_lastMouseY = rEvent.y;

    const auto [maxCamX, maxCamY] = ComputeMaxCamera_();

    const float relX = static_cast<float>(rEvent.x - m_mapLayout.x) / m_mapLayout.width;
    const float relY = static_cast<float>(rEvent.y - m_mapLayout.y) / m_mapLayout.height;

    const bool bInMap = relX >= 0.0f && relX <= 1.0f && relY >= 0.0f && relY <= 1.0f;
    if (!bInMap)
    {
        return false;
    }

    constexpr float k_EdgeZone = 0.05f;

    float scrollDirX = 0.0f;
    float scrollDirY = 0.0f;

    if (relX < k_EdgeZone)
        scrollDirX = -1.0f;
    else if (relX > 1.0f - k_EdgeZone)
        scrollDirX = 1.0f;

    if (relY < k_EdgeZone)
        scrollDirY = -1.0f;
    else if (relY > 1.0f - k_EdgeZone)
        scrollDirY = 1.0f;

    m_edgeScrollAccumulatorX += scrollDirX * m_edgeScrollSpeed;
    m_edgeScrollAccumulatorY += scrollDirY * m_edgeScrollSpeed;

    const int deltaX = static_cast<int>(m_edgeScrollAccumulatorX);
    const int deltaY = static_cast<int>(m_edgeScrollAccumulatorY);

    if (deltaX != 0 || deltaY != 0)
    {
        m_edgeScrollAccumulatorX -= static_cast<float>(deltaX);
        m_edgeScrollAccumulatorY -= static_cast<float>(deltaY);

        const int newCamX = std::clamp(m_rWorldDisplay.GetCameraX() + deltaX, 0, maxCamX);
        const int newCamY = std::clamp(m_rWorldDisplay.GetCameraY() + deltaY, 0, maxCamY);
        m_rWorldDisplay.SetCameraOffset(newCamX, newCamY);
        return true;
    }

    return false;
}

} // namespace ac
