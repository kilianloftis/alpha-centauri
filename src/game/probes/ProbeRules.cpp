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
#include "game/probes/ProbeCalculator.h"
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
#include <stdexcept>
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

// A target-side probe cost stat on a base target, with the actor as partner.
double ResolveOnTargetBase_(const BaseManager& rBase, StatId_t stat, const Faction& rActor)
{
    EffectContext_t ctx;
    ctx.pBase = &rBase;
    ctx.pPartner = &rActor;
    return ResolveBaseStat(rBase.GetBaseEffects(), stat, SeedFor(stat), &ctx);
}

// The same Base stat on a unit target, from the unit's own live effects. Resolved by hand, as
// AwayFromHomeDrones resolves a unit stat from a base's list: scope keeps base-only factors out.
double ResolveOnTargetUnit_(const Unit& rUnit, StatId_t stat, const Faction& rActor)
{
    const UnitEffects_t unitEffects = CollectLiveUnitEffects(rUnit);
    EffectContext_t ctx = UnitSubjectContext(&rUnit, EffectContext_t{});
    ctx.pPartner = &rActor;
    return ResolveStatModifiersTotal(FilterByStatIdInContext(unitEffects.effects, stat, ctx),
                                     SeedFor(stat), &ctx);
}

// The actor side: what rActor's effects make it pay toward the target named in rCtx (partner,
// and the base for a base quote, which TargetIsBase reads).
double ResolveActorCost_(const Faction& rActor, const EffectContext_t& rCtx)
{
    return ResolveFactionStat(rActor.GetActiveEffects(), StatId_t::ProbeActorCost,
                              SeedFor(StatId_t::ProbeActorCost), &rCtx);
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

// Every unit sharing the tile counts, allies included. The plan decides each unit's weight
// (probe_actions.json); Poly units raise the cost formula's multiplier for the whole stack.
void SumMindControlGarrison_(const Tile& rTile, const WorldMap& rMap,
                             MindControlCostInputs_t& rInputs)
{
    for (const Unit* pUnit : rMap.GetAllUnitsOnTile(rTile))
    {
        rInputs.garrisonWeight += ResolveStat(*pUnit, StatId_t::GarrisonProbeWeight);
        rInputs.garrisonMultiplier += ResolveStat(*pUnit, StatId_t::GarrisonProbeMultiplier);
    }
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
int ScaleTruncating_(int value, double scale)
{
    const double scaled = static_cast<double>(value) * scale;
    return static_cast<int>(scaled);
}

// The binary's integer steps after the formula become one multiplier, rounded once.
int ApplyProbeCostMultiplier_(int rawCost, double multiplier)
{
    const int scaled = static_cast<int>(std::lround(rawCost * multiplier));
    return std::max(1, scaled);
}

// TODO: SMAC charges x1.5 to a faction that wants revenge (DIPLO_WANT_REVENGE) when the target
// was not an atrocity victim; we keep no revenge record.
std::optional<int> QuoteBaseActionCost_(const ProbeCostConfig_t& rCost, const Faction& rActor,
                                        const BaseManager& rBase, const WorldMap& rMap)
{
    const Faction& rTargetFaction = rBase.GetFaction();
    const int rawDistance =
        DistanceToHeadquarters_(rTargetFaction, rBase.GetTile(), rMap.GetWidth());
    // The HQ tile itself, before any facility halves the distance.
    if (rawDistance <= 0)
    {
        return std::nullopt;
    }
    const double distScale = ResolveOnTargetBase_(rBase, StatId_t::ProbeDistanceScale, rActor);
    const GameState& rGame = CurrentGameState();
    MindControlCostInputs_t inputs;
    SumMindControlGarrison_(rBase.GetTile(), rMap, inputs);
    inputs.mindControlTotal = rGame.GetMindControlLedger().Total(rActor.GetFactionId());
    inputs.population = rBase.GetPopulation().GetSize();
    inputs.energy = rTargetFaction.GetEconomy().GetEnergy();
    inputs.distance = ScaleTruncating_(rawDistance, distScale);
    const int formula = rGame.GetGameData().probeCalculator->MindControlCost(rCost, inputs);
    EffectContext_t actorCtx;
    actorCtx.pPartner = &rTargetFaction;
    actorCtx.pBase = &rBase;
    const double targetScale = ResolveOnTargetBase_(rBase, StatId_t::ProbeActionCost, rActor);
    const double actorScale = ResolveActorCost_(rActor, actorCtx);
    return ApplyProbeCostMultiplier_(formula, targetScale * actorScale);
}

int QuoteUnitActionCost_(const ProbeCostConfig_t& rCost, const Faction& rActor,
                         const Unit& rTargetUnit, const WorldMap& rMap)
{
    const Faction& rTargetFaction = rTargetUnit.GetFaction();
    const int mineralCost = std::max(1, rTargetUnit.GetDesign().GetBaseCost());
    const int rawDistance =
        DistanceToHeadquarters_(rTargetFaction, rTargetUnit.GetTile(), rMap.GetWidth());
    const double distScale =
        ResolveOnTargetUnit_(rTargetUnit, StatId_t::ProbeDistanceScale, rActor);
    SubvertCostInputs_t inputs;
    inputs.mineralCost = mineralCost;
    inputs.energy = rTargetFaction.GetEconomy().GetEnergy();
    inputs.distance = ScaleTruncating_(rawDistance, distScale);
    const ProbeCalculator& rCalculator = *CurrentGameState().GetGameData().probeCalculator;
    const int formula = rCalculator.SubvertCost(rCost, inputs);
    EffectContext_t actorCtx;
    actorCtx.pPartner = &rTargetFaction;
    const double targetScale =
        ResolveOnTargetUnit_(rTargetUnit, StatId_t::ProbeActionCost, rActor);
    const double actorScale = ResolveActorCost_(rActor, actorCtx);
    return ApplyProbeCostMultiplier_(formula, targetScale * actorScale);
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

int GeneticPlagueKilled(const ProbeActionConfig_t& rAction, const BaseManager& rBase)
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
        killed /= rAction.repeatDivisor;
    }
    const double scale = ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::PlagueDamageScale,
                                         SeedFor(StatId_t::PlagueDamageScale), nullptr);
    killed = static_cast<int>(std::lround(static_cast<double>(killed) * scale));
    return std::clamp(killed, 0, cap);
}

int PlagueGarrisonDrawBound(const ProbeActionConfig_t& rAction, int hp)
{
    return std::max(1, hp / rAction.garrisonDamageDivisor);
}

int PlagueGarrisonDamage(const ProbeActionConfig_t& rAction, const BaseManager& rBase, int hp,
                         int draw)
{
    int damage = hp / rAction.garrisonDamageDivisor + draw;
    if (rBase.HasPlagueIntroduced())
    {
        damage /= rAction.repeatDivisor;
    }
    const double scale = ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::PlagueDamageScale,
                                         SeedFor(StatId_t::PlagueDamageScale), nullptr);
    const double scaled = static_cast<double>(damage) * scale;
    return static_cast<int>(std::lround(scaled));
}

bool MissionDrawSucceeds(const ProbeChance_t& rChances, int draw)
{
    return draw >= rChances.risk;
}

bool EscapeDrawSucceeds(const ProbeChance_t& rChances, int draw)
{
    return draw > rChances.risk;
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

std::string ProbeHunterSource(const ProbeTarget_t& rTarget)
{
    const BaseManager* pSource = EffectSourceBase(rTarget);
    if (!pSource)
    {
        throw std::logic_error("ProbeHunterSource: the target has no effect source");
    }
    EffectContext_t ctx;
    ctx.pBase = pSource;
    const StatBreakdown_t breakdown = ResolveStatModifiers(
        FilterBaseLevelByStatId(pSource->GetBaseEffects(), StatId_t::ProbeHunter, &ctx),
        SeedFor(StatId_t::ProbeHunter), &ctx);
    const auto strongest = std::ranges::max_element(
        breakdown.contributions, {}, &StatBreakdown_t::Contribution_t::amount);
    if (strongest == breakdown.contributions.end() || strongest->amount <= 0.0)
    {
        throw std::logic_error("ProbeHunterSource: nothing contributes probe_hunter");
    }
    return strongest->sourceId;
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

    const WorldMap& rMap = CurrentGameState().GetWorldMap();
    return std::visit(
        [&](const auto& rConcrete) -> std::optional<int>
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, ProbeBaseTarget_t>)
            {
                return QuoteBaseActionCost_(*rAction.cost, rActor, rConcrete.rBase, rMap);
            }
            else
            {
                return QuoteUnitActionCost_(*rAction.cost, rActor, rConcrete.rUnit, rMap);
            }
        },
        rTarget.ref);
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
                risk = std::max(risk, rAction.protectedRisk);
            }
            const BuildingConfig_t* pFacility = pBase->GetBuildingManager().FindBuilding(facilityId);
            if (pFacility && ConfigHasFlag_(pFacility->effects, RuleFlagId_t::HighRiskSabotage))
            {
                risk = std::max(risk, rAction.protectedRisk);
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

StealEnergyQuote_t QuoteStealEnergy(const ProbeActionConfig_t& rAction, const BaseManager& rBase,
                                    const Faction& rTarget, int morale)
{
    StealEnergyQuote_t quote;
    quote.remainderDivisor = rAction.remainderDivisor;
    quote.treasury = rTarget.GetEconomy().GetEnergy();
    if (quote.treasury <= 0)
    {
        return quote;
    }
    const int pop = rBase.GetPopulation().GetSize();
    const int factionPop = rTarget.TotalPopulation();
    const int share = quote.treasury * pop / (factionPop + 1);
    quote.floor = share * std::max(1, morale) / rAction.floorMoraleDivisor;
    quote.remainderBound = std::max(0, share - quote.floor);
    return quote;
}

int StealEnergyAmount(const StealEnergyQuote_t& rQuote, int remainderDraw)
{
    const int remainder = std::max(0, remainderDraw) / rQuote.remainderDivisor;
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
