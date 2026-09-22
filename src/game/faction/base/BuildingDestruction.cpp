#include "game/faction/base/BuildingDestruction.h"

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"

#include <algorithm>

namespace ac
{

namespace
{

bool BuildingIsHeadquarters_(const BuildingConfig_t& rBuilding)
{
    for (const EffectConfig_t& rEffect : rBuilding.effects)
    {
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.effect);
        if (pFlag && pFlag->flag == RuleFlagId_t::Headquarters)
        {
            return true;
        }
    }
    return false;
}

} // namespace

void DestroyBuildingAndNotify(BaseManager& rBase, const BuildingConfig_t& rBuilding)
{
    rBase.GetBuildingManager().DestroyBuilding(rBuilding.id);
}

std::vector<const BuildingConfig_t*> CollectDestroyableFacilities(const BaseManager& rBase,
                                                                  bool bExcludeHq,
                                                                  bool bExcludeSecretProjects)
{
    std::vector<const BuildingConfig_t*> candidates;
    for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
    {
        if (!pBuilding)
        {
            continue;
        }
        if (bExcludeSecretProjects && pBuilding->bIsSecretProject)
        {
            continue;
        }
        if (bExcludeHq && BuildingIsHeadquarters_(*pBuilding))
        {
            continue;
        }
        candidates.push_back(pBuilding);
    }
    return candidates;
}

std::vector<BuildingId_t> DestroyRandomFacilities(BaseManager& rBase, int count, bool bExcludeHq,
                                                  bool bExcludeSecretProjects, std::mt19937& rRng)
{
    if (count <= 0)
    {
        return {};
    }

    std::vector<const BuildingConfig_t*> candidates =
        CollectDestroyableFacilities(rBase, bExcludeHq, bExcludeSecretProjects);
    if (candidates.empty())
    {
        return {};
    }

    const int toDestroy = std::min(count, static_cast<int>(candidates.size()));
    std::shuffle(candidates.begin(), candidates.end(), rRng);
    std::vector<BuildingId_t> destroyed;
    destroyed.reserve(static_cast<size_t>(toDestroy));
    for (int i = 0; i < toDestroy; ++i)
    {
        const BuildingConfig_t& rBuilding = *candidates[static_cast<size_t>(i)];
        destroyed.push_back(rBuilding.id);
        DestroyBuildingAndNotify(rBase, rBuilding);
    }
    return destroyed;
}

} // namespace ac
