#include "game/probes/ProbeRules.h"

#include "game/CurrentGameState.h"
#include "game/research/TechRequirement.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/economy/EconomyManager.h"
#include "game/research/ResearchManager.h"
#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/map/MapUtils.h"
#include "game/probes/MindControlLedger.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/combat/MoraleCalculator.h"
#include "game/combat/MoraleRules.h"
#include "game/research/ResearchRules.h"
#include "game/research/TechConfigParser.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace ac
{

namespace
{

// Thinker/SMAC fallback when the target faction has no Headquarters.
constexpr int k_defaultHqDistance = 12;

int ResolveProbeDefenseBonus_(const BaseManager& rBase)
{
    const double value = ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::ProbeDefense,
                                         SeedFor(StatId_t::ProbeDefense), nullptr);
    return static_cast<int>(std::lround(value));
}

double ResolveProbeSuccessScale_(const BaseManager& rBase)
{
    return ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::ProbeSuccessScale,
                           SeedFor(StatId_t::ProbeSuccessScale), nullptr);
}

double ResolveProbeActionCostMultiplier_(const BaseManager& rBase)
{
    return ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::ProbeActionCost,
                           SeedFor(StatId_t::ProbeActionCost), nullptr);
}

int CountCombatUnitsOnTile_(const Tile& rTile, const WorldMap& rMap, FactionId_t ownerId)
{
    int count = 0;
    for (Unit* pUnit : rMap.GetUnitPositions().GetUnitsOnTile(rTile))
    {
        if (!pUnit || pUnit->GetFaction().GetFactionId() != ownerId)
        {
            continue;
        }
        if (ResolveStat(*pUnit, StatId_t::Attack) > 0 || ResolveStat(*pUnit, StatId_t::Defense) > 0)
        {
            ++count;
        }
    }
    return count;
}

int DistanceToHeadquarters_(const Faction& rFaction, const Tile& rTile, int mapWidth)
{
    const BaseManager* pHq = rFaction.GetHeadquarters();
    if (!pHq)
    {
        return k_defaultHqDistance;
    }
    return ChebyshevDistance(rTile, pHq->GetTile(), mapWidth);
}

int ApplyProbeCostMultiplier_(int rawCost, double multiplier)
{
    const int scaled = static_cast<int>(std::lround(rawCost * multiplier));
    return std::max(1, scaled);
}

// (garrison + actorMindControlTotal / divisor + pop) * (energy + bias) / (dist + bias), then SE
// multiplier.
std::optional<int> QuoteMindControlBaseCost_(const ProbeCostConfig_t& rCost, int garrison,
                                             int actorMindControlTotal, int population,
                                             int energy, int distToHq, double costMultiplier)
{
    if (distToHq <= 0)
    {
        return std::nullopt;
    }
    int cost = (garrison + actorMindControlTotal / rCost.mindControlDivisor + population)
               * ((energy + rCost.energyBias) / (distToHq + rCost.distBias));
    cost = ApplyProbeCostMultiplier_(cost, costMultiplier);
    return cost;
}

// mineralCost * (energy + bias) / (dist + bias), then SE multiplier.
std::optional<int> QuoteSubvertUnitCost_(const ProbeCostConfig_t& rCost, int mineralCost,
                                         int energy, int distToHq, double costMultiplier)
{
    // 0 is the HQ tile itself, refused as QuoteMindControlBaseCost_ refuses it; the no-HQ case
    // already became k_defaultHqDistance.
    if (distToHq <= 0)
    {
        return std::nullopt;
    }
    const int cost = mineralCost * (energy + rCost.energyBias) / (distToHq + rCost.distBias);
    return ApplyProbeCostMultiplier_(cost, costMultiplier);
}

std::optional<int> QuoteBaseActionCost_(const ProbeCostConfig_t& rCost, int actorMindControlTotal,
                                        const BaseManager& rBase,
                                        const Faction& rTargetFaction, const WorldMap& rMap,
                                        double costMultiplier)
{
    const int garrison =
        CountCombatUnitsOnTile_(rBase.GetTile(), rMap, rTargetFaction.GetFactionId());
    const int distance = DistanceToHeadquarters_(rTargetFaction, rBase.GetTile(), rMap.GetWidth());
    return QuoteMindControlBaseCost_(rCost, garrison, actorMindControlTotal,
                                     rBase.GetPopulation().GetSize(),
                                     rTargetFaction.GetEconomy().GetEnergy(), distance,
                                     costMultiplier);
}

std::optional<int> QuoteUnitActionCost_(const ProbeCostConfig_t& rCost, const Unit& rTargetUnit,
                                        const Faction& rTargetFaction, const WorldMap& rMap,
                                        double costMultiplier)
{
    const int mineralCost = std::max(1, rTargetUnit.GetDesign().GetBaseCost());
    const int distance =
        DistanceToHeadquarters_(rTargetFaction, rTargetUnit.GetTile(), rMap.GetWidth());
    return QuoteSubvertUnitCost_(rCost, mineralCost, rTargetFaction.GetEconomy().GetEnergy(),
                                 distance, costMultiplier);
}

bool ActorMeetsActionPrereqs_(const Unit& rProbe, const ProbeActionConfig_t& rAction)
{
    if (!ResolveFlag(rProbe, RuleFlagId_t::ProbeTeam))
    {
        return false;
    }
    return MeetsTechRequirement(rAction.requiredTech,
                                rProbe.GetFaction().GetResearch().GetDiscoveredTechs());
}

} // namespace

bool TargetHasFlag(const ProbeTarget_t& rTarget, RuleFlagId_t flag)
{
    if (const BaseManager* pSource = EffectSourceBase(rTarget))
    {
        if (ResolveFlag(*pSource, flag))
        {
            return true;
        }
    }
    if (ResolveFlag(rTarget.rFaction, flag))
    {
        return true;
    }
    for (const BaseManager& rBase : rTarget.rFaction.Bases())
    {
        if (ResolveFlag(rBase, flag))
        {
            return true;
        }
    }
    return false;
}

int TargetProbeDefenseEffect(const ProbeTarget_t& rTarget,
                             const ProbeSuccessFormula_t& rFormula)
{
    int effect = 0;
    if (const BaseManager* pSource = EffectSourceBase(rTarget))
    {
        effect = ResolveProbeDefenseBonus_(*pSource);
    }
    return std::clamp(effect, rFormula.defenseClampMin, rFormula.defenseClampMax);
}

double ResolveTargetSuccessScale(const ProbeTarget_t& rTarget)
{
    if (const BaseManager* pSource = EffectSourceBase(rTarget))
    {
        return ResolveProbeSuccessScale_(*pSource);
    }
    return 1.0;
}

ProbeChance_t ComputeProbeChances(int moraleLevel, int risk, int targetProbeEffect,
                                  const ProbeSuccessFormula_t& rFormula,
                                  double failureScale, double successScale)
{
    ProbeChance_t chances;
    chances.risk = risk;
    chances.targetProbeEffect = targetProbeEffect;

    const int morale = std::max(1, moraleLevel);
    if (risk < 0)
    {
        chances.successRate = 100;
        chances.survivalRate = 100;
        chances.probeStrength = morale;
        return chances;
    }

    const int missionStrength =
        (morale / std::max(1, rFormula.moraleDivisor)) - targetProbeEffect
        + rFormula.strengthOffset;
    chances.probeStrength = std::max(1, missionStrength);
    int failureRate = (risk * 100) / chances.probeStrength;
    const double scaledFailure = static_cast<double>(failureRate) * failureScale;
    failureRate = static_cast<int>(std::lround(scaledFailure));
    int successRate = 100 - failureRate;
    const double scaledSuccess = static_cast<double>(successRate) * successScale;
    successRate = static_cast<int>(std::lround(scaledSuccess));

    const int escapeStrength = std::max(1, morale - targetProbeEffect);
    int lossRate = ((risk + 1) * 100) / escapeStrength;
    const double scaledLoss = static_cast<double>(lossRate) * failureScale;
    lossRate = static_cast<int>(std::lround(scaledLoss));
    int survivalRate = 100 - lossRate;
    const double scaledSurvival = static_cast<double>(survivalRate) * successScale;
    survivalRate = static_cast<int>(std::lround(scaledSurvival));

    chances.successRate = std::clamp(successRate, 0, 100);
    chances.survivalRate = std::clamp(survivalRate, 0, 100);
    return chances;
}

bool ProbeDetected(const Unit& /*rProbe*/, const ProbeActionConfig_t& /*rAction*/,
                   const ProbeTarget_t& /*rTarget*/, const ProbeRollResult_t& /*rRoll*/)
{
    return false;
}

HostileAct_t ProbeHostileAct(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                             const ProbeTarget_t& rTarget, const ProbeRollResult_t& rRoll)
{
    const ProbeUnitTarget_t* pUnitTarget = std::get_if<ProbeUnitTarget_t>(&rTarget.ref);
    HostileAct_t act = pUnitTarget
        ? HostileActAgainst(rProbe, pUnitTarget->rUnit)
        : HostileActAgainst(rProbe, rTarget.rFaction.GetFactionId());
    act.bAttributed = act.bAttributed || ProbeDetected(rProbe, rAction, rTarget, rRoll);
    return act;
}

bool IsHeadquarters(const BaseManager& rBase)
{
    return ResolveFlag(rBase, RuleFlagId_t::Headquarters);
}

namespace
{

bool HqAllowsAction_(const ProbeActionConfig_t& rAction, const BaseManager& rBase)
{
    const bool bHq = IsHeadquarters(rBase);
    if (rAction.bHqOnly && !bHq)
    {
        return false;
    }
    // Paid mind-control style actions are never available against an HQ.
    return !bHq || (!rAction.bNotHq && !rAction.cost.has_value());
}

} // namespace

bool CanProbeAction(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                    const ProbeTarget_t& rTarget)
{
    if (!ActorMeetsActionPrereqs_(rProbe, rAction))
    {
        return false;
    }
    if (KindOf(rTarget.ref) != rAction.target)
    {
        return false;
    }

    const bool bIgnoresBlock = ResolveFlag(rProbe, RuleFlagId_t::IgnoresProbeBlock);
    if (!bIgnoresBlock && TargetHasFlag(rTarget, RuleFlagId_t::BlocksProbeTeams))
    {
        return false;
    }

    const BaseManager* pBase = AsBase(rTarget);
    if (pBase && !HqAllowsAction_(rAction, *pBase))
    {
        return false;
    }

    if (rAction.cost.has_value() && !bIgnoresBlock
        && TargetHasFlag(rTarget, RuleFlagId_t::ProbeSubversionImmune))
    {
        return false;
    }

    // TODO: no facility picker exists, so sabotage_facility could only fail; offer it once one
    // does.
    if (rAction.id == ProbeActionId_t::SabotageFacility)
    {
        return false;
    }

    // A paid action whose cost cannot be quoted is not offered, such as subverting a unit on the
    // HQ tile.
    if (rAction.cost.has_value()
        && !QuoteProbeActionCost(rAction, rProbe.GetFaction(), rTarget).has_value())
    {
        return false;
    }
    return true;
}

bool ProbeNoticeReachesPlayer(FactionId_t actorId, FactionId_t targetId)
{
    const FactionId_t playerId = CurrentGameState().GetPlayerFaction().GetFactionId();
    return playerId == actorId || playerId == targetId;
}

std::optional<int> QuoteProbeActionCost(const ProbeActionConfig_t& rAction,
                                        const Faction& rActor, const ProbeTarget_t& rTarget)
{
    if (!rAction.cost.has_value())
    {
        return 0;
    }

    double costMultiplier = 1.0;
    if (const BaseManager* pSource = EffectSourceBase(rTarget))
    {
        costMultiplier = ResolveProbeActionCostMultiplier_(*pSource);
    }

    const GameState& rGame = CurrentGameState();
    const WorldMap& rMap = rGame.GetWorldMap();
    const MindControlLedger& rMindControl = rGame.GetMindControlLedger();
    return std::visit(
        [&](const auto& rConcrete) -> std::optional<int>
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, ProbeBaseTarget_t>)
            {
                const int mindControl = rMindControl.Total(rActor.GetFactionId());
                return QuoteBaseActionCost_(*rAction.cost, mindControl, rConcrete.rBase,
                                            rTarget.rFaction, rMap, costMultiplier);
            }
            else
            {
                return QuoteUnitActionCost_(*rAction.cost, rConcrete.rUnit, rTarget.rFaction,
                                            rMap, costMultiplier);
            }
        },
        rTarget.ref);
}

int MissionRisk(const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget,
                const std::string& facilityId, bool bRepeatAtBase)
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

ProbeChance_t ProbeChancesFor(const Unit& rProbe, const ProbeTarget_t& rTarget, int risk)
{
    const GameDataContext& rData = CurrentGameState().GetGameData();
    const MoraleCalculator& rMorale = *rData.moraleCalculator;
    const ProbeSuccessFormula_t& rFormula = rData.probeActionsConfig->successFormula;
    EffectContext_t ctx;
    const int morale = EffectiveMoraleLevel(rProbe, ctx, rMorale);
    const int targetProbeEffect = TargetProbeDefenseEffect(rTarget, rFormula);
    double failureScale = 1.0;
    if (!TargetHasFlag(rTarget, RuleFlagId_t::BlocksProbeTeams))
    {
        failureScale = ResolveMultiplicativeStat(rProbe, StatId_t::ProbeFailureScale, 1.0,
        EffectContext_t{});
    }
    const double successScale = ResolveTargetSuccessScale(rTarget);
    return ComputeProbeChances(morale, risk, targetProbeEffect, rFormula, failureScale,
                               successScale);
}

int StealEnergyAmount(const BaseManager& rBase, const Faction& rTarget, int morale)
{
    const int energy = rTarget.GetEconomy().GetEnergy();
    if (energy <= 0)
    {
        return 0;
    }
    const int pop = rBase.GetPopulation().GetSize();
    const int factionPop = rTarget.TotalPopulation();
    const int share = energy * pop / (factionPop + 1);
    const int minSteal = std::max(0, (share * std::max(1, morale)) / 10);
    return std::clamp(minSteal + std::max(0, (share - minSteal) / 2), 0, energy);
}

std::vector<std::string> StealableTechs(const Faction& rActor, const Faction& rTarget)
{
    std::vector<std::string> techs;
    for (const TechConfig_t* pTech : AvailableTechs(rActor.GetResearch()))
    {
        if (rTarget.GetResearch().HasDiscoveredTech(pTech->id))
        {
            techs.push_back(pTech->id);
        }
    }
    return techs;
}

} // namespace ac
