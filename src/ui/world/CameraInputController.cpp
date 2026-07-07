#include "ui/world/CameraInputController.h"

#include "game/map/WorldMap.h"
#include "input/MouseEventQueue.h"
#include "ui/world/WorldDisplay.h"

#include <algorithm>

namespace ac
{

namespace
{

constexpr float k_EdgeZone              = 0.05f;
constexpr float k_RelativeMin           = 0.0f;
constexpr float k_RelativeMax           = 1.0f;
constexpr float k_ScrollDirectionLeft   = -1.0f;
constexpr float k_ScrollDirectionRight  = 1.0f;
constexpr float k_ScrollDirectionUp     = -1.0f;
constexpr float k_ScrollDirectionDown   = 1.0f;
constexpr int   k_CameraScrollStep        = 1;
constexpr int   k_InitialCameraOffset     = 0;
constexpr float k_EdgeScrollSpeed           = 0.02f;

} // namespace

CameraInputController::CameraInputController(WorldDisplay& rWorldDisplay, const WorldMap& rWorldMap, const WindowLayout_t& mapLayout)
    : m_rWorldDisplay(rWorldDisplay)
    , m_rWorldMap(rWorldMap)
    , m_mapLayout(mapLayout)
    , m_edgeScrollSpeed(k_EdgeScrollSpeed)
{
}

CameraInputController::CameraBounds_t CameraInputController::ComputeMaxCamera_() const
{
    return {
        std::max(k_InitialCameraOffset, m_rWorldMap.GetWidth()  - m_rWorldDisplay.GetVisibleCols()),
        std::max(k_InitialCameraOffset, m_rWorldMap.GetHeight() - m_rWorldDisplay.GetVisibleRows())
    };
}

bool CameraInputController::HandleKey(const KeyEvent_t& rEvent)
{
    const auto [maxCamX, maxCamY] = ComputeMaxCamera_();

    int camX = m_rWorldDisplay.GetCameraX();
    int camY = m_rWorldDisplay.GetCameraY();

    if (rEvent.key == Key_t::ArrowLeft)
    {
        m_rWorldDisplay.SetCameraOffset(std::max(k_InitialCameraOffset, camX - k_CameraScrollStep), camY);
        return true;
    }
    else if (rEvent.key == Key_t::ArrowRight)
    {
        m_rWorldDisplay.SetCameraOffset(std::min(maxCamX, camX + k_CameraScrollStep), camY);
        return true;
    }
    else if (rEvent.key == Key_t::ArrowUp)
    {
        m_rWorldDisplay.SetCameraOffset(camX, std::max(k_InitialCameraOffset, camY - k_CameraScrollStep));
        return true;
    }
    else if (rEvent.key == Key_t::ArrowDown)
    {
        m_rWorldDisplay.SetCameraOffset(camX, std::min(maxCamY, camY + k_CameraScrollStep));
        return true;
    }

    return false;
}

void CameraInputController::Update(bool bEnabled)
{
    if (!bEnabled || !HasLastMousePosition())
    {
        m_edgeScrollAccumulatorX = 0.0f;
        m_edgeScrollAccumulatorY = 0.0f;
        return;
    }

    const LastMousePosition_t mousePos = GetLastMousePosition();
    ApplyEdgeScroll_(mousePos.x, mousePos.y);
}

void CameraInputController::ApplyEdgeScroll_(int mouseX, int mouseY)
{
    const auto [maxCamX, maxCamY] = ComputeMaxCamera_();

    const float relX = static_cast<float>(mouseX - m_mapLayout.x) / m_mapLayout.width;
    const float relY = static_cast<float>(mouseY - m_mapLayout.y) / m_mapLayout.height;

    const bool bInMap = relX >= k_RelativeMin && relX <= k_RelativeMax && relY >= k_RelativeMin && relY <= k_RelativeMax;
    if (!bInMap)
    {
        m_edgeScrollAccumulatorX = 0.0f;
        m_edgeScrollAccumulatorY = 0.0f;
        return;
    }

    float scrollDirX = k_RelativeMin;
    float scrollDirY = k_RelativeMin;

    if (relX < k_EdgeZone)
        scrollDirX = k_ScrollDirectionLeft;
    else if (relX > k_RelativeMax - k_EdgeZone)
        scrollDirX = k_ScrollDirectionRight;

    if (relY < k_EdgeZone)
        scrollDirY = k_ScrollDirectionUp;
    else if (relY > k_RelativeMax - k_EdgeZone)
        scrollDirY = k_ScrollDirectionDown;

    if (scrollDirX == k_RelativeMin && scrollDirY == k_RelativeMin)
    {
        m_edgeScrollAccumulatorX = 0.0f;
        m_edgeScrollAccumulatorY = 0.0f;
        return;
    }

    m_edgeScrollAccumulatorX += scrollDirX * m_edgeScrollSpeed;
    m_edgeScrollAccumulatorY += scrollDirY * m_edgeScrollSpeed;

    const int deltaX = static_cast<int>(m_edgeScrollAccumulatorX);
    const int deltaY = static_cast<int>(m_edgeScrollAccumulatorY);

    if (deltaX != 0 || deltaY != 0)
    {
        m_edgeScrollAccumulatorX -= static_cast<float>(deltaX);
        m_edgeScrollAccumulatorY -= static_cast<float>(deltaY);

        const int newCamX = std::clamp(m_rWorldDisplay.GetCameraX() + deltaX, k_InitialCameraOffset, maxCamX);
        const int newCamY = std::clamp(m_rWorldDisplay.GetCameraY() + deltaY, k_InitialCameraOffset, maxCamY);
        m_rWorldDisplay.SetCameraOffset(newCamX, newCamY);
    }
}

} // namespace ac
