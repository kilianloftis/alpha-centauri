#include "game/faction/FactionEffectsPool.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/DifficultyConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/BaseMoodEffects.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/research/TechConfigParser.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"

#include <algorithm>

namespace ac
{

FactionEffectsPool::FactionEffectsPool(const Faction& rFaction,
                                       const BuildingRegistry& rBuildingRegistry,
                                       const Revision& rBaseListRevision,
                                       const std::vector<EffectConfig_t>& rTileYieldRules,
                                       const SocialRatingRegistry& rSocialRatings,
                                       const std::vector<EffectConfig_t>& rProductionEffects,
                                       const std::vector<EffectConfig_t>& rBaseConquestEffects,
                                       const std::vector<EffectConfig_t>& rPoliceRules,
                                       const std::vector<EffectConfig_t>& rPopCompositionEffects)
    : m_rFaction(rFaction)
    , m_rBuildingRegistry(rBuildingRegistry)
    , m_rBaseListRevision(rBaseListRevision)
    , m_rTileYieldRules(rTileYieldRules)
    , m_rSocialRatings(rSocialRatings)
    , m_rProductionEffects(rProductionEffects)
    , m_rBaseConquestEffects(rBaseConquestEffects)
    , m_rPoliceRules(rPoliceRules)
    , m_rPopCompositionEffects(rPopCompositionEffects)
    , m_cachedPool(rFaction)
{
}

const FactionEffects_t& FactionEffectsPool::Get() const
{
    Validate_();
    return m_cachedPool;
}

uint64_t FactionEffectsPool::GetVersion() const
{
    Validate_();
    return m_version;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectBuildingEffects_() const
{
    std::vector<ActiveEffect_t> result;
    for (const BaseManager& rBase : m_rFaction.Bases())
    {
        const auto baseEffects = rBase.CollectBuildingEffects();
        result.insert(result.end(), baseEffects.begin(), baseEffects.end());
    }
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectPopEffects_() const
{
    std::vector<ActiveEffect_t> result;
    for (const BaseManager& rBase : m_rFaction.Bases())
    {
        for (const Pop& rPop : rBase.GetPopulation().Pops())
        {
            // ThisPop is resolved by the pop itself; ThisBase merges per base via
            // CollectFromPops. Only faction-lane scopes enter the pool.
            AppendFactionLaneEffects(rPop.GetConfig().effects, nullptr, rPop.GetConfig().id,
                                     result);
        }
    }
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectMoodEffects_() const
{
    std::vector<ActiveEffect_t> result;
    for (const BaseManager& rBase : m_rFaction.Bases())
    {
        AppendBaseMoodFactionLaneEffects(rBase, result);
    }
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectUnitEffects_() const
{
    std::vector<ActiveEffect_t> result;
    for (const Unit& rUnit : m_rFaction.GetUnitManager().Units())
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

std::vector<ActiveEffect_t> FactionEffectsPool::CollectDefinitionEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rFaction.GetDefinition().effects, nullptr,
                        m_rFaction.GetDefinition().id, result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectDiscoveredTechEffects_() const
{
    std::vector<ActiveEffect_t> result;
    const ResearchManager& rResearch = m_rFaction.GetResearch();
    const TechRegistry& rTechs = rResearch.GetTechRegistry();
    for (const TechId& rTechId : rResearch.GetDiscoveredTechs())
    {
        // Discovered ids may be removed_by_tech gate tokens with no TechConfig entry.
        const TechConfig_t* pTech = rTechs.Find(rTechId);
        if (!pTech)
        {
            continue;
        }
        AppendActiveEffects(pTech->effects, nullptr, pTech->id, result);
    }
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectTileYieldRuleEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rTileYieldRules, nullptr, "tile_yield_rules", result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectBaseConquestEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rBaseConquestEffects, nullptr, "base_conquest", result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectPoliceRulesEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rPoliceRules, nullptr, "police_rules", result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectPopCompositionEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rPopCompositionEffects, nullptr, "pop_composition", result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectDifficultyEffects_() const
{
    const GameDataContext& rData = m_rFaction.GetDataContext();
    if (!rData.difficultyConfig)
    {
        throw std::runtime_error("FactionEffectsPool: GameDataContext has no difficultyConfig");
    }
    const DifficultyLevel_t& rLevel = rData.difficultyConfig->RequireForSession(
        m_rFaction.GetSettings().GetGameRules().difficultyId);

    // Append from the level's own storage, then drop the entries this faction does not
    // match. ActiveEffect_t borrows EffectConfig_t by pointer, so filtering into a local
    // vector first would leave every pointer dangling the moment it went out of scope.
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(rLevel.effects, nullptr, "difficulty", result);
    const bool bPlayerControlled = m_rFaction.IsPlayerControlled();
    std::erase_if(result, [bPlayerControlled](const ActiveEffect_t& rEffect) {
        return !FactionFilterMatchesOwner(*rEffect.config, bPlayerControlled);
    });
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectProductionEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rProductionEffects, nullptr, "production", result);
    return result;
}

void FactionEffectsPool::ApplyRemovedByTech_(FactionEffects_t& rEffects,
                                             const ResearchManager& rResearch)
{
    rEffects.effects.erase(
        std::remove_if(rEffects.effects.begin(), rEffects.effects.end(),
                       [&](const ActiveEffect_t& rEffect) {
                           return !rEffect.config->removedByTech.empty()
                               && rResearch.HasDiscoveredTech(rEffect.config->removedByTech);
                       }),
        rEffects.effects.end());
}

void FactionEffectsPool::CollectRevisions_(std::vector<uint64_t>& rOut) const
{
    rOut.clear();
    rOut.push_back(m_rBaseListRevision.Get());
    rOut.push_back(m_rFaction.GetResearch().GetRevision());
    rOut.push_back(m_rFaction.GetSocialEngineering().GetRevision());
    rOut.push_back(m_rFaction.GetUnitManager().GetRevision());
    // Difficulty is changeable mid-campaign; a rules change must invalidate the pool.
    rOut.push_back(m_rFaction.GetSettings().GetGameRulesRevision().Get());
    for (const BaseManager& rBase : m_rFaction.Bases())
    {
        rOut.push_back(rBase.GetBuildingManager().GetRevision());
        rOut.push_back(rBase.GetPopulation().GetRevision());
        rOut.push_back(rBase.GetPopulation().GetMoodRevision());
    }
}

void FactionEffectsPool::Validate_() const
{
    CollectRevisions_(m_scratchRevisions);
    if (m_scratchRevisions != m_cachedStamp)
    {
        Rebuild_();
    }
}

void FactionEffectsPool::Rebuild_() const
{
    // Pipeline: collect raw → gate → expand grants → gate → expand faction-lane ratings →
    // gate → stamp from the Validate_ scratch snapshot (do not re-collect). Every
    // expansion is bracketed by the gate, so no derivative outlives the effect that
    // produced it.
    FactionEffects_t factionEffects(m_rFaction);

    const std::vector<ActiveEffect_t> tileYieldRules = CollectTileYieldRuleEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), tileYieldRules.begin(),
                                  tileYieldRules.end());

    const std::vector<ActiveEffect_t> conquestEffects = CollectBaseConquestEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), conquestEffects.begin(),
                                  conquestEffects.end());

    const std::vector<ActiveEffect_t> policeEffects = CollectPoliceRulesEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), policeEffects.begin(),
                                  policeEffects.end());

    const std::vector<ActiveEffect_t> compositionEffects = CollectPopCompositionEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), compositionEffects.begin(),
                                  compositionEffects.end());

    const std::vector<ActiveEffect_t> diffEffects = CollectDifficultyEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), diffEffects.begin(),
                                  diffEffects.end());

    const std::vector<ActiveEffect_t> productionEffects = CollectProductionEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), productionEffects.begin(),
                                  productionEffects.end());

    const std::vector<ActiveEffect_t> defEffects = CollectDefinitionEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), defEffects.begin(),
                                  defEffects.end());

    const std::vector<ActiveEffect_t> techEffects = CollectDiscoveredTechEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), techEffects.begin(),
                                  techEffects.end());

    const std::vector<ActiveEffect_t> buildingEffects = CollectBuildingEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), buildingEffects.begin(),
                                  buildingEffects.end());

    const std::vector<ActiveEffect_t> seEffects =
        m_rFaction.GetSocialEngineering().CollectEffects();
    factionEffects.effects.insert(factionEffects.effects.end(), seEffects.begin(),
                                  seEffects.end());

    const std::vector<ActiveEffect_t> popEffects = CollectPopEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), popEffects.begin(),
                                  popEffects.end());

    const std::vector<ActiveEffect_t> moodEffects = CollectMoodEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), moodEffects.begin(),
                                  moodEffects.end());

    const std::vector<ActiveEffect_t> unitEffects = CollectUnitEffects_();
    factionEffects.effects.insert(factionEffects.effects.end(), unitEffects.begin(),
                                  unitEffects.end());

    const ResearchManager& rResearch = m_rFaction.GetResearch();
    ApplyRemovedByTech_(factionEffects, rResearch);

    {
        std::vector<const BaseManager*> bases;
        for (const BaseManager& rBase : m_rFaction.Bases())
        {
            bases.push_back(&rBase);
        }
        factionEffects.effects = ExpandGrantBuildingEffects(
            std::move(factionEffects.effects), m_rBuildingRegistry, bases);
    }

    // Gate the grant derivatives before anything reads them: a granted building's own
    // effects can carry removed_by_tech, and a gated SocialRatingModifier arriving this way
    // must not reach the accumulation below (the level effects it would produce carry no
    // gate of their own, so the final pass could not undo it).
    ApplyRemovedByTech_(factionEffects, rResearch);

    // Expand SE rating axes whose gameplay effects target FactionUnits (e.g. Morale →
    // morale_bonus). Accumulation is FactionWide-only; ThisBase stays on the per-base path.
    ExpandFactionLaneSocialRatingEffects(factionEffects, m_rSocialRatings);

    // Rating level effects are the last derivatives; gate them too.
    ApplyRemovedByTech_(factionEffects, rResearch);

    m_cachedPool = std::move(factionEffects);
    m_cachedStamp = m_scratchRevisions;
    ++m_version;
}

} // namespace ac
