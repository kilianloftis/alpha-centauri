#include "game/probes/ProbeActionExecutor.h"

#include "game/CurrentGameState.h"
#include "game/combat/MoraleRules.h"
#include "game/PlayerInteractionQueue.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/probes/ProbeActionMutations.h"
#include "game/probes/ProbeRules.h"
#include "game/research/TechRegistry.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/economy/EconomyManager.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"

#include <algorithm>
#include <optional>
#include <string>
#include <variant>

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

std::string ProbeSubjectName_(const ProbeTarget_t& rTarget)
{
    if (const BaseManager* pBase = AsBase(rTarget))
    {
        return pBase->GetName();
    }
    const Unit& rUnit = std::get<ProbeUnitTarget_t>(rTarget.ref).rUnit;
    return rUnit.GetDesign().GetName();
}

ProbeNoticeFaction_t FactionCopy_(const Faction& rFaction)
{
    const FactionConfig_t& rDef = rFaction.GetDefinition();
    ProbeNoticeFaction_t copy;
    copy.title = rDef.leader.title;
    copy.leader = rDef.leader.name;
    copy.name = rDef.identity.name;
    copy.noun = rDef.identity.noun;
    copy.adjective = rDef.identity.adjective;
    return copy;
}

std::string TechName_(const GameState& rGameState, const TechId& rTechId)
{
    const TechConfig_t* pTech = rGameState.GetGameData().techRegistry->Find(rTechId);
    if (!pTech)
    {
        return {};
    }
    return pTech->name;
}

std::string FacilityName_(const GameState& rGameState, const BuildingId_t& rBuildingId)
{
    const BuildingConfig_t* pBuilding =
        rGameState.GetGameData().buildingRegistry->Find(rBuildingId);
    if (!pBuilding)
    {
        return {};
    }
    return pBuilding->name;
}

void FillNoticeNames_(const GameState& rGameState, ProbeNoticeInteraction_t& rNotice)
{
    if (const auto* pTech = std::get_if<ProbeStolenTech_t>(&rNotice.result.detail))
    {
        rNotice.techName = TechName_(rGameState, pTech->techId);
    }
    if (const auto* pFacility = std::get_if<ProbeDestroyedFacility_t>(&rNotice.result.detail))
    {
        rNotice.facilityName = FacilityName_(rGameState, pFacility->buildingId);
    }
    if (const auto* pContact = std::get_if<ProbeContactDestroyed_t>(&rNotice.result.detail))
    {
        rNotice.facilityName = FacilityName_(rGameState, pContact->hunterSourceId);
    }
}

void AnnounceProbe_(GameState& rGameState, const Faction& rActor, ProbeActionId_t actionId,
                    const ProbeTarget_t& rTarget, const ProbeActionResult_t& rResult)
{
    const FactionId_t actorId = rActor.GetFactionId();
    const FactionId_t targetId = rTarget.rFaction.GetFactionId();
    if (!ProbeNoticeReachesPlayer(actorId, targetId))
    {
        return;
    }
    const Tile& rTile = rTarget.rTile;
    ProbeNoticeInteraction_t notice;
    notice.actorId = actorId;
    notice.targetFactionId = targetId;
    notice.actor = FactionCopy_(rActor);
    notice.target = FactionCopy_(rTarget.rFaction);
    notice.subjectName = ProbeSubjectName_(rTarget);
    notice.cameraTile = TileCoord_t{rTile.GetX(), rTile.GetY()};
    notice.actionId = actionId;
    notice.result = rResult;
    FillNoticeNames_(rGameState, notice);
    EnqueueForPlayer(rGameState, notice);
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
    if (WouldContactDestroyProbe(rProbe, rTile, rGameState))
    {
        return true;
    }
    return !ListAvailableProbeActions(rProbe, rTile, rGameState).empty();
}

bool ProbeActionExecutor::WouldContactDestroyProbe(const Unit& rProbe, const Tile& rTile,
                                                   GameState& rGameState) const
{
    RequireCurrentGameState(rGameState);
    if (rProbe.GetMoveFragmentsRemaining() <= 0)
    {
        return false;
    }
    const WorldMap& rWorldMap = CurrentGameState().GetWorldMap();
    if (!AreOnOrAdjacent_(rProbe.GetTile(), rTile, rWorldMap))
    {
        return false;
    }
    const std::optional<ProbeTarget_t> target = ResolveContactTarget_(rProbe, rTile, rGameState);
    if (!target.has_value())
    {
        return false;
    }
    return ProbeContactDestroys(rProbe, *target);
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

    const std::optional<ProbeTarget_t> contact =
        ResolveContactTarget_(rUnit, rTile, rGameState);
    if (contact.has_value() && WouldContactDestroyProbe(rUnit, rTile, rGameState))
    {
        return DestroyOnProbeContact_(rUnit, *contact, rGameState);
    }

    const std::optional<ProbeTarget_t> target =
        ResolveEligibleTarget_(rUnit, rTile, *pAction, rGameState);
    if (!target.has_value() || !TryPayProbeCost_(rUnit, *pAction, *target))
    {
        return result;
    }
    ApplyProbePaidEffects(rUnit, *pAction, *target, rGameState, m_rRng);

    if (pAction->id == ProbeActionId_t::Infiltrate)
    {
        ApplyProbeActionEffect(rUnit, *pAction, *target, rGameState, facilityId, result, m_rRng);
    }

    const int risk = MissionRisk(*pAction, *target, facilityId);
    result.chances = ProbeChancesFor(rUnit, *target, risk);
    const ProbeRollResult_t roll = RollProbeAction(result.chances, m_rRng);
    return ResolveProbeRoll_(rUnit, *pAction, *target, rGameState, facilityId, result, roll);
}

std::optional<ProbeTarget_t> ProbeActionExecutor::ResolveContactTarget_(
    const Unit& rUnit, const Tile& rTile, GameState& rGameState) const
{
    std::optional<ProbeTarget_t> target =
        ResolveProbeTarget(rUnit, rTile, ProbeTargetKind_t::Base, rGameState);
    if (target.has_value())
    {
        return target;
    }
    return ResolveProbeTarget(rUnit, rTile, ProbeTargetKind_t::Unit, rGameState);
}

ProbeActionResult_t ProbeActionExecutor::DestroyOnProbeContact_(Unit& rUnit,
                                                               const ProbeTarget_t& rTarget,
                                                               GameState& rGameState) const
{
    ProbeActionResult_t result;
    result.outcome = ProbeActionOutcome_t::MissionFailed;
    result.detail = ProbeContactDestroyed_t{ProbeHunterSource(rTarget)};
    Faction& rActor = rUnit.GetFaction();
    rActor.GetUnitManager().DestroyUnit(rUnit);
    AnnounceProbe_(rGameState, rActor, ProbeActionId_t::Infiltrate, rTarget, result);
    return result;
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
    if (ProbeDeclaresHostileAct(rAction))
    {
        ApplyHostileAct(rGameState, ProbeHostileAct(rUnit, rAction, rTarget, roll));
    }

    const Faction& rActor = rUnit.GetFaction();
    if (!roll.missionSucceeded)
    {
        ProbeActionResult_t failed = FailMission_(rUnit, result);
        AnnounceProbe_(rGameState, rActor, rAction.id, rTarget, failed);
        return failed;
    }

    // Infiltrate's on_success_effects already ran before the roll.
    if (rAction.id != ProbeActionId_t::Infiltrate
        && !ApplyProbeActionEffect(rUnit, rAction, rTarget, rGameState, facilityId, result,
                                   m_rRng))
    {
        // Handler failed after paying cost — treat as rejected but keep energy spent.
        result.outcome = ProbeActionOutcome_t::Rejected;
        return result;
    }

    if (!roll.escaped || !ReturnProbeHome_(rUnit, rGameState))
    {
        ProbeActionResult_t captured = CaptureProbe_(rUnit, result);
        AnnounceProbe_(rGameState, rActor, rAction.id, rTarget, captured);
        return captured;
    }

    FinalizeProbeSuccess_(rUnit, result);
    AnnounceProbe_(rGameState, rActor, rAction.id, rTarget, result);
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

bool ProbeActionExecutor::ReturnProbeHome_(Unit& rUnit, GameState& rGameState) const
{
    const BaseManager* pHome = ProbeReturnBase(rUnit);
    if (!pHome)
    {
        return false;
    }
    rGameState.GetWorldMap().GetUnitPositions().MoveUnit(rUnit, pHome->GetTile());
    return true;
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
