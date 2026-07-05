#include "ui/world/UnitOrderInputController.h"

#include "game/map/Tile.h"

namespace ac
{

bool UnitOrderInputController::HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit)
{
    if (!pSelectedUnit)
    {
        return false;
    }

    auto it = m_orderHandlers.find(rEvent.key);
    if (it == m_orderHandlers.end())
    {
        return false;
    }

    it->second(*pSelectedUnit);
    return true;
}

bool UnitOrderInputController::HandleMouse(const MouseEvent_t& rEvent, Unit* pSelectedUnit, const Tile* pClickedTile)
{
    if (rEvent.button != MouseButton_t::Right)
    {
        return false;
    }

    if (rEvent.bPressed)
    {
        m_bRightButtonHeld = true;
        m_rightButtonPressTime = std::chrono::steady_clock::now();
        return true;
    }

    if (!m_bRightButtonHeld)
    {
        return false;
    }

    m_bRightButtonHeld = false;

    if (!pClickedTile || !pSelectedUnit)
    {
        return true;
    }

    const auto elapsed = std::chrono::steady_clock::now() - m_rightButtonPressTime;
    const bool bLongHold = elapsed >= std::chrono::seconds(1);
    if (bLongHold)
    {
        pSelectedUnit->SetOrder(MoveOrder_t{pClickedTile});
    }

    return true;
}

} // namespace ac
