#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/effects/ActiveEffect.h"
#include <string>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class Tile;
class UnitComponentRegistry;
class WorldMap;

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
    TileEffectsContext(WorldMap& rWorldMap, const ImprovementRegistry& rImprovements,
                       const UnitComponentRegistry* pUnitComponents);

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
    // Intrinsic (terrain/improvement/river) plus area effects only — no base-wide modifiers.
    TileResources_t ResolveTileYield(const Tile& rTile) const;

    // As above, but also folds in any per-tile StatModifier from rBaseEffects whose selector
    // matches this tile (e.g. a building's "+1 mineral to every worked Mine"). isBaseTile
    // distinguishes the base center tile so BaseTile-selector modifiers resolve correctly.
    // This is the single entry point for a worked tile's full pre-pop-multiplier yield.
    // Takes BaseEffects_t: only a base's final effect list may drive the selector pass —
    // handing it the raw faction pool would apply other bases' per-tile modifiers here.
    TileResources_t ResolveTileYield(const Tile& rTile, bool isBaseTile,
                                     const BaseEffects_t& rBaseEffects) const;

    // Combined defense multiplier including aura effects (e.g. nearby Sensor, Rocky terrain).
    double ResolveTileDefenseMultiplier(const Tile& rTile) const;

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
    // Resolves nutrient/mineral/energy from an already-collected effect list (energy seeded
    // from the tile's elevation). Shared by both ResolveTileYield overloads.
    TileResources_t ResolveYieldFromEffects_(const Tile& rTile,
                                             const std::vector<ActiveEffect_t>& effects) const;

    WorldMap& m_rWorldMap;
    const ImprovementRegistry& m_rImprovements;
    // Max per-effect radius across all improvement configs and unit-component ThisTile
    // effects; bounds the aura scan. Cached in the constructor.
    int m_maxRadius;
};

} // namespace ac
