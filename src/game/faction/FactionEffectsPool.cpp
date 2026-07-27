#include "game/faction/FactionEffectsPool.h"

#include "game/Faction.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"

#include <algorithm>

namespace ac
{

FactionEffectsPool::FactionEffectsPool(const BuildingRegistry* pBuildingRegistry,
                                       const Revision& rBaseListRevision,
                                       const std::vector<EffectConfig_t>* pTileYieldRules,
                                       const SocialRatingRegistry* pSocialRatings)
    : m_pBuildingRegistry(pBuildingRegistry)
    , m_rBaseListRevision(rBaseListRevision)
    , m_pTileYieldRules(pTileYieldRules)
    , m_pSocialRatings(pSocialRatings)
{
}

const FactionEffects_t& FactionEffectsPool::Get(const Faction& rFaction) const
{
    Validate_(rFaction);
    return m_cachedPool;
}

uint64_t FactionEffectsPool::GetVersion(const Faction& rFaction) const
{
    Validate_(rFaction);
    return m_version;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectBuildingEffects_(const Faction& rFaction) const
{
    std::vector<ActiveEffect_t> result;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        const auto baseEffects = rBase.CollectBuildingEffects();
        result.insert(result.end(), baseEffects.begin(), baseEffects.end());
    }

    if (!m_pBuildingRegistry)
    {
        return result;
    }

    std::vector<const BaseManager*> bases;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        bases.push_back(&rBase);
    }
    return ExpandGrantBuildingEffects(std::move(result), *m_pBuildingRegistry, bases);
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectPopEffects_(const Faction& rFaction) const
{
    std::vector<ActiveEffect_t> result;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        for (const Pop& rPop : rBase.GetPopulation().Pops())
        {
            // ThisPop is resolved by the pop itself; ThisBase merges per base via
            // CollectFromPops. Only faction-lane scopes enter the pool.
            AppendFactionLaneEffects(rPop.GetConfig().effects, rPop.GetConfig().id, result);
        }
    }
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectUnitEffects_(const Faction& rFaction) const
{
    std::vector<ActiveEffect_t> result;
    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        for (const ActiveEffect_t& rEffect : rUnit.GetDesign().CollectEffects())
        {
            // ThisUnit is resolved by the unit itself; ThisTile by the tile resolvers
            // scanning units on the map. Only faction-lane scopes enter the pool.
            if (IsFactionLane(rEffect.config->scope))
            {
                result.push_back(rEffect);
            }
        }
    }
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectDefinitionEffects_(const Faction& rFaction) const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(rFaction.GetDefinition().effects, nullptr,
                        rFaction.GetDefinition().id, result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectTileYieldRuleEffects_() const
{
    std::vector<ActiveEffect_t> result;
    if (!m_pTileYieldRules)
    {
        return result;
    }
    AppendActiveEffects(*m_pTileYieldRules, nullptr, "tile_yield_rules", result);
    return result;
}

void FactionEffectsPool::CollectRevisions_(const Faction& rFaction, std::vector<uint64_t>& rOut) const
{
    rOut.clear();
    rOut.push_back(m_rBaseListRevision.Get());
    rOut.push_back(rFaction.GetResearch().GetRevision());
    rOut.push_back(rFaction.GetSocialEngineering().GetRevision());
    rOut.push_back(rFaction.GetUnitManager().GetRevision());
    for (const BaseManager& rBase : rFaction.Bases())
    {
        rOut.push_back(rBase.GetBuildingManager().GetRevision());
        rOut.push_back(rBase.GetPopulation().GetRevision());
    }
}

void FactionEffectsPool::Validate_(const Faction& rFaction) const
{
    CollectRevisions_(rFaction, m_scratchRevisions);
    if (m_scratchRevisions != m_cachedStamp)
    {
        Rebuild_(rFaction);
    }
}

void FactionEffectsPool::Rebuild_(const Faction& rFaction) const
{
    FactionEffects_t factionEffects;
    const std::vector<ActiveEffect_t> tileYieldRules = CollectTileYieldRuleEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), tileYieldRules.begin(),
                                  tileYieldRules.end());

    const std::vector<ActiveEffect_t> defEffects = CollectDefinitionEffects_(rFaction);
    factionEffects.effects.insert(factionEffects.effects.end(), defEffects.begin(), defEffects.end());

    const std::vector<ActiveEffect_t> buildingEffects = CollectBuildingEffects_(rFaction);
    factionEffects.effects.insert(factionEffects.effects.end(), buildingEffects.begin(), buildingEffects.end());

    const std::vector<ActiveEffect_t> seEffects = rFaction.GetSocialEngineering().CollectEffects();
    factionEffects.effects.insert(factionEffects.effects.end(), seEffects.begin(), seEffects.end());

    // Expand SE rating axes whose gameplay effects target units / faction-global lanes
    // (e.g. Morale → morale_bonus). Base-lane ratings still expand per base.
    if (m_pSocialRatings)
    {
        ExpandFactionLaneSocialRatingEffects(factionEffects, *m_pSocialRatings);
    }

    const std::vector<ActiveEffect_t> popEffects = CollectPopEffects_(rFaction);
    factionEffects.effects.insert(factionEffects.effects.end(), popEffects.begin(), popEffects.end());

    const std::vector<ActiveEffect_t> unitEffects = CollectUnitEffects_(rFaction);
    factionEffects.effects.insert(factionEffects.effects.end(), unitEffects.begin(), unitEffects.end());

    // Drop effects gated by a discovered tech (removed_by_tech). Research revision is in the
    // stamp, so discovering a tech rebuilds the pool and lifts those gates.
    const ResearchManager& rResearch = rFaction.GetResearch();
    factionEffects.effects.erase(
        std::remove_if(factionEffects.effects.begin(), factionEffects.effects.end(),
                       [&](const ActiveEffect_t& rEffect) {
                           return rEffect.config && !rEffect.config->removedByTech.empty()
                               && rResearch.HasDiscoveredTech(rEffect.config->removedByTech);
                       }),
        factionEffects.effects.end());

    m_cachedPool = std::move(factionEffects);
    CollectRevisions_(rFaction, m_cachedStamp);
    ++m_version;
}

} // namespace ac
