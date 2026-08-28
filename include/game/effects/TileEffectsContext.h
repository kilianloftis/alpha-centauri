#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/effects/ActiveEffect.h"
#include <span>
#include <string>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class Tile;
class UnitComponentRegistry;
class WorldMap;
struct TileYieldRulesConfig_t;

// Bundles WorldMap and ImprovementRegistry into a single dependency that knows how to resolve
// tile-level effects - yield, defense multipliers, and terrain mutations like Condenser moisture.
// Passed into sub-systems (WorkerAssignmentManager, ResourceManager) as a single reference so
// they don't need to know about WorldMap or ImprovementRegistry individually.
class TileEffectsContext
{
public:
    // pUnitComponents is only used to size the aura scan radius: unit components can carry
    // ThisTile-scoped effects with a radius (mobile auras, e.g. a sensor pod), and the scan
    // bound must cover the largest such radius. Pass nullptr if units never project auras.
    // rYieldRules must outlive this context: it is stamped into every per-tile EffectContext_t
    // so terrain-scaled amount sources (ElevationEnergy) can read the world's yield scalars.
    TileEffectsContext(WorldMap& rWorldMap, const ImprovementRegistry& rImprovements,
                       const UnitComponentRegistry* pUnitComponents,
                       const TileYieldRulesConfig_t& rYieldRules);

    // WorldMap access — used by callers (e.g. BaseManager) that need the map for spatial
    // queries like computing workable tile positions.
    WorldMap& GetWorldMap();
    const WorldMap& GetWorldMap() const;

    // Collects this tile's own effects plus any radius-extending effects from nearby tiles —
    // improvements/terrain (e.g. a Sensor or Mirror whose effect radius reaches rTile) and
    // units whose components project ThisTile effects (including units standing on rTile
    // itself). Used by all three resolvers below.
    std::vector<ActiveEffect_t> CollectAreaEffects(const Tile& rTile) const;

    // Combined nutrient/mineral/energy yield including aura effects (e.g. nearby Mirror).
    // Intrinsic (terrain/improvement/river) plus area effects only — no base-wide modifiers
    // and no tech-gated MaxClamp. bypass_clamp modifiers are included.
    // effective == potential when no MaxClamp/MinClamp is present in the effect list.
    TileYieldView_t ResolveTileYield(const Tile& rTile) const;

    // As above, but also folds in any per-tile StatModifier from rBaseEffects whose selector
    // matches this tile (including AnyTile MaxClamp resource caps). bypass_clamp modifiers
    // are added after clamps. Caps whose removed_by_tech has been discovered are already
    // absent from the faction pool. Production reads .effective; UI preview for unworked
    // tiles uses the same path with bIsBaseTile == false (as-if-worked tile-level yield,
    // without pop multipliers).
    TileYieldView_t ResolveTileYield(const Tile& rTile, bool bIsBaseTile,
                                     const BaseEffects_t& rBaseEffects) const;

    // Combined defense multiplier including aura effects (e.g. nearby Sensor, Rocky terrain).
    // forFaction selects which territory-owned improvement auras apply (Sensor only benefits
    // the faction that owns the Sensor's tile). Terrain and non-owned improvements always apply.
    double ResolveTileDefenseMultiplier(const Tile& rTile, FactionId_t forFaction) const;

    const ImprovementRegistry& GetImprovements() const;

    // Re-derives rTile's effective moisture from its stored base moisture plus any Condenser
    // aura in range, then calls Tile::SetMoisture(). Always recomputed from scratch so
    // multiple overlapping Condensers and add/remove order never cause drift.
    void RecomputeMoisture(Tile& rTile);

    // Adds improvementId to rTile, then re-runs RecomputeMoisture for every tile within
    // that improvement's radius so any terrain-mutating effect (e.g. Condenser) takes effect
    // immediately. Always use this instead of Tile::AddImprovement directly.
    void AddImprovementWithEffects(Tile& rTile, const std::string& improvementId);

    // Removes improvementId from rTile, then re-runs RecomputeMoisture over the same radius
    // so the bonus reverts cleanly even when other Condensers still cover some of those tiles.
    void RemoveImprovementWithEffects(Tile& rTile, const std::string& improvementId);

private:
    // Resolves one resource lane from an already-partitioned pointer list (no ActiveEffect_t
    // copies). When bIncludeClamps is false, MaxClamp / MinClamp contributions are skipped
    // (UI potential yield).
    int ResolveResource_(const Tile& rTile, std::span<const ActiveEffect_t*> effects,
                         StatId_t stat, bool bIncludeClamps) const;

    // Pointer-partition + resolve into effective (clamped) / potential (unclamped) totals.
    // Both include bypass_clamp Adds.
    struct YieldLanes_t
    {
        TileResources_t effective;
        TileResources_t potential;
    };
    YieldLanes_t ResolveYieldLanes_(const Tile& rTile,
                                    const std::vector<ActiveEffect_t>& effects) const;

    TileYieldView_t ResolveYieldFromEffects_(const Tile& rTile,
                                             const std::vector<ActiveEffect_t>& effects) const;

    WorldMap& m_rWorldMap;
    const ImprovementRegistry& m_rImprovements;
    const TileYieldRulesConfig_t& m_rYieldRules;
    // Max ThisTile-scoped effect radius across improvement configs and unit components;
    // bounds the aura scan. Cached in the constructor.
    int m_maxRadius;
};

} // namespace ac
