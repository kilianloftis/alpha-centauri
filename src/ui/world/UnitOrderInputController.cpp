#include "ui/world/UnitOrderInputController.h"

#include "ui/HotkeyConfig.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/AttackRules.h"
#include "game/units/AirdropRules.h"
#include "ui/style/UiStyle.h"

#include <magic_enum.hpp>
#include <stdexcept>
#include <vector>

namespace ac
{

UnitOrderInputController::UnitOrderInputController(const HotkeyConfig& rHotkeys)
    : m_rHotkeys(rHotkeys)
{
}

bool UnitOrderInputController::HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit)
{
    ClearRequestFlags_();

    if (!pSelectedUnit)
    {
        return false;
    }

    std::vector<HotkeyAction_t> valid;
    const auto consider = [&](HotkeyAction_t action, bool bValid)
    {
        const std::optional<HotkeyChord_t> chord = m_rHotkeys.Find(action);
        if (!chord || !chord->Matches(rEvent) || !bValid)
        {
            return;
        }
        valid.push_back(action);
    };

    // Unload, disband, and attach are available for any selected unit. WorldView runs the
    // order, and attach falls through when boarding fails. The rest wait for a unit that
    // can actually do them, so a shared chord can mean a different order.
    consider(HotkeyAction_t::UnloadTransport, true);
    consider(HotkeyAction_t::Disband, true);
    consider(HotkeyAction_t::AttachTransport, true);
    consider(HotkeyAction_t::SupplyCrawl,
             pSelectedUnit->GetFlag(RuleFlagId_t::SupplyCrawl) && pSelectedUnit->GetHomeBase());
    consider(HotkeyAction_t::FoundBase, pSelectedUnit->GetFlag(RuleFlagId_t::FoundBase));
    consider(HotkeyAction_t::Detonate, UnitCanDetonate(*pSelectedUnit));
    consider(HotkeyAction_t::Airdrop, CanAttemptAirdrop(*pSelectedUnit).Ok());
    consider(HotkeyAction_t::Hold, true);
    consider(HotkeyAction_t::SkipTurn, true);

    if (valid.empty())
    {
        return false;
    }
    if (valid.size() > 1)
    {
        std::string message = "More than one order is valid:";
        for (const HotkeyAction_t action : valid)
        {
            message += " ";
            message += magic_enum::enum_name(action);
        }
        throw std::logic_error(message);
    }

    switch (valid.front())
    {
    case HotkeyAction_t::UnloadTransport:
        m_bUnloadTransportRequested = true;
        break;
    case HotkeyAction_t::Disband:
        m_bDisbandRequested = true;
        break;
    case HotkeyAction_t::AttachTransport:
        m_bAttachTransportRequested = true;
        break;
    case HotkeyAction_t::SupplyCrawl:
        m_bSupplyCrawlRequested = true;
        break;
    case HotkeyAction_t::FoundBase:
        m_bFoundBaseRequested = true;
        break;
    case HotkeyAction_t::Detonate:
        m_bDetonateRequested = true;
        break;
    case HotkeyAction_t::Airdrop:
        m_bAirdropModeToggleRequested = true;
        break;
    case HotkeyAction_t::Hold:
        pSelectedUnit->SetOrder(HoldOrder_t{});
        m_bOrderAssigned = true;
        break;
    case HotkeyAction_t::SkipTurn:
        pSelectedUnit->SetOrder(SkipTurnOrder_t{});
        m_bOrderAssigned = true;
        break;
    case HotkeyAction_t::Bombard:
    case HotkeyAction_t::PanLeft:
    case HotkeyAction_t::PanRight:
    case HotkeyAction_t::PanUp:
    case HotkeyAction_t::PanDown:
    case HotkeyAction_t::Cancel:
    case HotkeyAction_t::EndTurn:
    case HotkeyAction_t::NextUnit:
    case HotkeyAction_t::Research:
    case HotkeyAction_t::SocialEngineering:
    case HotkeyAction_t::UnitDesigner:
    case HotkeyAction_t::Settings:
    case HotkeyAction_t::Satellites:
        break;
    }
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
    m_bBombardRequested = false;
    m_bSupplyCrawlRequested = false;
    m_bFoundBaseRequested = false;
    m_bAttachTransportRequested = false;
    m_bUnloadTransportRequested = false;
    m_bDisbandRequested = false;
    m_bAirdropModeToggleRequested = false;
    m_bProbeActionRequested = false;
    m_pInteractTarget = nullptr;
    m_bDetonateRequested = false;
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
    if (rMover.GetFlag(RuleFlagId_t::Bombard) && pGameState
        && IsWithinBombardRange(rMover, rDest, rMap))
    {
        m_bBombardRequested = true;
        m_pInteractTarget = &rDest;
        CancelPreview();
        return true;
    }

    const bool bAdjacent = AreChebyshevAdjacent(rMover.GetTile(), rDest, rMap.GetWidth());
    // Act-on questions are the game layer's: CanOpenProbeActions for probes;
    // FindAttackableHostileOnTile matches TryAttack's declare gate. A visible but
    // non-attackable hostile still steers move orders (planner contact path).
    const Unit* pVisibleHostile =
        pGameState ? pGameState->GetUnitOrderExecutor().FindVisibleHostileOnTile(rMover, rDest)
                   : nullptr;
    const Unit* pAttackableHostile =
        (pGameState && !rMover.GetFlag(RuleFlagId_t::Bombard))
            ? FindAttackableHostileOnTile(rMover, rDest, rMap, pGameState->GetTileEffects())
            : nullptr;

    if (bAdjacent
        && TryAdjacentInteract_(rMover, rDest, pGameState, pDataContext, pAttackableHostile))
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
