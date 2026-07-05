#pragma once

#include "game/units/Unit.h"
#include "input/Input.h"

#include <chrono>
#include <functional>
#include <unordered_map>

namespace ac
{

class Tile;

class UnitOrderInputController
{
public:
    using OrderHandler_t = std::function<void(Unit&)>;

    UnitOrderInputController() = default;

    bool HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit);
    bool HandleMouse(const MouseEvent_t& rEvent, Unit* pSelectedUnit, const Tile* pClickedTile);

private:
    const std::unordered_map<Key_t, OrderHandler_t> m_orderHandlers = {
        { Key_t::H, [](Unit& rUnit) { rUnit.SetOrder(HoldOrder_t{}); } },
        // TODO: Add handlers for additional order types as hotkeys are defined.
    };

    bool m_bRightButtonHeld = false;
    std::chrono::steady_clock::time_point m_rightButtonPressTime;
};

} // namespace ac
