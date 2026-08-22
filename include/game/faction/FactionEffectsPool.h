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

// Assembles and memoizes the faction-wide *local* active effect pool for one owning
// Faction (bound at construction). WorldGlobal peers and council extras are composed
// later by Faction::GetActiveEffects — this pool never sees them.
class FactionEffectsPool
{
public:
    // All data dependencies come from GameDataContext, which the composition root always
    // supplies complete. They were optional pointers whose null branches silently dropped
    // whole effect lanes — no buildings, no tile-yield rules, no social-rating expansion —
    // with no diagnostic, which is how fixture factions came to resolve ratings differently
    // from real ones.
    FactionEffectsPool(const Faction& rFaction,
                       const BuildingRegistry& rBuildingRegistry,
                       const Revision& rBaseListRevision,
                       const std::vector<EffectConfig_t>& rTileYieldRules,
                       const SocialRatingRegistry& rSocialRatings,
                       const std::vector<EffectConfig_t>& rProductionEffects,
                       const std::vector<EffectConfig_t>& rBaseConquestEffects);

    // The validated local pool. Valid until the next effect-source mutation on the owner.
    const FactionEffects_t& Get() const;

    // Monotonic pool version; changes iff the pool content changed.
    uint64_t GetVersion() const;

private:
    // Raw continuous effects from constructed buildings across all bases (no grant expand).
    std::vector<ActiveEffect_t> CollectBuildingEffects_() const;

    // Faction-lane effects (any scope except the locally-resolved ThisPop/ThisBase)
    // declared by pops living in the faction's bases. ThisBase pop effects merge per base
    // via CollectFromPops; ThisPop effects are resolved by the pop itself.
    std::vector<ActiveEffect_t> CollectPopEffects_() const;

    // Faction-lane effects (any scope except the locally-resolved ThisUnit/ThisTile)
    // declared by the components of the faction's live units.
    std::vector<ActiveEffect_t> CollectUnitEffects_() const;

    // Permanent bonuses from the faction definition config.
    std::vector<ActiveEffect_t> CollectDefinitionEffects_() const;

    // Continuous effects declared on techs this faction has discovered.
    std::vector<ActiveEffect_t> CollectDiscoveredTechEffects_() const;

    // Universal tile-yield rules (TileResourceCap, etc.) from GameDataContext.
    std::vector<ActiveEffect_t> CollectTileYieldRuleEffects_() const;

    // Production.json continuous effects (prototype StartingExperience, …).
    std::vector<ActiveEffect_t> CollectProductionEffects_() const;

    // base_conquest.json continuous effects (pop-loss baselines).
    std::vector<ActiveEffect_t> CollectBaseConquestEffects_() const;

    // Session difficulty continuous effects, re-resolved from the owner's GameDataContext
    // and current game rules. Not cached at construction: difficulty is changeable
    // mid-campaign, and CollectRevisions_ samples the game-rules revision to catch it.
    std::vector<ActiveEffect_t> CollectDifficultyEffects_() const;

    // Erase effects whose removedByTech is already discovered.
    static void ApplyRemovedByTech_(FactionEffects_t& rEffects, const ResearchManager& rResearch);

    // Fills rOut with the revision of every pool contributor in a fixed order — the
    // single source of truth for what the pool depends on, used both to validate the
    // cache and to stamp it after a rebuild. A new contributor must be added here and
    // collected in Rebuild_.
    void CollectRevisions_(std::vector<uint64_t>& rOut) const;

    // Rebuild the pool if any contributor revision changed since the last stamp.
    void Validate_() const;

    // Recollect the pool, stamp from the pre-rebuild scratch revisions, advance version.
    void Rebuild_() const;

    const Faction& m_rFaction;
    const BuildingRegistry& m_rBuildingRegistry;
    const Revision& m_rBaseListRevision;
    const std::vector<EffectConfig_t>& m_rTileYieldRules;
    const SocialRatingRegistry& m_rSocialRatings;
    const std::vector<EffectConfig_t>& m_rProductionEffects;
    const std::vector<EffectConfig_t>& m_rBaseConquestEffects;

    // The empty initial stamp never equals a real collection, so no "never built"
    // sentinel is needed. m_scratchRevisions is reused between validations to keep the
    // read path allocation-free.
    mutable FactionEffects_t m_cachedPool;
    mutable std::vector<uint64_t> m_cachedStamp;
    mutable std::vector<uint64_t> m_scratchRevisions;
    mutable uint64_t m_version = 0;
};

} // namespace ac
