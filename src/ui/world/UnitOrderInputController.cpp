#include "ui/world/UnitOrderInputController.h"

#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/EffectEnums.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitOrderExecutor.h"
#include "ui/style/UiStyle.h"

namespace ac
{

bool UnitOrderInputController::HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit)
{
    ClearRequestFlags_();

    if (!pSelectedUnit)
    {
        return false;
    }

    // O opens the supply-crawl resource picker (WorldView reacts to WasSupplyCrawlRequested).
    // Only consume the key when the unit can actually crawl, so global O (Settings) still works.
    if (rEvent.key == Key_t::O)
    {
        if (pSelectedUnit->GetFlag(RuleFlagId_t::SupplyCrawl) && pSelectedUnit->GetHomeBase())
        {
            m_bSupplyCrawlRequested = true;
            return true;
        }
        return false;
    }

    // B requests founding a base (WorldView runs UnitOrderExecutor::TryFoundBase).
    // Only consume when the unit has FoundBase so B stays free for other UI on ordinary units.
    if (rEvent.key == Key_t::B)
    {
        if (pSelectedUnit->GetFlag(RuleFlagId_t::FoundBase))
        {
            m_bFoundBaseRequested = true;
            return true;
        }
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

bool UnitOrderInputController::HandleMouse(const MouseEvent_t& rEvent, Unit* pSelectedUnit,
                                           const Tile* pHoveredTile,
                                           const Pathfinder* pPathfinder, GameState* pGameState,
                                           const GameDataContext* pDataContext)
{
    ClearRequestFlags_();

    if (rEvent.button == MouseButton_t::Left)
    {
        return HandleLeftButton_(rEvent, pSelectedUnit, pHoveredTile, pPathfinder, pGameState,
                                 pDataContext);
    }

    // Mouse move: update preview destination while holding.
    if (rEvent.button == MouseButton_t::None && m_bLeftButtonHeld && pSelectedUnit && pPathfinder)
    {
        return UpdateHoldPreview_(*pSelectedUnit, pHoveredTile, *pPathfinder);
    }

    return false;
}

void UnitOrderInputController::ClearRequestFlags_()
{
    m_bOrderAssigned = false;
    m_bAttackRequested = false;
    m_bSupplyCrawlRequested = false;
    m_bFoundBaseRequested = false;
    m_bProbeActionRequested = false;
    m_pInteractTarget = nullptr;
}

bool UnitOrderInputController::HasExceededHoldThreshold_() const
{
    const auto elapsed = std::chrono::steady_clock::now() - m_leftButtonPressTime;
    return elapsed >= std::chrono::milliseconds(Style().unitOrderInput.holdThresholdMs);
}

bool UnitOrderInputController::HandleLeftButton_(const MouseEvent_t& rEvent, Unit* pSelectedUnit,
                                                 const Tile* pHoveredTile,
                                                 const Pathfinder* pPathfinder,
                                                 GameState* pGameState,
                                                 const GameDataContext* pDataContext)
{
    if (rEvent.bPressed)
    {
        return BeginLeftHold_(pSelectedUnit, pHoveredTile);
    }

    return FinishLeftHold_(pPathfinder, pGameState, pDataContext);
}

bool UnitOrderInputController::BeginLeftHold_(Unit* pSelectedUnit, const Tile* pHoveredTile)
{
    if (!pSelectedUnit)
    {
        return false;
    }

    m_bLeftButtonHeld = true;
    m_leftButtonPressTime = std::chrono::steady_clock::now();
    m_pPreviewUnit = pSelectedUnit;
    m_pPreviewDestination = pHoveredTile;
    m_bPreviewActive = false;
    m_bPreviewSearchDone = false;
    return true;
}

bool UnitOrderInputController::FinishLeftHold_(const Pathfinder* pPathfinder, GameState* pGameState,
                                               const GameDataContext* pDataContext)
{
    if (!m_bLeftButtonHeld)
    {
        return false;
    }

    m_bLeftButtonHeld = false;

    Unit* pMover = m_pPreviewUnit;
    const Tile* pDest = m_pPreviewDestination;
    const bool bHeldLongEnough = HasExceededHoldThreshold_() || m_bPreviewActive;
    const bool bOtherTile = pMover && pDest && pDest != &pMover->GetTile();

    if (bOtherTile && pPathfinder && bHeldLongEnough
        && TryResolveHoldRelease_(*pMover, *pDest, *pPathfinder, pGameState, pDataContext))
    {
        return true;
    }

    CancelPreview();
    // Long-press on another tile that did not yield an order is an aborted pathfinding
    // gesture — consume it so the map does not fall through to tile / unit selection.
    // Short clicks (below the hold threshold) still fall through for normal selection.
    return bOtherTile && bHeldLongEnough;
}

bool UnitOrderInputController::TryResolveHoldRelease_(Unit& rMover, const Tile& rDest,
                                                      const Pathfinder& rPathfinder,
                                                      GameState* pGameState,
                                                      const GameDataContext* pDataContext)
{
    const WorldMap& rMap = rPathfinder.GetWorldMap();
    const bool bAdjacent = AreChebyshevAdjacent(rMover.GetTile(), rDest, rMap.GetWidth());
    // Both act-on questions are the game layer's to answer: CanOpenProbeActions
    // covers adjacency, visibility and probe eligibility, and FindVisibleHostileOnTile
    // is the same gate TryAttack uses.
    const Unit* pVisibleHostile =
        pGameState ? pGameState->GetUnitOrderExecutor().FindVisibleHostileOnTile(rMover, rDest)
                   : nullptr;

    if (bAdjacent
        && TryAdjacentInteract_(rMover, rDest, pGameState, pDataContext, pVisibleHostile))
    {
        return true;
    }

    return TryAssignMoveOrder_(rMover, rDest, pVisibleHostile);
}

bool UnitOrderInputController::TryAdjacentInteract_(Unit& rMover, const Tile& rDest,
                                                    GameState* pGameState,
                                                    const GameDataContext* pDataContext,
                                                    const Unit* pVisibleHostile)
{
    if (pGameState && pDataContext
        && pGameState->GetProbeActions().CanOpenProbeActions(rMover, rDest, *pGameState,
                                                             *pDataContext))
    {
        m_bProbeActionRequested = true;
        m_pInteractTarget = &rDest;
        CancelPreview();
        return true;
    }

    if (pVisibleHostile)
    {
        m_bAttackRequested = true;
        m_pInteractTarget = &rDest;
        CancelPreview();
        return true;
    }

    return false;
}

bool UnitOrderInputController::TryAssignMoveOrder_(Unit& rMover, const Tile& rDest,
                                                   const Unit* pVisibleHostile)
{
    // Everything else is a move. A seen hostile is the one case the planner refuses
    // to route into, so order the move anyway and let Execute_ close in via
    // DesiredContactStep. Undefended bases plan fine (only units and ZoC block a
    // step), and concealed occupants are invisible to the planner too — so both
    // take the ordinary reachability path, and the step that fails on arrival
    // reveals whatever was hiding there.
    if (!pVisibleHostile && !(m_bPreviewActive && m_pathPreview.bReachable))
    {
        return false;
    }

    rMover.SetOrder(MoveOrder_t{&rDest});
    m_bOrderAssigned = true;
    CancelPreview();
    return true;
}

bool UnitOrderInputController::UpdateHoldPreview_(Unit& rSelectedUnit, const Tile* pHoveredTile,
                                                  const Pathfinder& rPathfinder)
{
    if (!HasExceededHoldThreshold_())
    {
        return false;
    }

    if (pHoveredTile && pHoveredTile != &rSelectedUnit.GetTile())
    {
        m_pPreviewUnit = &rSelectedUnit;
        if (pHoveredTile != m_pPreviewDestination)
        {
            m_pPreviewDestination = pHoveredTile;
            m_bPreviewSearchDone = false;
            UpdatePreview_(rSelectedUnit, *pHoveredTile, rPathfinder);
        }
        else if (!m_bPreviewSearchDone)
        {
            // First search after the hold threshold (destination was set on press).
            UpdatePreview_(rSelectedUnit, *pHoveredTile, rPathfinder);
        }
    }
    else
    {
        m_bPreviewActive = false;
    }

    return false;
}

void UnitOrderInputController::UpdatePreview_(Unit& rMover, const Tile& rDestination,
                                              const Pathfinder& rPathfinder)
{
    m_pathPreview = rPathfinder.FindPath(rMover, rDestination);
    m_bPreviewSearchDone = true;
    m_bPreviewActive = m_pathPreview.bReachable && !m_pathPreview.tiles.empty();
}

const Path_t* UnitOrderInputController::GetPathPreview() const
{
    if (m_bPreviewActive)
    {
        return &m_pathPreview;
    }
    return nullptr;
}

void UnitOrderInputController::CancelPreview()
{
    m_bPreviewActive = false;
    m_bPreviewSearchDone = false;
    m_pPreviewUnit = nullptr;
    m_pPreviewDestination = nullptr;
    m_pathPreview = {};
}

} // namespace ac
