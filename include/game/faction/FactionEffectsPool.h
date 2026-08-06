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
    FactionEffectsPool(const Faction& rFaction,
                       const BuildingRegistry* pBuildingRegistry,
                       const Revision& rBaseListRevision,
                       const std::vector<EffectConfig_t>* pTileYieldRules = nullptr,
                       const SocialRatingRegistry* pSocialRatings = nullptr);

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

    // Universal tile-yield rules (TileResourceCap, etc.) from GameDataContext.
    std::vector<ActiveEffect_t> CollectTileYieldRuleEffects_() const;

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
    const BuildingRegistry* m_pBuildingRegistry;
    const Revision& m_rBaseListRevision;
    const std::vector<EffectConfig_t>* m_pTileYieldRules;
    const SocialRatingRegistry* m_pSocialRatings;

    // The empty initial stamp never equals a real collection, so no "never built"
    // sentinel is needed. m_scratchRevisions is reused between validations to keep the
    // read path allocation-free.
    mutable FactionEffects_t m_cachedPool;
    mutable std::vector<uint64_t> m_cachedStamp;
    mutable std::vector<uint64_t> m_scratchRevisions;
    mutable uint64_t m_version = 0;
};

} // namespace ac
