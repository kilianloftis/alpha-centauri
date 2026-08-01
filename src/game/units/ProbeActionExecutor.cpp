#include "game/units/ProbeActionExecutor.h"

#include "game/units/ProbeActionEffects.h"
#include "game/units/ProbeRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/Faction.h"
#include "game/GameDataContext.h"
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

ProbeActionExecutor::ProbeActionExecutor(WorldMap& rWorldMap, const MoraleCalculator& rMorale,
                                         std::mt19937& rRng)
    : m_rWorldMap(rWorldMap)
    , m_rMorale(rMorale)
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
    if (!AreOnOrAdjacent_(rUnit.GetTile(), rTarget.rTile, m_rWorldMap))
    {
        return false;
    }
    return CanProbeAction(rUnit, rAction, rTarget);
}

bool ProbeActionExecutor::CanTryProbeAction(const Unit& rUnit,
                                            const ProbeActionConfig_t& rAction,
                                            const Tile& rTile, GameState& rGameState) const
{
    return ResolveEligibleTarget_(rUnit, rTile, rAction, rGameState).has_value();
}

std::vector<std::pair<ProbeActionId_t, std::string>> ProbeActionExecutor::ListAvailableProbeActions(
    const Unit& rProbe, const Tile& rTile, GameState& rGameState,
    const GameDataContext& rDataContext) const
{
    std::vector<std::pair<ProbeActionId_t, std::string>> actions;
    if (!rDataContext.probeActionsConfig)
    {
        return actions;
    }

    for (const ProbeActionConfig_t& rAction : rDataContext.probeActionsConfig->actions)
    {
        if (CanTryProbeAction(rProbe, rAction, rTile, rGameState))
        {
            actions.emplace_back(rAction.id, rAction.name);
        }
    }
    return actions;
}

bool ProbeActionExecutor::CanOpenProbeActions(const Unit& rProbe, const Tile& rTile,
                                              GameState& rGameState,
                                              const GameDataContext& rDataContext) const
{
    return !ListAvailableProbeActions(rProbe, rTile, rGameState, rDataContext).empty();
}

ProbeActionResult_t ProbeActionExecutor::TryProbeAction(
    Unit& rUnit, ProbeActionId_t actionId, const Tile& rTile, GameState& rGameState,
    const GameDataContext& rDataContext, const BuildingId_t& facilityId, bool bRepeatAtBase)
{
    ProbeActionResult_t result;
    if (!rDataContext.probeActionsConfig)
    {
        return result;
    }
    const ProbeActionConfig_t* pAction = rDataContext.probeActionsConfig->Find(actionId);
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

    const int risk = ResolveMissionRisk_(*pAction, *target, facilityId, bRepeatAtBase);
    FillProbeChances_(result, rUnit, *target, rDataContext, risk);
    const ProbeRollResult_t roll = RollProbeAction(result.chances, m_rRng);
    return ResolveProbeRoll_(rUnit, *pAction, *target, rGameState, rDataContext, facilityId,
                             result, roll);
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
    const std::optional<int> cost = QuoteProbeActionCost(rAction, rTarget, m_rWorldMap);
    if (!cost.has_value())
    {
        return false;
    }
    if (*cost <= 0)
    {
        return true;
    }
    if (rUnit.GetFaction().GetEconomy().GetEnergy() < *cost)
    {
        return false;
    }
    rUnit.GetFaction().GetEconomy().AddEnergy(-(*cost));
    return true;
}

int ProbeActionExecutor::ResolveMissionRisk_(const ProbeActionConfig_t& rAction,
                                             const ProbeTarget_t& rTarget,
                                             const BuildingId_t& facilityId,
                                             bool bRepeatAtBase) const
{
    int risk = rAction.risk;
    if (bRepeatAtBase && rAction.riskRepeat.has_value())
    {
        risk = *rAction.riskRepeat;
    }
    if (rAction.id == ProbeActionId_t::SabotageFacility && !facilityId.empty())
    {
        const BaseManager* pBase = AsBase(rTarget);
        if (pBase && IsHeadquarters(*pBase))
        {
            risk = std::max(risk, 2);
        }
    }
    return risk;
}

void ProbeActionExecutor::FillProbeChances_(ProbeActionResult_t& rResult, const Unit& rUnit,
                                            const ProbeTarget_t& rTarget,
                                            const GameDataContext& rDataContext, int risk) const
{
    EffectContext_t ctx;
    const int morale = m_rMorale.EffectiveMoraleLevel(rUnit, ctx);
    const int targetProbeEffect =
        TargetProbeDefenseEffect(rTarget, rDataContext.probeActionsConfig->successFormula);

    // Thinker: failure-scale (Algo) only when the target does not BlocksProbeTeams; success
    // scale (HSA) always applies against that target.
    double failureScale = 1.0;
    if (!TargetHasFlag(rTarget, RuleFlagId_t::BlocksProbeTeams))
    {
        failureScale = ResolveMultiplicativeStat(rUnit, StatId_t::ProbeFailureScale, 1.0);
    }
    const double successScale = ResolveTargetSuccessScale(rTarget);

    rResult.chances =
        ComputeProbeChances(morale, risk, targetProbeEffect,
                            rDataContext.probeActionsConfig->successFormula, failureScale,
                            successScale);
}

ProbeActionResult_t ProbeActionExecutor::ResolveProbeRoll_(
    Unit& rUnit, const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget,
    GameState& rGameState, const GameDataContext& rDataContext, const BuildingId_t& facilityId,
    ProbeActionResult_t result, const ProbeRollResult_t& roll)
{
    if (!roll.missionSucceeded)
    {
        return FailMission_(rUnit, result);
    }

    if (!ApplyProbeActionEffect(rUnit, rAction, rTarget, rGameState, rDataContext, facilityId,
                                result, m_rRng))
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
    rUnit.SetMoveFragmentsRemaining(0);
    rUnit.ClearOrder();
    rResult.outcome = ProbeActionOutcome_t::Succeeded;
}

} // namespace ac
