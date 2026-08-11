#include "game/buildings/BuildingUpkeep.h"

#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"

#include <algorithm>
#include <cmath>
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

std::vector<ActiveEffect_t> MatchingFacilityUpkeepEffects_(
    const BuildingConfig_t& rBuilding,
    const std::vector<ActiveEffect_t>& rEffects,
    const BaseManager* pOriginBase)
{
    std::vector<ActiveEffect_t> matching;
    for (const ActiveEffect_t& rEffect : FilterByStatId(rEffects, StatId_t::FacilityEnergyUpkeep))
    {
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
    // FilterByStatId requires a vector; materialize the span when needed.
    const std::vector<ActiveEffect_t> effects(rEffects.begin(), rEffects.end());
    const std::vector<ActiveEffect_t> matching =
        MatchingFacilityUpkeepEffects_(rBuilding, effects, pOriginBase);
    const double multiplier = ResolveStatModifiers(
        matching, SeedFor(StatId_t::FacilityEnergyUpkeep)).total;
    const long rounded = std::lround(static_cast<double>(rBuilding.upkeep) * multiplier);
    return static_cast<int>(std::max(0L, rounded));
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
    std::vector<BuildingUpkeepLine_t> lines;
    lines.reserve(byId.size());
    for (auto& [rUnusedId, rLine] : byId)
    {
        const std::vector<ActiveEffect_t> matching =
            MatchingFacilityUpkeepEffects_(*rLine.pConfig, effects, pOriginBase);
        const double multiplier = ResolveStatModifiers(
            matching, SeedFor(StatId_t::FacilityEnergyUpkeep)).total;
        const long rounded =
            std::lround(static_cast<double>(rLine.pConfig->upkeep) * multiplier);
        rLine.upkeepPerCopy = static_cast<int>(std::max(0L, rounded));
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
