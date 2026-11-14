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
#include "game/base/BaseRules.h"
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
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticPermissionRules.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace ac
{

namespace
{

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

const ProbeCalculator& Calculator_()
{
    return *CurrentGameState().GetGameData().probeCalculator;
}

// A target-side probe cost stat on a base target, with the actor as partner.
double ResolveOnTargetBase_(const BaseManager& rBase, StatId_t stat, const Faction& rActor)
{
    EffectContext_t ctx;
    ctx.pBase = &rBase;
    ctx.pPartnerFaction = &rActor;
    return ResolveBaseStat(rBase.GetBaseEffects(), stat, SeedFor(stat), &ctx);
}

// The same Base stat on a unit target, from the unit's own live effects. Resolved by hand, as
// AwayFromHomeDrones resolves a unit stat from a base's list: scope keeps base-only factors out.
double ResolveOnTargetUnit_(const Unit& rUnit, StatId_t stat, const Faction& rActor)
{
    const UnitEffects_t unitEffects = CollectLiveUnitEffects(rUnit);
    EffectContext_t ctx = UnitSubjectContext(&rUnit, EffectContext_t{});
    ctx.pPartnerFaction = &rActor;
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
    const int rawDistance = HeadquartersDistance(rTargetFaction, rBase.GetTile());
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
    const int formula = Calculator_().MindControlCost(rCost, inputs);
    EffectContext_t actorCtx;
    actorCtx.pPartnerFaction = &rTargetFaction;
    actorCtx.pBase = &rBase;
    const double targetScale = ResolveOnTargetBase_(rBase, StatId_t::ProbeActionCost, rActor);
    const double actorScale = ResolveActorCost_(rActor, actorCtx);
    return ApplyProbeCostMultiplier_(formula, targetScale * actorScale);
}

int QuoteUnitActionCost_(const ProbeCostConfig_t& rCost, const Faction& rActor,
                         const Unit& rTargetUnit)
{
    const Faction& rTargetFaction = rTargetUnit.GetFaction();
    const int mineralCost = std::max(1, rTargetUnit.GetDesign().GetBaseCost());
    const int rawDistance = HeadquartersDistance(rTargetFaction, rTargetUnit.GetTile());
    const double distScale =
        ResolveOnTargetUnit_(rTargetUnit, StatId_t::ProbeDistanceScale, rActor);
    SubvertCostInputs_t inputs;
    inputs.mineralCost = mineralCost;
    inputs.energy = rTargetFaction.GetEconomy().GetEnergy();
    inputs.distance = ScaleTruncating_(rawDistance, distScale);
    const int formula = Calculator_().SubvertCost(rCost, inputs);
    EffectContext_t actorCtx;
    actorCtx.pPartnerFaction = &rTargetFaction;
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

int TargetProbeDefenseEffect(const ProbeTarget_t& rTarget,
                             const ProbeSuccessFormula_t& rFormula)
{
    int probeDefense = 0;
    if (const BaseManager* pSource = EffectSourceBase(rTarget))
    {
        probeDefense = ResolveProbeDefenseBonus_(*pSource);
    }
    return Calculator_().ProbeDefense(rFormula, probeDefense);
}

double ResolveTargetSuccessScale(const ProbeTarget_t& rTarget)
{
    if (const BaseManager* pSource = EffectSourceBase(rTarget))
    {
        return ResolveProbeSuccessScale_(*pSource);
    }
    return 1.0;
}

ProbeChance_t ComputeProbeChances(int probeStrength, int escapeStrength, int risk,
                                  double failureScale, double successScale)
{
    ProbeChance_t chances;
    chances.risk = risk;
    chances.probeStrength = probeStrength;
    chances.escapeStrength = escapeStrength;
    if (risk < 0)
    {
        chances.successRate = 100;
        chances.survivalRate = 100;
        return chances;
    }

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

namespace
{

// HostileAct when the action's hostile_act_on_break, else pact_ends from a Pact and a bare
// treaty_on Vendetta from a Truce or Treaty.
BreakOutcome_t BreakOutcomeFor_(DiplomaticStatus_t status, const ProbeActionConfig_t& rAction)
{
    if (rAction.bHostileActOnBreak)
    {
        return BreakOutcome_t::HostileAct;
    }
    if (status == DiplomaticStatus_t::Pact)
    {
        return BreakOutcome_t::PactEnded;
    }
    return BreakOutcome_t::Vendetta;
}

// Factions are never removed, so one with no base left is out of the game.
bool IsFrameCandidate_(const Faction& rFaction, const Faction& rActor, const Faction& rTarget)
{
    if (&rFaction == &rActor || &rFaction == &rTarget)
    {
        return false;
    }
    const DiplomacyLedger& rLedger = CurrentGameState().GetDiplomacyLedger();
    return HasDiplomacy(rFaction) && rFaction.GetBaseCount() > 0
        && rLedger.AreKnown(rActor.GetFactionId(), rFaction.GetFactionId());
}

} // namespace

ProbeAftermath_t ResolveProbeAftermath(const HostileAct_t& rAct, const ProbeActionConfig_t& rAction)
{
    const GameState& rGame = CurrentGameState();
    const DiplomaticStatus_t status =
        rGame.GetDiplomacyLedger().GetStatus(rAct.aggressor, rAct.victim);
    ProbeAftermath_t aftermath;
    if (!rAct.bAttributed || !IsAgreement(status))
    {
        return aftermath;
    }
    // TODO: a surrendered Pact skips the aftermath (terranx-map.md); nobody surrenders yet.
    aftermath.breakOutcome = BreakOutcomeFor_(status, rAction);
    aftermath.bMayKeepAgreement = rAction.bMayKeepAgreement;
    aftermath.keepFrictionDraw = rGame.GetGameData().diplomacyConfig->probeKeepFrictionDraw;
    return aftermath;
}

bool ProbeMayFrame(const Faction& rActor, const Faction& rTarget,
                   const ProbeActionConfig_t& rAction)
{
    // TODO: an AI actor with morale >= 5 frames the human (0x5A2E04); no AI probes yet.
    return rAction.bMayFrame && rActor.IsPlayerControlled() && !rTarget.IsPlayerControlled()
        && HasDiplomacy(rActor) && HasDiplomacy(rTarget);
}

std::vector<FactionId_t> FrameCandidates(const Faction& rActor, const Faction& rTarget)
{
    std::vector<FactionId_t> candidates;
    for (const Faction& rFaction : CurrentGameState().Factions())
    {
        if (IsFrameCandidate_(rFaction, rActor, rTarget))
        {
            candidates.push_back(rFaction.GetFactionId());
        }
    }
    return candidates;
}

std::optional<ProbeFrame_t> ResolveProbeFrame(const HostileAct_t& rAct,
                                              std::optional<FactionId_t> framedId,
                                              const ProbeRollResult_t& rRoll)
{
    // The binary resolves a frame only over an actor-victim agreement (0x5A4B2F); we resolve
    // every frame (config/deviations/README.md).
    if (!framedId.has_value())
    {
        return std::nullopt;
    }
    if (rRoll.missionSucceeded && rRoll.escaped)
    {
        return ProbeFrame_t{.outcome = ProbeFrameOutcome_t::Framed,
                            .framedId = *framedId,
                            .avenger = rAct.victim,
                            .avengedOn = *framedId};
    }
    // TODO: a player framed by an AI gets FRAMEEXCUSE / PACTFRAMEEXCUSE (0x5A4C3E) instead;
    // only the player frames yet. The flop's prb_state stays negative, so keep-or-break follows.
    return ProbeFrame_t{.outcome = ProbeFrameOutcome_t::Flopped,
                        .framedId = *framedId,
                        .avenger = *framedId,
                        .avengedOn = rAct.aggressor};
}

ProbeStanding_t ResolveProbeStanding(const HostileAct_t& rAct, const ProbeActionConfig_t& rAction,
                                     std::optional<FactionId_t> framedId,
                                     const ProbeRollResult_t& rRoll)
{
    ProbeStanding_t standing;
    standing.frame = ResolveProbeFrame(rAct, framedId, rRoll);
    if (!standing.frame.has_value() || standing.frame->outcome == ProbeFrameOutcome_t::Flopped)
    {
        standing.aftermath = ResolveProbeAftermath(rAct, rAction);
    }
    return standing;
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
            if (!ResolveFlag(rTech.effects, RuleFlagId_t::IncGeneWarfareDefense))
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
    PlagueKillInputs_t inputs;
    inputs.population = size;
    inputs.defenseTechs = defenseTechs;
    inputs.knownDefenseTechs = known;
    inputs.bRepeated = rBase.HasPlagueIntroduced();
    const int formula = Calculator_().PlagueKills(*rAction.plague, inputs);
    const double scale = ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::PlagueDamageScale,
                                         SeedFor(StatId_t::PlagueDamageScale), nullptr);
    const double scaled = static_cast<double>(formula) * scale;
    const int killed = static_cast<int>(std::lround(scaled));
    return std::clamp(killed, 0, cap);
}

int PlagueGarrisonDrawBound(const ProbeActionConfig_t& rAction, int hp)
{
    return Calculator_().PlagueGarrisonDamageBound(*rAction.plague, hp);
}

int PlagueGarrisonDamage(const ProbeActionConfig_t& rAction, const BaseManager& rBase, int hp,
                         int draw)
{
    const int formula = Calculator_().PlagueGarrisonDamage(*rAction.plague, hp, draw,
                                                                rBase.HasPlagueIntroduced());
    const double scale = ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::PlagueDamageScale,
                                         SeedFor(StatId_t::PlagueDamageScale), nullptr);
    const double scaled = static_cast<double>(formula) * scale;
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
    const bool bHq = rBase.IsHeadquarters();
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
        && ResolveFlag(rTarget.rFaction, RuleFlagId_t::ProbeSubversionImmune))
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
                return QuoteUnitActionCost_(*rAction.cost, rActor, rConcrete.rUnit);
            }
        },
        rTarget.ref);
}

int MissionRisk(const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget,
                const std::string& facilityId, std::optional<FactionId_t> framedId)
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
            if (pBase->IsHeadquarters())
            {
                risk = std::max(risk, rAction.protectedRisk);
            }
            const BuildingConfig_t* pFacility = pBase->GetBuildingManager().FindBuilding(facilityId);
            if (pFacility && ResolveFlag(pFacility->effects, RuleFlagId_t::HighRiskSabotage))
            {
                risk = std::max(risk, rAction.protectedRisk);
            }
        }
    }
    if (framedId.has_value())
    {
        risk += CurrentGameState().GetGameData().probeActionsConfig->frameRisk;
    }
    return risk;
}

ProbeChance_t ProbeChancesFor(const Unit& rProbe, const ProbeTarget_t& rTarget, int risk)
{
    const GameDataContext& rData = CurrentGameState().GetGameData();
    const MoraleCalculator& rMorale = *rData.moraleCalculator;
    const ProbeCalculator& rCalculator = *rData.probeCalculator;
    const ProbeSuccessFormula_t& rFormula = rData.probeActionsConfig->successFormula;
    EffectContext_t ctx;
    const int morale = EffectiveMoraleLevel(rProbe, ctx, rMorale);
    const int defense = TargetProbeDefenseEffect(rTarget, rFormula);
    const int probeStrength = rCalculator.MissionStrength(rFormula, morale, defense);
    const int escapeStrength = rCalculator.EscapeStrength(rFormula, morale, defense);
    const IntrusionBonuses_t bonuses = IntrusionBonuses_(rProbe, rTarget);
    const double successScale = ResolveTargetSuccessScale(rTarget);
    ProbeChance_t chances = ComputeProbeChances(probeStrength, escapeStrength, risk,
                                                bonuses.failureScale, successScale);
    chances.failureSaveChance = bonuses.failureSaveChance;
    chances.successBustChance = ResolveTargetAdditive_(rTarget, StatId_t::ProbeSuccessBust);
    return chances;
}

StealEnergyQuote_t QuoteStealEnergy(const ProbeActionConfig_t& rAction, const BaseManager& rBase,
                                    const Faction& rTarget, int morale)
{
    StealEnergyQuote_t quote;
    quote.treasury = rTarget.GetEconomy().GetEnergy();
    if (quote.treasury <= 0)
    {
        return quote;
    }
    const ProbeCalculator& rCalculator = Calculator_();
    DrainShareInputs_t inputs;
    inputs.treasury = quote.treasury;
    inputs.population = rBase.GetPopulation().GetSize();
    inputs.factionPopulation = rTarget.TotalPopulation();
    const int share = rCalculator.DrainShare(*rAction.drain, inputs);
    quote.floor = rCalculator.DrainFloor(*rAction.drain, share, morale);
    // The binary's game_randv(share - floor).
    quote.remainderBound = std::max(0, share - quote.floor);
    return quote;
}

int StealEnergyAmount(const ProbeActionConfig_t& rAction, const StealEnergyQuote_t& rQuote,
                      int remainderDraw)
{
    const int amount =
        Calculator_().DrainAmount(*rAction.drain, rQuote.floor, std::max(0, remainderDraw));
    int stolen = std::clamp(amount, 0, rQuote.treasury);
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
