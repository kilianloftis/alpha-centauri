#pragma once

#include "lib/Revision.h"
#include "game/effects/ActiveEffect.h"
#include <cstdint>
#include <vector>

namespace ac
{

class Faction;
class BuildingRegistry;

// Assembles and memoizes the faction-wide active effect pool (code review finding 1.1).
// Reads rFaction's subsystems through their public API. rFaction is taken as a parameter
// on each call rather than stored, so the pool holds no back-reference to its owner —
// only the cache itself and the two dependencies that are not "the whole faction"
// (the building registry and the base-list revision) are held as state.
class FactionEffectsPool
{
public:
    FactionEffectsPool(const BuildingRegistry* pBuildingRegistry,
                       const Revision& rBaseListRevision,
                       const std::vector<EffectConfig_t>* pTileYieldRules = nullptr);

    // The validated pool for rFaction. The reference is valid until the next
    // effect-source mutation on rFaction.
    const FactionEffects_t& Get(const Faction& rFaction) const;

    // Monotonic pool version for rFaction; changes iff the pool content changed.
    uint64_t GetVersion(const Faction& rFaction) const;

private:
    // Buildings across all bases, with grant chains expanded.
    std::vector<ActiveEffect_t> CollectBuildingEffects_(const Faction& rFaction) const;

    // Faction-lane effects (any scope except the locally-resolved ThisPop/ThisBase)
    // declared by pops living in the faction's bases. ThisBase pop effects merge per base
    // via CollectFromPops; ThisPop effects are resolved by the pop itself.
    std::vector<ActiveEffect_t> CollectPopEffects_(const Faction& rFaction) const;

    // Faction-lane effects (any scope except the locally-resolved ThisUnit/ThisTile)
    // declared by the components of the faction's live units.
    std::vector<ActiveEffect_t> CollectUnitEffects_(const Faction& rFaction) const;

    // Permanent bonuses from the faction definition config.
    std::vector<ActiveEffect_t> CollectDefinitionEffects_(const Faction& rFaction) const;

    // Universal tile-yield rules (TileResourceCap, etc.) from GameDataContext.
    std::vector<ActiveEffect_t> CollectTileYieldRuleEffects_() const;

    // Fills rOut with the revision of every pool contributor in a fixed order — the
    // single source of truth for what the pool depends on, used both to validate the
    // cache and to stamp it after a rebuild. A new contributor must be added here and
    // collected in Rebuild_.
    void CollectRevisions_(const Faction& rFaction, std::vector<uint64_t>& rOut) const;

    // Rebuild the pool if any contributor revision changed since the last stamp.
    void Validate_(const Faction& rFaction) const;

    // Recollect the pool, restamp, and advance the version.
    void Rebuild_(const Faction& rFaction) const;

    const BuildingRegistry* m_pBuildingRegistry;
    const Revision& m_rBaseListRevision;
    const std::vector<EffectConfig_t>* m_pTileYieldRules;

    // The empty initial stamp never equals a real collection, so no "never built"
    // sentinel is needed. m_scratchRevisions is reused between validations to keep the
    // read path allocation-free.
    mutable FactionEffects_t m_cachedPool;
    mutable std::vector<uint64_t> m_cachedStamp;
    mutable std::vector<uint64_t> m_scratchRevisions;
    mutable uint64_t m_version = 0;
};

} // namespace ac
