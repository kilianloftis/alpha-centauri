#include "game/faction/FactionEffectsPool.h"

#include "game/faction/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/CurrentGameState.h"
#include "game/DifficultyConfig.h"
#include "game/natives/NativeLifeLevelConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/research/ResearchManager.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/base/BaseMoodEffects.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/research/TechConfigParser.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "lib/config/EnumNames.h"

#include <algorithm>

namespace ac
{

namespace
{

void AppendAll_(std::vector<ActiveEffect_t>& rOut, const std::vector<ActiveEffect_t>& rSource)
{
    rOut.insert(rOut.end(), rSource.begin(), rSource.end());
}

std::vector<const BaseManager*> BasePointers_(const Faction& rFaction)
{
    std::vector<const BaseManager*> bases;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        bases.push_back(&rBase);
    }
    return bases;
}

} // namespace

FactionEffectsPool::FactionEffectsPool(const Faction& rFaction,
                                       const BuildingRegistry& rBuildingRegistry,
                                       const Revision& rBaseListRevision,
                                       const std::vector<EffectConfig_t>& rTileYieldRules,
                                       const SocialRatingRegistry& rSocialRatings,
                                       const std::vector<EffectConfig_t>& rProductionEffects,
                                       const std::vector<EffectConfig_t>& rBaseConquestEffects,
                                       const std::vector<EffectConfig_t>& rPoliceRules,
                                       const std::vector<EffectConfig_t>& rPopCompositionEffects,
                                       const std::vector<EffectConfig_t>& rGrowthEffects,
                                       const DifficultyConfig_t& rDifficulty,
                                       const NativeLifeLevelConfig_t& rNativeLifeLevels,
                                       const std::vector<EffectConfig_t>& rEcoDamageEffects,
                                       const std::vector<EffectConfig_t>& rProbeActionEffects,
                                       const DiplomacyConfig_t& rDiplomacy)
    : m_rFaction(rFaction)
    , m_rBuildingRegistry(rBuildingRegistry)
    , m_rBaseListRevision(rBaseListRevision)
    , m_rTileYieldRules(rTileYieldRules)
    , m_rSocialRatings(rSocialRatings)
    , m_rProductionEffects(rProductionEffects)
    , m_rBaseConquestEffects(rBaseConquestEffects)
    , m_rPoliceRules(rPoliceRules)
    , m_rPopCompositionEffects(rPopCompositionEffects)
    , m_rGrowthEffects(rGrowthEffects)
    , m_rDifficulty(rDifficulty)
    , m_rNativeLifeLevels(rNativeLifeLevels)
    , m_rEcoDamageEffects(rEcoDamageEffects)
    , m_rProbeActionEffects(rProbeActionEffects)
    , m_rDiplomacy(rDiplomacy)
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
        // Faction-lane only: ThisTech (TechLocal) cost modifiers resolve on the research
        // target, not after discovery.
        AppendFactionLaneEffects(pTech->effects, nullptr, pTech->id, result);
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
    const DifficultyLevel_t& rLevel =
        m_rDifficulty.RequireForSession(m_rFaction.GetSettings().GetGameRules().difficultyId);

    // Appended from the level's own storage and then filtered: ActiveEffect_t borrows configs
    // by pointer, so filtering into a local vector first would leave them dangling.
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(rLevel.effects, nullptr, "difficulty", result);
    const bool bPlayerControlled = m_rFaction.IsPlayerControlled();
    std::erase_if(result, [bPlayerControlled](const ActiveEffect_t& rEffect) {
        return !FactionFilterMatchesOwner(*rEffect.config, bPlayerControlled);
    });
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectNativeLifeEffects_() const
{
    const NativeLifeLevel_t& rLevel = m_rNativeLifeLevels.RequireForSession(
        m_rFaction.GetSettings().GetGameRules().nativeLifeLevelId);
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(rLevel.effects, nullptr, "native_life_level", result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectEcoDamageEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rEcoDamageEffects, nullptr, "eco_damage", result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectProbeActionEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rProbeActionEffects, nullptr, "probe_actions", result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectDiplomaticStatusEffects_() const
{
    std::vector<ActiveEffect_t> result;
    const GameState* pState = FindCurrentGameState();
    if (!pState)
    {
        return result;
    }
    const DiplomacyLedger& rLedger = pState->GetDiplomacyLedger();
    const FactionId_t selfId = m_rFaction.GetFactionId();
    for (const Faction& rPartner : pState->Factions())
    {
        const FactionId_t partnerId = rPartner.GetFactionId();
        if (partnerId == selfId)
        {
            continue;
        }
        const DiplomaticStatus_t status = rLedger.GetStatus(selfId, partnerId);
        const std::size_t first = result.size();
        AppendActiveEffects(m_rDiplomacy.For(status).effects, nullptr,
                            "diplomacy_" + EnumToLowerName(status), result);
        for (std::size_t i = first; i < result.size(); ++i)
        {
            result[i].partnerFaction = partnerId;
        }
    }
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectProductionEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rProductionEffects, nullptr, "production", result);
    return result;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectGrowthEffects_() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_rGrowthEffects, nullptr, "pop_growth", result);
    return result;
}

void FactionEffectsPool::ApplyRemovedByTech_(FactionEffects_t& rEffects,
                                             const ResearchManager& rResearch)
{
    std::erase_if(rEffects.effects, [&](const ActiveEffect_t& rEffect) {
        return !rEffect.config->removedByTech.empty()
            && rResearch.HasDiscoveredTech(rEffect.config->removedByTech);
    });
}

void FactionEffectsPool::CollectRevisions_(std::vector<uint64_t>& rOut) const
{
    rOut.clear();
    rOut.push_back(m_rBaseListRevision.Get());
    rOut.push_back(m_rFaction.GetResearch().GetRevision());
    rOut.push_back(m_rFaction.GetSocialEngineering().GetRevision());
    rOut.push_back(m_rFaction.GetUnitManager().GetRevision());
    // Difficulty and native life are changeable mid-campaign; a rules change must invalidate
    // the pool.
    rOut.push_back(m_rFaction.GetSettings().GetGameRulesRevision().Get());
    // Pair effects follow every status change and every faction that joins the session.
    if (const GameState* pState = FindCurrentGameState())
    {
        // A different current game state must not hit this cache.
        rOut.push_back(reinterpret_cast<uintptr_t>(pState));
        rOut.push_back(pState->GetDiplomacyLedger().GetRevision());
        const int factionCount = pState->GetNumFactions();
        rOut.push_back(static_cast<uint64_t>(factionCount));
    }
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
    // Collect, then gate after every expansion (grants, faction-lane ratings), so no derivative
    // outlives the effect that produced it.
    FactionEffects_t factionEffects(m_rFaction);
    factionEffects.effects = CollectSources_();
    const ResearchManager& rResearch = m_rFaction.GetResearch();
    ApplyRemovedByTech_(factionEffects, rResearch);

    factionEffects.effects = ExpandGrantBuildingEffects(
        std::move(factionEffects.effects), m_rBuildingRegistry, BasePointers_(m_rFaction));
    // Before anything reads the grants: a granted building's effects can be removed by tech, and
    // the rating effects they would produce carry no gate of their own.
    ApplyRemovedByTech_(factionEffects, rResearch);

    // Expand SE rating axes whose gameplay effects target FactionUnits (e.g. Morale →
    // morale_rank). Accumulation is FactionWide-only; ThisBase stays on the per-base path.
    ExpandFactionLaneSocialRatingEffects(factionEffects, m_rSocialRatings);
    // Rating level effects are the last derivatives; gate them too.
    ApplyRemovedByTech_(factionEffects, rResearch);

    m_cachedPool = std::move(factionEffects);
    m_cachedStamp = m_scratchRevisions;
    ++m_version;
}

std::vector<ActiveEffect_t> FactionEffectsPool::CollectSources_() const
{
    std::vector<ActiveEffect_t> effects;
    AppendAll_(effects, CollectTileYieldRuleEffects_());
    AppendAll_(effects, CollectBaseConquestEffects_());
    AppendAll_(effects, CollectPoliceRulesEffects_());
    AppendAll_(effects, CollectPopCompositionEffects_());
    AppendAll_(effects, CollectDifficultyEffects_());
    AppendAll_(effects, CollectNativeLifeEffects_());
    AppendAll_(effects, CollectEcoDamageEffects_());
    AppendAll_(effects, CollectProbeActionEffects_());
    AppendAll_(effects, CollectProductionEffects_());
    AppendAll_(effects, CollectGrowthEffects_());
    AppendAll_(effects, CollectDiplomaticStatusEffects_());
    AppendAll_(effects, CollectDefinitionEffects_());
    AppendAll_(effects, CollectDiscoveredTechEffects_());
    AppendAll_(effects, CollectBuildingEffects_());
    AppendAll_(effects, m_rFaction.GetSocialEngineering().CollectEffects());
    AppendAll_(effects, CollectPopEffects_());
    AppendAll_(effects, CollectMoodEffects_());
    AppendAll_(effects, CollectUnitEffects_());
    return effects;
}

} // namespace ac
