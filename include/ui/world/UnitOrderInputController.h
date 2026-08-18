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
class GameState;
struct GameDataContext;

class UnitOrderInputController
{
public:
    using OrderHandler_t = std::function<void(Unit&)>;

    UnitOrderInputController() = default;

    bool HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit);
    // pGameState / pDataContext are used so probe validity can be deferred to
    // ProbeActionExecutor (via GameState). Attack still uses local enemy-unit checks.
    bool HandleMouse(const MouseEvent_t& rEvent, Unit* pSelectedUnit, const Tile* pHoveredTile,
                     const Pathfinder* pPathfinder, GameState* pGameState = nullptr,
                     const GameDataContext* pDataContext = nullptr);

    // True after the most recent HandleKey/HandleMouse call assigned an order.
    bool WasOrderAssigned() const { return m_bOrderAssigned; }

    // True after a long-press release on an adjacent tile with a non-allied unit.
    // Target remains valid until the next HandleKey/HandleMouse call.
    bool WasAttackRequested() const { return m_bAttackRequested; }
    const Tile* GetAttackTarget() const { return m_pInteractTarget; }

    // True after a long-press release when ProbeActionExecutor reports a legal probe menu
    // against that tile. Target is that tile.
    bool WasProbeActionRequested() const { return m_bProbeActionRequested; }
    const Tile* GetProbeTarget() const { return m_pInteractTarget; }

    // True after O on a supply-capable unit with a home base. WorldView opens the resource
    // picker; the controller does not assign the order itself.
    bool WasSupplyCrawlRequested() const { return m_bSupplyCrawlRequested; }

    // True after B on a FoundBase-capable unit. WorldView runs
    // UnitOrderExecutor::TryFoundBase (tile legality is checked there).
    bool WasFoundBaseRequested() const { return m_bFoundBaseRequested; }

    // True after L. WorldView tries TryAttachToTransport; on failure terraform may use L.
    bool WasAttachTransportRequested() const { return m_bAttachTransportRequested; }

    // True after Shift+U. WorldView runs TryUnloadTransport.
    bool WasUnloadTransportRequested() const { return m_bUnloadTransportRequested; }

    // True after Shift+D. WorldView opens Disband / Self Destruct / Cancel.
    bool WasDisbandRequested() const { return m_bDisbandRequested; }

    // Non-null while a left-click hold has exceeded the threshold and the path is valid.
    const Path_t* GetPathPreview() const;

    void CancelPreview();

private:
    void ClearRequestFlags_();
    bool HasExceededHoldThreshold_() const;

    bool HandleLeftButton_(const MouseEvent_t& rEvent, Unit* pSelectedUnit,
                           const Tile* pHoveredTile, const Pathfinder* pPathfinder,
                           GameState* pGameState, const GameDataContext* pDataContext);
    bool BeginLeftHold_(Unit* pSelectedUnit, const Tile* pHoveredTile);
    bool FinishLeftHold_(const Pathfinder* pPathfinder, GameState* pGameState,
                         const GameDataContext* pDataContext);
    bool TryResolveHoldRelease_(Unit& rMover, const Tile& rDest, const Pathfinder& rPathfinder,
                                GameState* pGameState, const GameDataContext* pDataContext);
    bool TryAdjacentInteract_(Unit& rMover, const Tile& rDest, GameState* pGameState,
                              const GameDataContext* pDataContext, const Unit* pVisibleHostile);
    bool TryAssignMoveOrder_(Unit& rMover, const Tile& rDest, const Unit* pVisibleHostile);
    bool UpdateHoldPreview_(Unit& rSelectedUnit, const Tile* pHoveredTile,
                            const Pathfinder& rPathfinder);

    void UpdatePreview_(Unit& rMover, const Tile& rDestination, const Pathfinder& rPathfinder);

    const std::unordered_map<Key_t, OrderHandler_t> m_orderHandlers = {
        { Key_t::H, [this](Unit& rUnit) {
            rUnit.SetOrder(HoldOrder_t{});
            m_bOrderAssigned = true;
        } },
        { Key_t::Space, [this](Unit& rUnit) {
            rUnit.SetOrder(SkipTurnOrder_t{});
            m_bOrderAssigned = true;
        } },
    };

    bool m_bOrderAssigned = false;
    bool m_bAttackRequested = false;
    bool m_bProbeActionRequested = false;
    bool m_bSupplyCrawlRequested = false;
    bool m_bFoundBaseRequested = false;
    bool m_bAttachTransportRequested = false;
    bool m_bUnloadTransportRequested = false;
    bool m_bDisbandRequested = false;
    const Tile* m_pInteractTarget = nullptr;

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
