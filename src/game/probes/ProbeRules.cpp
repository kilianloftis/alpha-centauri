#include "game/probes/ProbeRules.h"

#include "game/CurrentGameState.h"
#include "game/research/TechRequirement.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/economy/EconomyManager.h"
#include "game/research/ResearchManager.h"
#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/map/MapUtils.h"
#include "game/probes/MindControlLedger.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticStatus.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/population/AssimilationTracker.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/combat/MoraleCalculator.h"
#include "game/combat/MoraleRules.h"
#include "game/research/ResearchRules.h"
#include "game/research/TechConfigParser.h"
#include "game/research/TechRegistry.h"
#include "game/buildings/BuildingRules.h"
#include "game/buildings/FacilityExclusions.h"
#include "game/buildings/BuildingManager.h"
#include "game/production/ProductionManager.h"
#include "game/units/UnitDomain.h"

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

double ResolveMindControlDistanceScale_(const BaseManager& rBase)
{
    return ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::MindControlDistanceScale,
                           SeedFor(StatId_t::MindControlDistanceScale), nullptr);
}

// An Additive target-side stat (hunter, bust chance) from the target's effect source; 0 without one.
int ResolveTargetAdditive_(const ProbeTarget_t& rTarget, StatId_t stat)
{
    const BaseManager* pSource = EffectSourceBase(rTarget);
    if (!pSource)
    {
        return 0;
    }
    const double value = ResolveBaseStat(pSource->GetBaseEffects(), stat, SeedFor(stat), nullptr);
    return static_cast<int>(std::lround(value));
}

// The target's hunter neutralises the probe's intrusion bonuses when it matches or beats them.
bool HunterNeutralises_(const Unit& rProbe, const ProbeTarget_t& rTarget)
{
    const int hunter = ResolveTargetAdditive_(rTarget, StatId_t::ProbeHunter);
    return hunter > 0 && hunter >= ResolveStat(rProbe, StatId_t::ProbeIntrusion);
}

struct IntrusionBonuses_t
{
    double failureScale = 1.0;
    int failureSaveChance = 0;
};

// The probe's failure scale and save chance, void while the target's hunter neutralises it.
IntrusionBonuses_t IntrusionBonuses_(const Unit& rProbe, const ProbeTarget_t& rTarget)
{
    IntrusionBonuses_t bonuses;
    if (HunterNeutralises_(rProbe, rTarget))
    {
        return bonuses;
    }
    bonuses.failureScale = ResolveMultiplicativeStat(
        rProbe, StatId_t::ProbeFailureScale, SeedFor(StatId_t::ProbeFailureScale),
        EffectContext_t{});
    bonuses.failureSaveChance = ResolveStat(rProbe, StatId_t::ProbeFailureSave);
    return bonuses;
}

double ResolveSubversionDistanceScale_(const Unit& rTargetUnit)
{
    const BaseManager* pHome = rTargetUnit.GetHomeBase();
    if (!pHome)
    {
        return 1.0;
    }
    return ResolveBaseStat(pHome->GetBaseEffects(), StatId_t::SubversionDistanceScale,
                           SeedFor(StatId_t::SubversionDistanceScale), nullptr);
}

// The binary's AI-vs-human discount (cost * 3 / diff_level): only an AI actor pays the target's
// scale, and difficulty emits it on player factions only.
double AiActorCostScale_(const Faction& rActor, const BaseManager& rSource)
{
    if (rActor.IsPlayerControlled())
    {
        return 1.0;
    }
    return ResolveBaseStat(rSource.GetBaseEffects(), StatId_t::AiProbeCostScale,
                           SeedFor(StatId_t::AiProbeCostScale), nullptr);
}

double ResolveActorProbeActionCostMultiplier_(const Faction& rActor)
{
    if (const BaseManager* pHq = rActor.GetHeadquarters())
    {
        return ResolveProbeActionCostMultiplier_(*pHq);
    }
    for (const BaseManager& rBase : rActor.Bases())
    {
        return ResolveProbeActionCostMultiplier_(rBase);
    }
    return 1.0;
}

int ApplyDiplomaticCostMods_(int cost, const Faction& rActor, const ProbeTarget_t& rTarget)
{
    const GameState& rGame = CurrentGameState();
    if (rGame.GetDiplomacyLedger().GetStatus(rActor.GetFactionId(),
                                            rTarget.rFaction.GetFactionId())
        == DiplomaticStatus_t::Pact)
    {
        cost *= 2;
    }
    if (const BaseManager* pBase = AsBase(rTarget))
    {
        const AssimilationState& rWindow =
            pBase->GetPopulation().GetAssimilation().OccupierWindow();
        if (rWindow.IsActive() && rWindow.formerOwner == rActor.GetFactionId())
        {
            cost /= 2;
        }
    }
    if (rGame.GetAtrocityLedger().HasVictimized(rActor.GetFactionId(),
                                               rTarget.rFaction.GetFactionId()))
    {
        cost *= 2;
    }
    return std::max(1, cost);
}

// Every unit sharing the tile counts, allies included. The plan decides each unit's weight
// (probe_actions.json); Poly units raise one multiplier for the whole stack.
int MindControlGarrison_(const Tile& rTile, const WorldMap& rMap, const ProbeCostConfig_t& rCost)
{
    int weight = 0;
    int multiplier = rCost.garrisonMultiplierBase;
    for (const Unit* pUnit : rMap.GetAllUnitsOnTile(rTile))
    {
        weight += ResolveStat(*pUnit, StatId_t::GarrisonProbeWeight);
        multiplier += ResolveStat(*pUnit, StatId_t::GarrisonProbeMultiplier);
    }
    return weight * multiplier;
}

// The binary's vector_dist, which TabletopDiagonalDistance equals on every offset.
int DistanceToHeadquarters_(const Faction& rFaction, const Tile& rTile, int mapWidth)
{
    const BaseManager* pHq = rFaction.GetHeadquarters();
    if (!pHq)
    {
        return k_defaultHqDistance;
    }
    return TabletopDiagonalDistance(rTile, pHq->GetTile(), mapWidth);
}

// Truncated toward zero, which equals the binary's sequential integer x2 and /2 steps.
int ScaleDistance_(int distance, double scale)
{
    const double scaled = static_cast<double>(distance) * scale;
    return static_cast<int>(scaled);
}

int ApplyProbeCostMultiplier_(int rawCost, double multiplier)
{
    const int scaled = static_cast<int>(std::lround(rawCost * multiplier));
    return std::max(1, scaled);
}

// (garrison + actorMindControlTotal / divisor + pop) * ((energy + bias) / (dist + bias)), then
// the cost multiplier.
int QuoteMindControlBaseCost_(const ProbeCostConfig_t& rCost, int garrison,
                              int actorMindControlTotal, int population, int energy,
                              int distToHq, double costMultiplier)
{
    const int cost = (garrison + actorMindControlTotal / rCost.mindControlDivisor + population)
                     * ((energy + rCost.energyBias) / (distToHq + rCost.distBias));
    return ApplyProbeCostMultiplier_(cost, costMultiplier);
}

// mineralCost * ((energy + bias) / (dist + bias)), dividing first as the binary does, then the
// cost multiplier.
int QuoteSubvertUnitCost_(const ProbeCostConfig_t& rCost, int mineralCost, int energy,
                          int distToHq, double costMultiplier)
{
    const int cost = mineralCost * ((energy + rCost.energyBias) / (distToHq + rCost.distBias));
    return ApplyProbeCostMultiplier_(cost, costMultiplier);
}

std::optional<int> QuoteBaseActionCost_(const ProbeCostConfig_t& rCost, int actorMindControlTotal,
                                        const BaseManager& rBase,
                                        const Faction& rTargetFaction, const WorldMap& rMap,
                                        double costMultiplier)
{
    const int rawDistance =
        DistanceToHeadquarters_(rTargetFaction, rBase.GetTile(), rMap.GetWidth());
    // The HQ tile itself, before any facility halves the distance.
    if (rawDistance <= 0)
    {
        return std::nullopt;
    }
    const int garrison = MindControlGarrison_(rBase.GetTile(), rMap, rCost);
    const double distScale = ResolveMindControlDistanceScale_(rBase);
    const int distance = ScaleDistance_(rawDistance, distScale);
    return QuoteMindControlBaseCost_(rCost, garrison, actorMindControlTotal,
                                     rBase.GetPopulation().GetSize(),
                                     rTargetFaction.GetEconomy().GetEnergy(), distance,
                                     costMultiplier);
}

int QuoteUnitActionCost_(const ProbeCostConfig_t& rCost, const Unit& rTargetUnit,
                         const Faction& rTargetFaction, const WorldMap& rMap,
                         double costMultiplier)
{
    const int mineralCost = std::max(1, rTargetUnit.GetDesign().GetBaseCost());
    const int rawDistance =
        DistanceToHeadquarters_(rTargetFaction, rTargetUnit.GetTile(), rMap.GetWidth());
    const double distScale = ResolveSubversionDistanceScale_(rTargetUnit);
    const int distance = ScaleDistance_(rawDistance, distScale);
    const double unitScale =
        ResolveMultiplicativeStat(rTargetUnit, StatId_t::SubversionCostScale,
                                  SeedFor(StatId_t::SubversionCostScale), EffectContext_t{});
    return QuoteSubvertUnitCost_(rCost, mineralCost, rTargetFaction.GetEconomy().GetEnergy(),
                                 distance, costMultiplier * unitScale);
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

bool ConfigHasFlag_(const std::vector<EffectConfig_t>& rEffects, RuleFlagId_t flag)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        const auto* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.effect);
        if (pFlag && pFlag->flag == flag)
        {
            return true;
        }
    }
    return false;
}

bool SabotageFacilityOffered_(const BaseManager& rBase)
{
    FacilityExclusions_t exclusions;
    exclusions.excludeHq = true;
    exclusions.excludeSecretProjects = true;
    if (!CollectDestroyableFacilities(rBase, exclusions).empty())
    {
        return true;
    }
    return rBase.GetProduction().GetCurrentProduction() != nullptr;
}

bool ProbeCanOccupyBase_(const Unit& rProbe, const BaseManager& rBase)
{
    switch (rProbe.GetDomain())
    {
        case UnitDomain_t::Sea:
            return rBase.MayOccupyWater();
        case UnitDomain_t::Land:
            return !rBase.GetTile().IsWater();
        case UnitDomain_t::Air:
        case UnitDomain_t::Orbital:
            return true;
    }
    return false;
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
    chances.escapeStrength = std::max(1, morale - targetProbeEffect);
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

    int lossRate = ((risk + 1) * 100) / chances.escapeStrength;
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
    return true;
}

bool ProbeDeclaresHostileAct(const ProbeActionConfig_t& rAction)
{
    return rAction.id != ProbeActionId_t::Infiltrate
        && rAction.id != ProbeActionId_t::TotalThoughtControl;
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

int GeneticPlagueKilled(const BaseManager& rBase)
{
    const int size = rBase.GetPopulation().GetSize();
    const int cap = std::max(0, size - 1);
    if (cap == 0)
    {
        return 0;
    }

    int defenseTechs = 0;
    int known = 0;
    const TechRegistry* pTechs = CurrentGameState().GetGameData().techRegistry.get();
    if (pTechs)
    {
        const ResearchManager& rResearch = rBase.GetFaction().GetResearch();
        for (const TechConfig_t& rTech : pTechs->GetAll())
        {
            if (!ConfigHasFlag_(rTech.effects, RuleFlagId_t::IncGeneWarfareDefense))
            {
                continue;
            }
            ++defenseTechs;
            if (rResearch.HasDiscoveredTech(rTech.id))
            {
                ++known;
            }
        }
    }
    const int defenseCap = std::max(1, defenseTechs);
    const int knownClamped = std::clamp(known, 1, defenseCap);
    int killed = size * (defenseCap - knownClamped) / defenseCap;
    killed += 1;
    if (rBase.HasPlagueIntroduced())
    {
        killed /= 2;
    }
    const double scale = ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::PlagueDamageScale,
                                         SeedFor(StatId_t::PlagueDamageScale), nullptr);
    killed = static_cast<int>(std::lround(static_cast<double>(killed) * scale));
    return std::clamp(killed, 0, cap);
}

const BaseManager* ProbeReturnBase(const Unit& rProbe)
{
    const WorldMap& rMap = CurrentGameState().GetWorldMap();
    const BaseManager* pBest = nullptr;
    int bestDist = 0;
    for (const BaseManager& rBase : rProbe.GetFaction().Bases())
    {
        if (!ProbeCanOccupyBase_(rProbe, rBase))
        {
            continue;
        }
        const int dist = ChebyshevDistance(rProbe.GetTile(), rBase.GetTile(), rMap.GetWidth());
        if (!pBest || dist < bestDist)
        {
            pBest = &rBase;
            bestDist = dist;
        }
    }
    return pBest;
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

bool ProbeContactDestroys(const Unit& rProbe, const ProbeTarget_t& rTarget)
{
    const int hunter = ResolveTargetAdditive_(rTarget, StatId_t::ProbeHunter);
    return hunter > ResolveStat(rProbe, StatId_t::ProbeIntrusion);
}

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

    const BaseManager* pBase = AsBase(rTarget);
    if (pBase && !HqAllowsAction_(rAction, *pBase))
    {
        return false;
    }

    if (rAction.cost.has_value()
        && TargetHasFlag(rTarget, RuleFlagId_t::ProbeSubversionImmune))
    {
        return false;
    }

    if (rAction.id == ProbeActionId_t::SabotageFacility
        && (!pBase || !SabotageFacilityOffered_(*pBase)))
    {
        return false;
    }

    // A paid action whose cost cannot be quoted is not offered: a base on the HQ tile.
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
        costMultiplier *= AiActorCostScale_(rActor, *pSource);
    }
    costMultiplier *= ResolveActorProbeActionCostMultiplier_(rActor);

    const GameState& rGame = CurrentGameState();
    const WorldMap& rMap = rGame.GetWorldMap();
    const MindControlLedger& rMindControl = rGame.GetMindControlLedger();
    const std::optional<int> quoted = std::visit(
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
    if (!quoted.has_value())
    {
        return std::nullopt;
    }
    return ApplyDiplomaticCostMods_(*quoted, rActor, rTarget);
}

int MissionRisk(const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget,
                const std::string& facilityId)
{
    int risk = rAction.risk;
    if (rAction.id == ProbeActionId_t::StealTech && rAction.riskRepeat.has_value())
    {
        const BaseManager* pBase = AsBase(rTarget);
        if (pBase && pBase->HasStolenResearch())
        {
            risk = *rAction.riskRepeat;
        }
    }
    if (rAction.id == ProbeActionId_t::SabotageFacility && !facilityId.empty())
    {
        const BaseManager* pBase = AsBase(rTarget);
        if (pBase)
        {
            if (IsHeadquarters(*pBase))
            {
                risk = std::max(risk, 2);
            }
            const BuildingConfig_t* pFacility = pBase->GetBuildingManager().FindBuilding(facilityId);
            if (pFacility && ConfigHasFlag_(pFacility->effects, RuleFlagId_t::HighRiskSabotage))
            {
                risk = std::max(risk, 2);
            }
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
    const IntrusionBonuses_t bonuses = IntrusionBonuses_(rProbe, rTarget);
    const double successScale = ResolveTargetSuccessScale(rTarget);
    ProbeChance_t chances = ComputeProbeChances(morale, risk, targetProbeEffect, rFormula,
                                                bonuses.failureScale, successScale);
    chances.failureSaveChance = bonuses.failureSaveChance;
    chances.successBustChance = ResolveTargetAdditive_(rTarget, StatId_t::ProbeSuccessBust);
    return chances;
}

StealEnergyQuote_t QuoteStealEnergy(const BaseManager& rBase, const Faction& rTarget, int morale)
{
    StealEnergyQuote_t quote;
    quote.treasury = rTarget.GetEconomy().GetEnergy();
    if (quote.treasury <= 0)
    {
        return quote;
    }
    const int pop = rBase.GetPopulation().GetSize();
    const int factionPop = rTarget.TotalPopulation();
    const int share = quote.treasury * pop / (factionPop + 1);
    quote.floor = share * std::max(1, morale) / 6;
    quote.remainderBound = std::max(0, share - quote.floor);
    return quote;
}

int StealEnergyAmount(const StealEnergyQuote_t& rQuote, int remainderDraw)
{
    const int remainder = std::max(0, remainderDraw) / 2;
    int stolen = std::clamp(rQuote.floor + remainder, 0, rQuote.treasury);
    if (stolen == 1)
    {
        stolen = 0;
    }
    return stolen;
}

std::vector<std::string> StealableTechs(const Faction& rActor, const Faction& rTarget)
{
    std::vector<std::string> techs;
    for (const TechId& rTechId : rTarget.GetResearch().GetDiscoveredTechs())
    {
        if (!rActor.GetResearch().HasDiscoveredTech(rTechId))
        {
            techs.push_back(rTechId);
        }
    }
    return techs;
}

} // namespace ac
