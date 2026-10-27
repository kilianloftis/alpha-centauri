#pragma once

#include "lib/Revision.h"
#include "game/effects/ActiveEffect.h"
#include <cstdint>
#include <vector>

namespace ac
{

class Faction;
class BuildingRegistry;
class ResearchManager;
class SocialRatingRegistry;
struct DifficultyConfig_t;
struct DiplomacyConfig_t;
struct NativeLifeLevelConfig_t;

// The faction's local effect pool, memoized; Faction::GetActiveEffects composes world effects
// on top.
class FactionEffectsPool
{
public:
    // Every dependency comes from GameDataContext, which is always complete.
    FactionEffectsPool(const Faction& rFaction,
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
                       const DiplomacyConfig_t& rDiplomacy);

    // The validated local pool. Valid until the next effect-source mutation on the owner.
    const FactionEffects_t& Get() const;

    // Monotonic pool version; changes iff the pool content changed.
    uint64_t GetVersion() const;

private:
    // Raw continuous effects from constructed buildings across all bases (no grant expand).
    std::vector<ActiveEffect_t> CollectBuildingEffects_() const;

    // Faction-lane effects from pops in the faction's bases; ThisBase and ThisPop resolve locally.
    std::vector<ActiveEffect_t> CollectPopEffects_() const;

    // Faction-lane effects (any scope except the locally-resolved ThisUnit/ThisTile)
    // declared by the components of the faction's live units.
    std::vector<ActiveEffect_t> CollectUnitEffects_() const;

    // Permanent bonuses from the faction definition config.
    std::vector<ActiveEffect_t> CollectDefinitionEffects_() const;

    // Continuous effects declared on techs this faction has discovered.
    std::vector<ActiveEffect_t> CollectDiscoveredTechEffects_() const;

    // Universal tile-yield rules (MaxClamp resource caps, etc.) from GameDataContext.
    std::vector<ActiveEffect_t> CollectTileYieldRuleEffects_() const;

    // Production.json continuous effects (merged into every faction pool).
    std::vector<ActiveEffect_t> CollectProductionEffects_() const;

    // pop_growth.json continuous baselines (StartingSize / MaxBaseSize).
    std::vector<ActiveEffect_t> CollectGrowthEffects_() const;

    // base_conquest.json continuous effects (pop-loss and conquered-drone-cap baselines).
    std::vector<ActiveEffect_t> CollectBaseConquestEffects_() const;

    // police_rules.json continuous effects (combat-unit away weight and effectiveness).
    std::vector<ActiveEffect_t> CollectPoliceRulesEffects_() const;
    std::vector<ActiveEffect_t> CollectPopCompositionEffects_() const;

    // The faction-lane half of each base's mood effects, stamped with the base, so a riot's
    // FactionUnits morale penalty reaches units.
    std::vector<ActiveEffect_t> CollectMoodEffects_() const;

    // Difficulty effects, re-resolved each rebuild since difficulty can change mid-campaign.
    std::vector<ActiveEffect_t> CollectDifficultyEffects_() const;

    // Session native life level continuous effects; same mid-campaign rule as difficulty.
    std::vector<ActiveEffect_t> CollectNativeLifeEffects_() const;

    // eco_damage.json continuous effects (the clean-minerals baseline).
    std::vector<ActiveEffect_t> CollectEcoDamageEffects_() const;

    // probe_actions.json continuous effects (garrison weight and subversion cost baselines).
    std::vector<ActiveEffect_t> CollectProbeActionEffects_() const;

    // For every other session faction, the FactionPair effects of the status the two hold,
    // tagged with that partner. None while the faction is not bound to a session.
    std::vector<ActiveEffect_t> CollectDiplomaticStatusEffects_() const;

    // Erase effects whose removedByTech is already discovered.
    static void ApplyRemovedByTech_(FactionEffects_t& rEffects, const ResearchManager& rResearch);

    // Every contributor's revision in fixed order, to validate and stamp the cache. A new
    // contributor is added here and in CollectSources_.
    void CollectRevisions_(std::vector<uint64_t>& rOut) const;

    // Rebuild the pool if any contributor revision changed since the last stamp.
    void Validate_() const;

    // Recollect the pool, stamp from the pre-rebuild scratch revisions, advance version.
    void Rebuild_() const;
    // Every source's effects, in a fixed order.
    std::vector<ActiveEffect_t> CollectSources_() const;

    const Faction& m_rFaction;
    const BuildingRegistry& m_rBuildingRegistry;
    const Revision& m_rBaseListRevision;
    const std::vector<EffectConfig_t>& m_rTileYieldRules;
    const SocialRatingRegistry& m_rSocialRatings;
    const std::vector<EffectConfig_t>& m_rProductionEffects;
    const std::vector<EffectConfig_t>& m_rBaseConquestEffects;
    const std::vector<EffectConfig_t>& m_rPoliceRules;
    const std::vector<EffectConfig_t>& m_rPopCompositionEffects;
    const std::vector<EffectConfig_t>& m_rGrowthEffects;
    const DifficultyConfig_t& m_rDifficulty;
    const NativeLifeLevelConfig_t& m_rNativeLifeLevels;
    const std::vector<EffectConfig_t>& m_rEcoDamageEffects;
    const std::vector<EffectConfig_t>& m_rProbeActionEffects;
    const DiplomacyConfig_t& m_rDiplomacy;

    // The empty initial stamp never matches a real collection. m_scratchRevisions keeps
    // validation allocation-free.
    mutable FactionEffects_t m_cachedPool;
    mutable std::vector<uint64_t> m_cachedStamp;
    mutable std::vector<uint64_t> m_scratchRevisions;
    mutable uint64_t m_version = 0;
};

} // namespace ac
