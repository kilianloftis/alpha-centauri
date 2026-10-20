#include "game/probes/ProbeActionExecutor.h"

#include "game/CurrentGameState.h"
#include "game/combat/MoraleRules.h"
#include "game/probes/ProbeActionMutations.h"
#include "game/probes/ProbeRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/economy/EconomyManager.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"

#include <algorithm>
#include <optional>

namespace ac
{

namespace
{

bool AreOnOrAdjacent_(const Tile& rA, const Tile& rB, const WorldMap& rMap)
{
    if (&rA == &rB)
    {
        return true;
    }
    return AreChebyshevAdjacent(rA, rB, rMap.GetWidth());
}

} // namespace

ProbeActionExecutor::ProbeActionExecutor(const MoraleCalculator& rMorale,
                                         const ProbeActionsConfig_t& rProbeActions,
                                         std::mt19937& rRng)
    : m_rMorale(rMorale)
    , m_rProbeActions(rProbeActions)
    , m_rRng(rRng)
{
}

bool ProbeActionExecutor::CanTryProbeAction_(const Unit& rUnit,
                                             const ProbeActionConfig_t& rAction,
                                             const ProbeTarget_t& rTarget) const
{
    if (rUnit.GetMoveFragmentsRemaining() <= 0)
    {
        return false;
    }
    const WorldMap& rWorldMap = CurrentGameState().GetWorldMap();
    if (!AreOnOrAdjacent_(rUnit.GetTile(), rTarget.rTile, rWorldMap))
    {
        return false;
    }
    return CanProbeAction(rUnit, rAction, rTarget);
}

bool ProbeActionExecutor::CanTryProbeAction(const Unit& rUnit,
                                            const ProbeActionConfig_t& rAction,
                                            const Tile& rTile, GameState& rGameState) const
{
    RequireCurrentGameState(rGameState);
    return ResolveEligibleTarget_(rUnit, rTile, rAction, rGameState).has_value();
}

std::vector<std::pair<ProbeActionId_t, std::string>> ProbeActionExecutor::ListAvailableProbeActions(
    const Unit& rProbe, const Tile& rTile, GameState& rGameState) const
{
    RequireCurrentGameState(rGameState);
    std::vector<std::pair<ProbeActionId_t, std::string>> actions;
    for (const ProbeActionConfig_t& rAction : m_rProbeActions.actions)
    {
        if (CanTryProbeAction(rProbe, rAction, rTile, rGameState))
        {
            actions.emplace_back(rAction.id, rAction.name);
        }
    }
    return actions;
}

bool ProbeActionExecutor::CanOpenProbeActions(const Unit& rProbe, const Tile& rTile,
                                              GameState& rGameState) const
{
    RequireCurrentGameState(rGameState);
    return !ListAvailableProbeActions(rProbe, rTile, rGameState).empty();
}

ProbeActionResult_t ProbeActionExecutor::TryProbeAction(
    Unit& rUnit, ProbeActionId_t actionId, const Tile& rTile, GameState& rGameState,
    const BuildingId_t& facilityId)
{
    RequireCurrentGameState(rGameState);
    ProbeActionResult_t result;
    const ProbeActionConfig_t* pAction = m_rProbeActions.Find(actionId);
    if (!pAction)
    {
        return result;
    }

    const std::optional<ProbeTarget_t> target =
        ResolveEligibleTarget_(rUnit, rTile, *pAction, rGameState);
    if (!target.has_value() || !TryPayProbeCost_(rUnit, *pAction, *target))
    {
        return result;
    }
    ApplyProbePaidEffects(rUnit, *pAction, *target, rGameState, m_rRng);

    // Decided here from session history rather than trusted from the caller. Recorded before
    // the roll so an attempt counts whether or not it succeeds — a failed probe still tips the
    // base off.
    bool bRepeatAtBase = false;
    if (const BaseManager* pTargetBase = AsBase(*target))
    {
        bRepeatAtBase = NoteAndCheckRepeatAtBase_(rUnit.GetFaction().GetFactionId(),
                                                  pTargetBase->GetBaseId());
    }

    const int risk = MissionRisk(*pAction, *target, facilityId, bRepeatAtBase);
    result.chances = ProbeChancesFor(rUnit, *target, risk);
    const ProbeRollResult_t roll = RollProbeAction(result.chances, m_rRng);
    return ResolveProbeRoll_(rUnit, *pAction, *target, rGameState, facilityId, result, roll);
}

bool ProbeActionExecutor::NoteAndCheckRepeatAtBase_(FactionId_t actorFactionId,
                                                    BaseId_t targetBaseId)
{
    // insert().second is false when the pair was already present — i.e. this is a repeat.
    return !m_probedBases.emplace(actorFactionId, targetBaseId).second;
}

std::optional<ProbeTarget_t> ProbeActionExecutor::ResolveEligibleTarget_(
    const Unit& rUnit, const Tile& rTile, const ProbeActionConfig_t& rAction,
    GameState& rGameState) const
{
    const std::optional<ProbeTarget_t> target =
        ResolveProbeTarget(rUnit, rTile, rAction.target, rGameState);
    if (!target.has_value() || !CanTryProbeAction_(rUnit, rAction, *target))
    {
        return std::nullopt;
    }
    return target;
}

bool ProbeActionExecutor::TryPayProbeCost_(Unit& rUnit, const ProbeActionConfig_t& rAction,
                                           const ProbeTarget_t& rTarget)
{
    const std::optional<int> cost =
        QuoteProbeActionCost(rAction, rUnit.GetFaction(), rTarget);
    if (!cost.has_value())
    {
        return false;
    }
    if (*cost <= 0)
    {
        return true;
    }
    EconomyManager& rEconomy = rUnit.GetFaction().GetEconomy();
    if (!rEconomy.CanAfford(*cost))
    {
        return false;
    }
    rEconomy.SpendEnergy(*cost);
    return true;
}

ProbeActionResult_t ProbeActionExecutor::ResolveProbeRoll_(
    Unit& rUnit, const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget,
    GameState& rGameState, const BuildingId_t& facilityId, ProbeActionResult_t result,
    const ProbeRollResult_t& roll)
{
    ApplyHostileAct(rGameState, ProbeHostileAct(rUnit, rAction, rTarget, roll));

    if (!roll.missionSucceeded)
    {
        return FailMission_(rUnit, result);
    }

    if (!ApplyProbeActionEffect(rUnit, rAction, rTarget, rGameState, facilityId, result,
                                m_rRng))
    {
        // Handler failed after paying cost — treat as rejected but keep energy spent.
        result.outcome = ProbeActionOutcome_t::Rejected;
        return result;
    }

    if (!roll.escaped)
    {
        return CaptureProbe_(rUnit, result);
    }

    FinalizeProbeSuccess_(rUnit, result);
    return result;
}

ProbeActionResult_t ProbeActionExecutor::FailMission_(Unit& rUnit,
                                                     ProbeActionResult_t result) const
{
    result.outcome = ProbeActionOutcome_t::MissionFailed;
    rUnit.GetFaction().GetUnitManager().DestroyUnit(rUnit);
    return result;
}

ProbeActionResult_t ProbeActionExecutor::CaptureProbe_(Unit& rUnit,
                                                      ProbeActionResult_t result) const
{
    result.outcome = ProbeActionOutcome_t::Captured;
    rUnit.GetFaction().GetUnitManager().DestroyUnit(rUnit);
    return result;
}

void ProbeActionExecutor::FinalizeProbeSuccess_(Unit& rUnit,
                                                ProbeActionResult_t& rResult) const
{
    TryPromoteProbeMission(rUnit, m_rMorale);
    rUnit.SpendRemainingMoveFragments();
    rUnit.ClearOrder();
    rResult.outcome = ProbeActionOutcome_t::Succeeded;
}

} // namespace ac
