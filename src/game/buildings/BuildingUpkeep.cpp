#include "game/buildings/BuildingUpkeep.h"

#include "game/base/BaseManager.h"

#include <algorithm>
#include <map>
#include <stdexcept>

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

// pOriginBase is the base subject, so BaseSize-scaled upkeep resolves. Conditional effects
// are still rejected: per-building upkeep has no conditional rule yet.
std::vector<ActiveEffect_t> MatchingFacilityUpkeepEffects_(
    const BuildingConfig_t& rBuilding,
    std::span<const ActiveEffect_t> rEffects,
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

// Keyed by an ordered map so the lines come out sorted by id.
std::map<BuildingId_t, BuildingUpkeepLine_t> CountCopiesById_(
    const std::vector<const BuildingConfig_t*>& rBuildings)
{
    std::map<BuildingId_t, BuildingUpkeepLine_t> byId;
    for (const BuildingConfig_t* pBuilding : rBuildings)
    {
        if (!pBuilding)
        {
            throw std::logic_error("CountCopiesById_: null building");
        }
        BuildingUpkeepLine_t& rLine = byId[pBuilding->id];
        rLine.pConfig = pBuilding;
        ++rLine.count;
    }
    return byId;
}

int ResolveFacilityEnergyUpkeepPerCopy_(const BuildingConfig_t& rBuilding,
                                        std::span<const ActiveEffect_t> rEffects,
                                        const BaseManager* pOriginBase)
{
    const EffectContext_t ctx{.pBase = pOriginBase};
    const std::vector<ActiveEffect_t> matching =
        MatchingFacilityUpkeepEffects_(rBuilding, rEffects, pOriginBase, ctx);
    // FacilityEnergyUpkeep is RawScaled: seed with the building's base upkeep.
    const double seed = static_cast<double>(rBuilding.upkeep);
    const int resolved = FinalizeResolvedStat(ResolveStatModifiers(matching, seed, &ctx).total);
    return std::max(0, resolved);
}

} // namespace

std::vector<BuildingUpkeepLine_t> TallyBuildingUpkeepByType(
    const std::vector<const BuildingConfig_t*>& rBuildings,
    std::span<const ActiveEffect_t> rEffects,
    const BaseManager* pOriginBase)
{
    std::map<BuildingId_t, BuildingUpkeepLine_t> byId = CountCopiesById_(rBuildings);
    std::vector<BuildingUpkeepLine_t> lines;
    lines.reserve(byId.size());
    for (auto& [rUnusedId, rLine] : byId)
    {
        rLine.upkeepPerCopy =
            ResolveFacilityEnergyUpkeepPerCopy_(*rLine.pConfig, rEffects, pOriginBase);
        lines.push_back(rLine);
    }
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
