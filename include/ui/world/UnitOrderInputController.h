#pragma once

#include "game/units/Unit.h"
#include "game/movement/Pathfinder.h"
#include "input/Input.h"
#include "ui/HotkeyConfig.h"

#include <chrono>
#include <optional>
#include <vector>

namespace ac
{

class Tile;
class GameState;

// What WorldView carries out for an order input. OrderAssigned means the controller already set
// the unit's order; the others are WorldView's to perform.
enum class UnitOrderRequestKind_t
{
    OrderAssigned,
    AttachTransport,  // L: on failure terraform may use L
    UnloadTransport,  // Shift+U
    SupplyCrawl,      // O: WorldView opens the resource picker
    Disband,          // Shift+D: Disband / Self Destruct / Cancel
    FoundBase,        // B: tile legality is TryFoundBase's
    Detonate,         // Shift+X on a design with on_detonate_effects
    ToggleAirdrop,    // I on an airdrop-capable unit that may drop now
    Probe,            // long-press release where a probe menu is legal
    Attack,           // long-press release on an adjacent hostile
    Bombard,          // long-press release inside bombard range
};

struct UnitOrderRequest_t
{
    UnitOrderRequestKind_t kind = UnitOrderRequestKind_t::OrderAssigned;
    // The tile a Probe, Attack or Bombard acts on; null otherwise.
    const Tile* pTarget = nullptr;
};

class UnitOrderInputController
{
public:
    UnitOrderInputController(const HotkeyConfig& rHotkeys, GameState& rGameState);

    bool HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit);
    bool HandleMouse(const MouseEvent_t& rEvent, Unit* pSelectedUnit, const Tile* pHoveredTile);

    // What the last HandleKey / HandleMouse call asks WorldView to do, if anything. Valid until
    // the next call.
    const std::optional<UnitOrderRequest_t>& GetRequest() const { return m_request; }

    // Non-null while a left-click hold has exceeded the threshold and the path is valid.
    const Path_t* GetPathPreview() const;

    void CancelPreview();

private:
    std::vector<HotkeyAction_t> MatchingOrderActions_(const KeyEvent_t& rEvent,
                                                      const Unit& rUnit) const;
    // A chord that means two orders for this unit is a hotkey config defect.
    static void RequireSingleOrder_(const std::vector<HotkeyAction_t>& rValid);
    // Raises the request flag WorldView acts on, or assigns Hold / Skip directly.
    void ApplyOrderAction_(HotkeyAction_t action, Unit& rUnit);
    void Request_(UnitOrderRequestKind_t kind, const Tile* pTarget);
    bool HasExceededHoldThreshold_() const;

    bool HandleLeftButton_(const MouseEvent_t& rEvent, Unit* pSelectedUnit,
                           const Tile* pHoveredTile);
    bool BeginLeftHold_(Unit* pSelectedUnit, const Tile* pHoveredTile);
    bool FinishLeftHold_();
    bool TryResolveHoldRelease_(Unit& rMover, const Tile& rDest);
    // Probe legality is ProbeActionExecutor's; attack legality is the game layer's.
    bool TryAdjacentInteract_(Unit& rMover, const Tile& rDest, const Unit* pVisibleHostile);
    bool TryAssignMoveOrder_(Unit& rMover, const Tile& rDest, const Unit* pVisibleHostile);
    bool UpdateHoldPreview_(Unit& rSelectedUnit, const Tile* pHoveredTile);

    void UpdatePreview_(Unit& rMover, const Tile& rDestination);

    const HotkeyConfig& m_rHotkeys;
    GameState& m_rGameState;

    std::optional<UnitOrderRequest_t> m_request;

    // Left-click long-press path preview.
    bool m_bLeftButtonHeld = false;
    std::chrono::steady_clock::time_point m_leftButtonPressTime;
    Unit* m_pPreviewUnit = nullptr;
    const Tile* m_pPreviewDestination = nullptr;
    Path_t m_pathPreview;
    // True when a path graphic should be drawn (reachable non-empty path).
    bool m_bPreviewActive = false;
    // True after FindPath has been run for m_pPreviewDestination (reachable or not).
    // Prevents re-flooding the map on every mouse-move for unreachable goals.
    bool m_bPreviewSearchDone = false;
};

} // namespace ac
