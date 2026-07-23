#include "ui/world/CameraInputController.h"

#include "game/map/WorldMap.h"
#include "input/MouseEventQueue.h"
#include "ui/style/UiStyle.h"
#include "ui/world/MapViewport.h"
#include "ui/world/WorldDisplay.h"

#include <algorithm>

namespace ac
{

namespace
{

constexpr float k_ScrollDirectionLeft   = -1.0f;
constexpr float k_ScrollDirectionRight  = 1.0f;
constexpr float k_ScrollDirectionUp     = -1.0f;
constexpr float k_ScrollDirectionDown   = 1.0f;

} // namespace

CameraInputController::CameraInputController(WorldDisplay& rWorldDisplay, const WorldMap& rWorldMap, const WindowLayout_t& mapLayout)
    : m_rWorldDisplay(rWorldDisplay)
    , m_rWorldMap(rWorldMap)
    , m_mapLayout(mapLayout)
    , m_edgeScrollSpeed(Style().cameraInput.edgeScrollSpeed)
{
}

int CameraInputController::ComputeMaxCameraY_() const
{
    const int initialOffset = Style().cameraInput.initialCameraOffset;
    return std::max(initialOffset, m_rWorldMap.GetHeight() - m_rWorldDisplay.GetVisibleRows());
}

bool CameraInputController::HandleKey(const KeyEvent_t& rEvent)
{
    const auto& s = Style().cameraInput;
    const int maxCamY = ComputeMaxCameraY_();
    MapViewport& rViewport = m_rWorldDisplay.GetViewport();

    const int camX = rViewport.CameraX();
    const int camY = rViewport.CameraY();

    if (rEvent.key == Key_t::ArrowLeft)
    {
        rViewport.ScrollBy(-s.cameraScrollStep, 0);
        return true;
    }
    else if (rEvent.key == Key_t::ArrowRight)
    {
        rViewport.ScrollBy(s.cameraScrollStep, 0);
        return true;
    }
    else if (rEvent.key == Key_t::ArrowUp)
    {
        rViewport.SetCamera(camX, std::max(s.initialCameraOffset, camY - s.cameraScrollStep));
        return true;
    }
    else if (rEvent.key == Key_t::ArrowDown)
    {
        rViewport.SetCamera(camX, std::min(maxCamY, camY + s.cameraScrollStep));
        return true;
    }

    return false;
}

void CameraInputController::CenterOnTile(int tileX, int tileY)
{
    const int initialOffset = Style().cameraInput.initialCameraOffset;
    const int maxCamY = ComputeMaxCameraY_();
    MapViewport& rViewport = m_rWorldDisplay.GetViewport();
    const int cameraX = tileX - (rViewport.VisibleCols() / 2);
    const int cameraY = std::clamp(
        tileY - (rViewport.VisibleRows() / 2), initialOffset, maxCamY);
    rViewport.SetCamera(cameraX, cameraY);
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
    const auto& s = Style().cameraInput;
    const int maxCamY = ComputeMaxCameraY_();
    MapViewport& rViewport = m_rWorldDisplay.GetViewport();

    const float relX = static_cast<float>(mouseX - m_mapLayout.x) / m_mapLayout.width;
    const float relY = static_cast<float>(mouseY - m_mapLayout.y) / m_mapLayout.height;

    const bool bInMap = relX >= s.relativeMin && relX <= s.relativeMax
        && relY >= s.relativeMin && relY <= s.relativeMax;
    if (!bInMap)
    {
        m_edgeScrollAccumulatorX = 0.0f;
        m_edgeScrollAccumulatorY = 0.0f;
        return;
    }

    float scrollDirX = s.relativeMin;
    float scrollDirY = s.relativeMin;

    if (relX < s.edgeZone)
        scrollDirX = k_ScrollDirectionLeft;
    else if (relX > s.relativeMax - s.edgeZone)
        scrollDirX = k_ScrollDirectionRight;

    if (relY < s.edgeZone)
        scrollDirY = k_ScrollDirectionUp;
    else if (relY > s.relativeMax - s.edgeZone)
        scrollDirY = k_ScrollDirectionDown;

    if (scrollDirX == s.relativeMin && scrollDirY == s.relativeMin)
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

        const int newCamY = std::clamp(rViewport.CameraY() + deltaY, s.initialCameraOffset, maxCamY);
        rViewport.SetCamera(rViewport.CameraX() + deltaX, newCamY);
    }
}

} // namespace ac
