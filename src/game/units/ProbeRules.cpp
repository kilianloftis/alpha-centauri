#include "game/units/ProbeRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "lib/RandomRoll.h"

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
    return static_cast<int>(std::lround(
        ResolveStatModifiers(FilterBaseLevelByStatId(rBase.GetBaseEffects(), StatId_t::ProbeDefense),
                             SeedFor(StatId_t::ProbeDefense))
            .total));
}

double ResolveProbeSuccessScale_(const BaseManager& rBase)
{
    return ResolveStatModifiers(
               FilterBaseLevelByStatId(rBase.GetBaseEffects(), StatId_t::ProbeSuccessScale),
               SeedFor(StatId_t::ProbeSuccessScale))
        .total;
}

double ResolveProbeActionCostMultiplier_(const BaseManager& rBase)
{
    return ResolveStatModifiers(
               FilterBaseLevelByStatId(rBase.GetBaseEffects(), StatId_t::ProbeActionCost),
               SeedFor(StatId_t::ProbeActionCost))
        .total;
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
        if (pUnit->GetStat(StatId_t::Attack) > 0 || pUnit->GetStat(StatId_t::Defense) > 0)
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
    return std::max(1, static_cast<int>(std::lround(rawCost * multiplier)));
}

// (garrison + pop) * (energy + bias) / (dist + bias), then SE multiplier; riot halves.
std::optional<int> QuoteMindControlBaseCost_(const ProbeCostConfig_t& rCost, int garrison,
                                             int population, int energy, int distToHq,
                                             double costMultiplier, bool bRioting)
{
    if (distToHq <= 0)
    {
        return std::nullopt;
    }
    int cost = (garrison + population)
               * ((energy + rCost.energyBias) / (distToHq + rCost.distBias));
    cost = ApplyProbeCostMultiplier_(cost, costMultiplier);
    if (bRioting)
    {
        cost = std::max(1, cost / 2);
    }
    return cost;
}

// mineralCost * (energy + bias) / (dist + bias), then SE multiplier.
std::optional<int> QuoteSubvertUnitCost_(const ProbeCostConfig_t& rCost, int mineralCost,
                                         int energy, int distToHq, double costMultiplier)
{
    // Distance 0 means the target sits on the HQ tile — a real distance, not "no HQ";
    // DistanceToHeadquarters_ already substitutes k_defaultHqDistance for the no-HQ case.
    // Treating 0 as "no HQ" priced an HQ-garrison subversion at denominator 12 + distBias
    // instead of refusing it, making the best-defended tile on the map the cheapest to subvert.
    // Refused, matching QuoteMindControlBaseCost_ on the same input.
    if (distToHq <= 0)
    {
        return std::nullopt;
    }
    const int cost = mineralCost * (energy + rCost.energyBias) / (distToHq + rCost.distBias);
    return ApplyProbeCostMultiplier_(cost, costMultiplier);
}

std::optional<int> QuoteBaseActionCost_(const ProbeCostConfig_t& rCost,
                                        const BaseManager& rBase,
                                        const Faction& rTargetFaction, const WorldMap& rMap,
                                        double costMultiplier)
{
    return QuoteMindControlBaseCost_(
        rCost,
        CountCombatUnitsOnTile_(rBase.GetTile(), rMap, rTargetFaction.GetFactionId()),
        rBase.GetPopulation().GetSize(),
        rTargetFaction.GetEconomy().GetEnergy(),
        DistanceToHeadquarters_(rTargetFaction, rBase.GetTile(), rMap.GetWidth()),
        costMultiplier,
        rBase.GetPopulation().IsRioting());
}

std::optional<int> QuoteUnitActionCost_(const ProbeCostConfig_t& rCost, const Unit& rTargetUnit,
                                        const Faction& rTargetFaction, const WorldMap& rMap,
                                        double costMultiplier)
{
    return QuoteSubvertUnitCost_(
        rCost,
        std::max(1, rTargetUnit.GetDesign().GetBaseCost()),
        rTargetFaction.GetEconomy().GetEnergy(),
        DistanceToHeadquarters_(rTargetFaction, rTargetUnit.GetTile(), rMap.GetWidth()),
        costMultiplier);
}

bool ActorMeetsActionPrereqs_(const Unit& rProbe, const ProbeActionConfig_t& rAction)
{
    if (!rProbe.GetFlag(RuleFlagId_t::ProbeTeam))
    {
        return false;
    }
    if (!rAction.requiredTech.empty()
        && !rProbe.GetFaction().GetResearch().HasDiscoveredTech(rAction.requiredTech))
    {
        return false;
    }
    return true;
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
    failureRate = static_cast<int>(std::lround(failureRate * failureScale));
    int successRate = 100 - failureRate;
    successRate = static_cast<int>(std::lround(successRate * successScale));

    const int escapeStrength = std::max(1, morale - targetProbeEffect);
    int lossRate = ((risk + 1) * 100) / escapeStrength;
    lossRate = static_cast<int>(std::lround(lossRate * failureScale));
    int survivalRate = 100 - lossRate;
    survivalRate = static_cast<int>(std::lround(survivalRate * successScale));

    chances.successRate = std::clamp(successRate, 0, 100);
    chances.survivalRate = std::clamp(survivalRate, 0, 100);
    return chances;
}

ProbeRollResult_t RollProbeAction(const ProbeChance_t& rChances, std::mt19937& rRng)
{
    ProbeRollResult_t result;
    result.chances = rChances;

    if (rChances.risk <= 0 || RollPercent(rChances.successRate, rRng))
    {
        result.missionSucceeded = true;
    }
    else
    {
        return result;
    }

    result.escaped = RollPercent(rChances.survivalRate, rRng);
    return result;
}

bool IsHeadquarters(const BaseManager& rBase)
{
    return ResolveFlag(rBase, RuleFlagId_t::Headquarters);
}

bool CanProbeAction(const Unit& rProbe, const ProbeActionConfig_t& rAction,
                    const ProbeTarget_t& rTarget, const WorldMap& rMap)
{
    if (!ActorMeetsActionPrereqs_(rProbe, rAction))
    {
        return false;
    }
    if (KindOf(rTarget.ref) != rAction.target)
    {
        return false;
    }

    const bool bIgnoresBlock = rProbe.GetFlag(RuleFlagId_t::IgnoresProbeBlock);
    if (!bIgnoresBlock && TargetHasFlag(rTarget, RuleFlagId_t::BlocksProbeTeams))
    {
        return false;
    }

    if (const BaseManager* pBase = AsBase(rTarget))
    {
        if (rAction.bHqOnly && !IsHeadquarters(*pBase))
        {
            return false;
        }
        if (rAction.bNotHq && IsHeadquarters(*pBase))
        {
            return false;
        }
        // Paid mind-control style actions are never available against an HQ.
        if (rAction.cost.has_value() && IsHeadquarters(*pBase))
        {
            return false;
        }
    }

    if (rAction.cost.has_value() && !bIgnoresBlock
        && TargetHasFlag(rTarget, RuleFlagId_t::ProbeSubversionImmune))
    {
        return false;
    }

    // TODO: sabotage_facility needs a facility to target, and nothing can supply one — there is
    // no facility picker in the UI, so every caller passes an empty id. Offering it would risk
    // the probe on a mission that can only fail. Previously an empty id silently fell through
    // to the random-sabotage branch, making this action a duplicate of sabotage_random rather
    // than the targeted one it is configured to be. Re-enable when the picker exists.
    if (rAction.id == ProbeActionId_t::SabotageFacility)
    {
        return false;
    }

    // A paid action whose cost cannot be quoted is not offerable: TryPayProbeCost_ would
    // refuse it and the attempt would silently do nothing. This is what keeps "Subvert Unit"
    // off the menu for a garrison standing on the HQ tile, where the quote is nullopt —
    // previously such a unit was priced as if the HQ were 12 tiles away, making the
    // best-defended tile on the map the cheapest to subvert.
    if (rAction.cost.has_value() && !QuoteProbeActionCost(rAction, rTarget, rMap).has_value())
    {
        return false;
    }
    return true;
}

std::optional<int> QuoteProbeActionCost(const ProbeActionConfig_t& rAction,
                                        const ProbeTarget_t& rTarget,
                                        const WorldMap& rMap)
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

    return std::visit(
        [&](const auto& rConcrete) -> std::optional<int>
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, ProbeBaseTarget_t>)
            {
                return QuoteBaseActionCost_(*rAction.cost, rConcrete.rBase, rTarget.rFaction,
                                            rMap, costMultiplier);
            }
            else
            {
                return QuoteUnitActionCost_(*rAction.cost, rConcrete.rUnit, rTarget.rFaction,
                                            rMap, costMultiplier);
            }
        },
        rTarget.ref);
}

bool TryPromoteProbeMission(Unit& rProbe, const MoraleCalculator& rMorale)
{
    if (!ResolveFlag(rProbe, RuleFlagId_t::ProbeTeam))
    {
        return false;
    }
    const int xp = rProbe.GetXp();
    if (xp >= rMorale.GetConfig().MaxLevel())
    {
        return false;
    }
    rProbe.SetXp(xp + 1);
    return true;
}

} // namespace ac
