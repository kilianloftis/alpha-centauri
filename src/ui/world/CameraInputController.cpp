#include "ui/world/CameraInputController.h"

#include "ui/HotkeyConfig.h"
#include "game/map/WorldMap.h"
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

// Screen-space pan (right/down positive) → camera tile delta for the 2:1 iso grid:
// screenX ∝ (relX - relY), screenY ∝ (relX + relY).
void ScreenPanToCameraDelta_(int screenDeltaX, int screenDeltaY, int& rCamDeltaX, int& rCamDeltaY)
{
    rCamDeltaX = screenDeltaX + screenDeltaY;
    rCamDeltaY = screenDeltaY - screenDeltaX;
}

} // namespace

CameraInputController::CameraInputController(WorldDisplay& rWorldDisplay, const WorldMap& rWorldMap,
                                             const WindowLayout_t& mapLayout, const HotkeyConfig& rHotkeys)
    : m_rWorldDisplay(rWorldDisplay)
    , m_rWorldMap(rWorldMap)
    , m_rHotkeys(rHotkeys)
    , m_mapLayout(mapLayout)
    , m_edgeScrollSpeed(Style().cameraInput.edgeScrollSpeed)
{
}

int CameraInputController::ComputeMaxCameraY_() const
{
    const int initialOffset = Style().cameraInput.initialCameraOffset;
    return std::max(initialOffset, m_rWorldMap.GetHeight() - m_rWorldDisplay.GetVisibleRows());
}

bool CameraInputController::ApplyCameraDelta_(int deltaCamX, int deltaCamY)
{
    if (deltaCamX == 0 && deltaCamY == 0)
    {
        return false;
    }
    const auto& s = Style().cameraInput;
    const int maxCamY = ComputeMaxCameraY_();
    MapViewport& rViewport = m_rWorldDisplay.GetViewport();
    const int newCamY =
        std::clamp(rViewport.CameraY() + deltaCamY, s.initialCameraOffset, maxCamY);
    return rViewport.SetCamera(rViewport.CameraX() + deltaCamX, newCamY);
}

bool CameraInputController::HandleKey(const KeyEvent_t& rEvent)
{
    const auto& s = Style().cameraInput;
    const int step = s.cameraScrollStep;

    const auto pan = [&](HotkeyAction_t action) {
        const std::optional<HotkeyChord_t> chord = m_rHotkeys.Find(action);
        return chord && chord->Matches(rEvent);
    };

    int screenDx = 0;
    int screenDy = 0;
    if (pan(HotkeyAction_t::PanLeft))
    {
        screenDx = -step;
    }
    else if (pan(HotkeyAction_t::PanRight))
    {
        screenDx = step;
    }
    else if (pan(HotkeyAction_t::PanUp))
    {
        screenDy = -step;
    }
    else if (pan(HotkeyAction_t::PanDown))
    {
        screenDy = step;
    }
    else
    {
        return false;
    }

    int camDx = 0;
    int camDy = 0;
    ScreenPanToCameraDelta_(screenDx, screenDy, camDx, camDy);
    return ApplyCameraDelta_(camDx, camDy);
}

bool CameraInputController::CenterOnTile(int tileX, int tileY)
{
    const int initialOffset = Style().cameraInput.initialCameraOffset;
    const int maxCamY = ComputeMaxCameraY_();
    MapViewport& rViewport = m_rWorldDisplay.GetViewport();
    const int cameraX = tileX - (rViewport.VisibleCols() / 2);
    const int cameraY = std::clamp(
        tileY - (rViewport.VisibleRows() / 2), initialOffset, maxCamY);
    return rViewport.SetCamera(cameraX, cameraY);
}

bool CameraInputController::Update(bool bEnabled, std::optional<MousePosition_t> mousePosition)
{
    if (!bEnabled || !mousePosition)
    {
        m_edgeScrollAccumulatorX = 0.0f;
        m_edgeScrollAccumulatorY = 0.0f;
        return false;
    }

    return ApplyEdgeScroll_(mousePosition->x, mousePosition->y);
}

bool CameraInputController::ApplyEdgeScroll_(int mouseX, int mouseY)
{
    const auto& s = Style().cameraInput;

    const float relX = static_cast<float>(mouseX - m_mapLayout.x) / m_mapLayout.width;
    const float relY = static_cast<float>(mouseY - m_mapLayout.y) / m_mapLayout.height;

    const bool bInMap = relX >= s.relativeMin && relX <= s.relativeMax
        && relY >= s.relativeMin && relY <= s.relativeMax;
    if (!bInMap)
    {
        m_edgeScrollAccumulatorX = 0.0f;
        m_edgeScrollAccumulatorY = 0.0f;
        return false;
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
        return false;
    }

    // Accumulate screen-space pan, then convert to iso camera steps so edges move
    // horizontally/vertically on screen rather than along tile axes.
    m_edgeScrollAccumulatorX += scrollDirX * m_edgeScrollSpeed;
    m_edgeScrollAccumulatorY += scrollDirY * m_edgeScrollSpeed;

    const int screenDx = static_cast<int>(m_edgeScrollAccumulatorX);
    const int screenDy = static_cast<int>(m_edgeScrollAccumulatorY);
    if (screenDx == 0 && screenDy == 0)
    {
        return false;
    }

    m_edgeScrollAccumulatorX -= static_cast<float>(screenDx);
    m_edgeScrollAccumulatorY -= static_cast<float>(screenDy);

    int camDx = 0;
    int camDy = 0;
    ScreenPanToCameraDelta_(screenDx, screenDy, camDx, camDy);
    return ApplyCameraDelta_(camDx, camDy);
}

} // namespace ac
