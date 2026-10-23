#include "ui/world/UnitOrderInputController.h"

#include "ui/HotkeyConfig.h"
#include "game/GameState.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TriggeredEffectRules.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/combat/AttackRules.h"
#include "game/movement/AirdropRules.h"
#include "ui/style/UiStyle.h"

#include <magic_enum.hpp>
#include <stdexcept>
#include <vector>

namespace ac
{

UnitOrderInputController::UnitOrderInputController(const HotkeyConfig& rHotkeys,
                                                   GameState& rGameState)
    : m_rHotkeys(rHotkeys)
    , m_rGameState(rGameState)
{
}

bool UnitOrderInputController::HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit)
{
    ClearRequestFlags_();
    if (!pSelectedUnit)
    {
        return false;
    }
    const std::vector<HotkeyAction_t> valid = MatchingOrderActions_(rEvent, *pSelectedUnit);
    if (valid.empty())
    {
        return false;
    }
    RequireSingleOrder_(valid);
    ApplyOrderAction_(valid.front(), *pSelectedUnit);
    return true;
}

// Unload, disband and attach apply to any selected unit; the rest wait for one that can do
// them, so a shared chord can mean a different order.
std::vector<HotkeyAction_t> UnitOrderInputController::MatchingOrderActions_(
    const KeyEvent_t& rEvent, const Unit& rUnit) const
{
    std::vector<HotkeyAction_t> valid;
    const auto consider = [&](HotkeyAction_t action, bool bValid)
    {
        if (bValid && m_rHotkeys.Matches(action, rEvent))
        {
            valid.push_back(action);
        }
    };
    consider(HotkeyAction_t::UnloadTransport, true);
    consider(HotkeyAction_t::Disband, true);
    consider(HotkeyAction_t::AttachTransport, true);
    consider(HotkeyAction_t::SupplyCrawl,
             ResolveFlag(rUnit, RuleFlagId_t::SupplyCrawl) && rUnit.GetHomeBase());
    consider(HotkeyAction_t::FoundBase, ResolveFlag(rUnit, RuleFlagId_t::FoundBase));
    consider(HotkeyAction_t::Detonate, UnitCanDetonate(rUnit));
    consider(HotkeyAction_t::Airdrop, CanAttemptAirdrop(rUnit).Ok());
    consider(HotkeyAction_t::Hold, true);
    consider(HotkeyAction_t::SkipTurn, true);
    return valid;
}

void UnitOrderInputController::RequireSingleOrder_(const std::vector<HotkeyAction_t>& rValid)
{
    if (rValid.size() <= 1)
    {
        return;
    }
    std::string message = "More than one order is valid:";
    for (const HotkeyAction_t action : rValid)
    {
        message += " ";
        message += magic_enum::enum_name(action);
    }
    throw std::logic_error(message);
}

void UnitOrderInputController::ApplyOrderAction_(HotkeyAction_t action, Unit& rUnit)
{
    switch (action)
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
        rUnit.SetOrder(HoldOrder_t{});
        m_bOrderAssigned = true;
        break;
    case HotkeyAction_t::SkipTurn:
        rUnit.SetOrder(SkipTurnOrder_t{});
        m_bOrderAssigned = true;
        break;
    case HotkeyAction_t::Bombard:
    case HotkeyAction_t::PanLeft:
    case HotkeyAction_t::PanRight:
    case HotkeyAction_t::PanUp:
    case HotkeyAction_t::PanDown:
    case HotkeyAction_t::ZoomIn:
    case HotkeyAction_t::ZoomOut:
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
}

bool UnitOrderInputController::HandleMouse(const MouseEvent_t& rEvent, Unit* pSelectedUnit,
                                           const Tile* pHoveredTile)
{
    ClearRequestFlags_();

    if (rEvent.button == MouseButton_t::Left)
    {
        return HandleLeftButton_(rEvent, pSelectedUnit, pHoveredTile);
    }

    // Mouse move: update preview destination while holding.
    if (rEvent.button == MouseButton_t::None && m_bLeftButtonHeld && pSelectedUnit)
    {
        return UpdateHoldPreview_(*pSelectedUnit, pHoveredTile);
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
                                                 const Tile* pHoveredTile)
{
    if (rEvent.bPressed)
    {
        return BeginLeftHold_(pSelectedUnit, pHoveredTile);
    }

    return FinishLeftHold_();
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

bool UnitOrderInputController::FinishLeftHold_()
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

    if (bOtherTile && bHeldLongEnough && TryResolveHoldRelease_(*pMover, *pDest))
    {
        return true;
    }

    CancelPreview();
    // A long press elsewhere that gave no order is an aborted path gesture, consumed; short
    // clicks still fall through to selection.
    return bOtherTile && bHeldLongEnough;
}

bool UnitOrderInputController::TryResolveHoldRelease_(Unit& rMover, const Tile& rDest)
{
    const WorldMap& rMap = m_rGameState.GetWorldMap();
    const bool bBombards = ResolveFlag(rMover, RuleFlagId_t::Bombard);
    if (bBombards && IsWithinBombardRange(rMover, rDest))
    {
        m_bBombardRequested = true;
        m_pInteractTarget = &rDest;
        CancelPreview();
        return true;
    }

    const bool bAdjacent = AreChebyshevAdjacent(rMover.GetTile(), rDest, rMap.GetWidth());
    // Whether to act on the tile is the game layer's question. A visible but non-attackable
    // hostile still steers a move order.
    const Unit* pVisibleHostile = FindVisibleHostileOnTile(rMover, rDest);
    const Unit* pAttackableHostile = bBombards ? nullptr : FindAttackableHostileOnTile(rMover, rDest);

    if (bAdjacent && TryAdjacentInteract_(rMover, rDest, pAttackableHostile))
    {
        return true;
    }

    return TryAssignMoveOrder_(rMover, rDest, pVisibleHostile);
}

bool UnitOrderInputController::TryAdjacentInteract_(Unit& rMover, const Tile& rDest,
                                                    const Unit* pVisibleHostile)
{
    if (m_rGameState.GetProbeActions().CanOpenProbeActions(rMover, rDest, m_rGameState))
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
    // Everything else is a move. A seen hostile is the one target the planner will not route into,
    // so the move is ordered anyway and Execute_ closes in.
    if (!pVisibleHostile && !(m_bPreviewActive && m_pathPreview.bReachable))
    {
        return false;
    }

    rMover.SetOrder(MoveOrder_t{&rDest});
    m_bOrderAssigned = true;
    CancelPreview();
    return true;
}

bool UnitOrderInputController::UpdateHoldPreview_(Unit& rSelectedUnit, const Tile* pHoveredTile)
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
            UpdatePreview_(rSelectedUnit, *pHoveredTile);
        }
        else if (!m_bPreviewSearchDone)
        {
            // First search after the hold threshold (destination was set on press).
            UpdatePreview_(rSelectedUnit, *pHoveredTile);
        }
    }
    else
    {
        m_bPreviewActive = false;
    }

    return false;
}

void UnitOrderInputController::UpdatePreview_(Unit& rMover, const Tile& rDestination)
{
    m_pathPreview = m_rGameState.GetPathfinder().FindPath(rMover, rDestination);
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
