#include "game/buildings/BuildingUpkeep.h"

#include "game/faction/base/BaseManager.h"

#include <algorithm>
#include <unordered_map>

namespace ac
{

namespace
{

bool FacilityUpkeepEffectApplies_(const ActiveEffect_t& rEffect,
                                  const BuildingConfig_t& rBuilding,
                                  const BaseManager* pOriginBase)
{
    if (!rEffect.config)
    {
        return false;
    }
    if (rEffect.config->condition.has_value())
    {
        return false;
    }
    if (!BuildingFilterSatisfied(*rEffect.config, rBuilding))
    {
        return false;
    }
    if (TagsOriginBase(rEffect.config->scope))
    {
        return pOriginBase != nullptr && rEffect.originBase == pOriginBase;
    }
    return true;
}

// rCtx carries pOriginBase as the base subject so a BaseSize-scaled upkeep modifier resolves
// (FacilityEnergyUpkeep is Base-domain, and this is the stat's only resolve site). Conditional
// effects are still rejected by FacilityUpkeepEffectApplies_ — per-building upkeep has no
// defined conditional rule yet.
std::vector<ActiveEffect_t> MatchingFacilityUpkeepEffects_(
    const BuildingConfig_t& rBuilding,
    const std::vector<ActiveEffect_t>& rEffects,
    const BaseManager* pOriginBase,
    const EffectContext_t& rCtx)
{
    std::vector<ActiveEffect_t> matching;
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        if (!StatModifierMatchesInContext(rEffect, StatId_t::FacilityEnergyUpkeep, rCtx))
        {
            continue;
        }
        if (FacilityUpkeepEffectApplies_(rEffect, rBuilding, pOriginBase))
        {
            matching.push_back(rEffect);
        }
    }
    return matching;
}

} // namespace

int ResolveFacilityEnergyUpkeepPerCopy(const BuildingConfig_t& rBuilding,
                                       std::span<const ActiveEffect_t> rEffects,
                                       const BaseManager* pOriginBase)
{
    const std::vector<ActiveEffect_t> effects(rEffects.begin(), rEffects.end());
    const EffectContext_t ctx{.pBase = pOriginBase};
    const std::vector<ActiveEffect_t> matching =
        MatchingFacilityUpkeepEffects_(rBuilding, effects, pOriginBase, ctx);
    // FacilityEnergyUpkeep is RawScaled: seed with the building's base upkeep.
    const int resolved = FinalizeResolvedStat(
        ResolveStatModifiers(matching, static_cast<double>(rBuilding.upkeep), &ctx).total);
    return std::max(0, resolved);
}

std::vector<BuildingUpkeepLine_t> TallyBuildingUpkeepByType(
    const std::vector<const BuildingConfig_t*>& rBuildings,
    std::span<const ActiveEffect_t> rEffects,
    const BaseManager* pOriginBase)
{
    std::unordered_map<BuildingId_t, BuildingUpkeepLine_t> byId;
    for (const BuildingConfig_t* pBuilding : rBuildings)
    {
        if (!pBuilding)
        {
            continue;
        }
        BuildingUpkeepLine_t& rLine = byId[pBuilding->id];
        rLine.pConfig = pBuilding;
        ++rLine.count;
    }

    const std::vector<ActiveEffect_t> effects(rEffects.begin(), rEffects.end());
    const EffectContext_t ctx{.pBase = pOriginBase};
    std::vector<BuildingUpkeepLine_t> lines;
    lines.reserve(byId.size());
    for (auto& [rUnusedId, rLine] : byId)
    {
        const std::vector<ActiveEffect_t> matching =
            MatchingFacilityUpkeepEffects_(*rLine.pConfig, effects, pOriginBase, ctx);
        const int resolved = FinalizeResolvedStat(
            ResolveStatModifiers(matching, static_cast<double>(rLine.pConfig->upkeep), &ctx).total);
        rLine.upkeepPerCopy = std::max(0, resolved);
        lines.push_back(rLine);
    }
    std::sort(lines.begin(), lines.end(),
              [](const BuildingUpkeepLine_t& a, const BuildingUpkeepLine_t& b)
              {
                  return a.pConfig->id < b.pConfig->id;
              });
    return lines;
}

int SumBuildingUpkeep(const std::vector<BuildingUpkeepLine_t>& rLines)
{
    int total = 0;
    for (const BuildingUpkeepLine_t& rLine : rLines)
    {
        total += rLine.TotalUpkeep();
    }
    return total;
}

} // namespace ac
