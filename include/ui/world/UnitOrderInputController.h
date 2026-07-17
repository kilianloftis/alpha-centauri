#pragma once

#include "game/units/Unit.h"
#include "game/units/Pathfinder.h"
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
    bool HandleMouse(const MouseEvent_t& rEvent, Unit* pSelectedUnit, const Tile* pHoveredTile,
                     const Pathfinder* pPathfinder);

    // Non-null while a left-click hold has exceeded the threshold and the path is valid.
    const Path_t* GetPathPreview() const;

    void CancelPreview();

private:
    static constexpr int k_holdThresholdMs = 500;

    void UpdatePreview_(Unit& rMover, const Tile& rDestination, const Pathfinder& rPathfinder);

    const std::unordered_map<Key_t, OrderHandler_t> m_orderHandlers = {
        { Key_t::H, [](Unit& rUnit) { rUnit.SetOrder(HoldOrder_t{}); } },
    };

    // Left-click long-press path preview.
    bool m_bLeftButtonHeld = false;
    std::chrono::steady_clock::time_point m_leftButtonPressTime;
    Unit* m_pPreviewUnit = nullptr;
    const Tile* m_pPreviewDestination = nullptr;
    Path_t m_pathPreview;
    bool m_bPreviewActive = false;
};

} // namespace ac
