#pragma once

#include "game/base/BaseTypes.h"
#include "game/effects/ActiveEffect.h"
#include "game/map/TileChangeListener.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class IWorldEffectsSource;
class Tile;
struct ImprovementConfig_t;
class UnitComponentRegistry;
class WorldMap;
struct TileYieldRulesConfig_t;
struct InteractionGridsConfig_t;

// Resolves tile-level effects (yield, defence, terrain mutations) over WorldMap and
// ImprovementRegistry.
class TileEffectsContext : public TileChangeListener
{
public:
    // pUnitComponents sizes the aura scan; nullptr if units never project auras. rYieldRules and
    // rInteractionGrids must outlive this.
    TileEffectsContext(WorldMap& rWorldMap, const ImprovementRegistry& rImprovements,
                       const UnitComponentRegistry* pUnitComponents,
                       const TileYieldRulesConfig_t& rYieldRules,
                       const InteractionGridsConfig_t& rInteractionGrids);
    ~TileEffectsContext() override;

    // Session WorldGlobal source. Unbound contexts resolve tile-local effects only.
    void BindWorldEffects(IWorldEffectsSource& rWorldEffects);
    void UnbindWorldEffects();

    // WorldMap access — used by callers (e.g. BaseManager) that need the map for spatial
    // queries like computing workable tile positions.
    WorldMap& GetWorldMap();
    const WorldMap& GetWorldMap() const;

    // rTile's own effects plus auras reaching it from nearby tiles and units.
    std::vector<ActiveEffect_t> CollectAreaEffects(const Tile& rTile) const;

    // Yield from the tile and its auras, without base modifiers or tech caps.
    TileYieldView_t ResolveTileYield(const Tile& rTile) const;

    // Also folds in rBaseEffects' per-tile modifiers that match, caps included; bypass_clamp ones
    // add after clamps.
    TileYieldView_t ResolveTileYield(const Tile& rTile, bool bIsBaseTile,
                                     const BaseEffects_t& rBaseEffects) const;

    // Defence multiplier with auras. Territory-owned auras count only for forFaction.
    double ResolveTileDefenseMultiplier(const Tile& rTile, FactionId_t forFaction) const;

    const ImprovementRegistry& GetImprovements() const;

    // Recomputed from base moisture plus Condensers in range, so overlap and order never drift.
    void RecomputeMoisture(Tile& rTile);

    // Adds occupantId, drops improvements that can no longer stay, and recomputes moisture in its
    // radius. Use instead of Tile::AddOccupant.
    void AddOccupantWithEffects(Tile& rTile, const std::string& occupantId);

    void OnTileChanged(Tile& rTile, std::string_view keepId) override;

    // Removes occupantId and recomputes moisture over its radius. An unknown id removes nothing.
    void RemoveOccupantWithEffects(Tile& rTile, const std::string& occupantId);

private:
    // One resource lane from partitioned pointers.
    int ResolveResource_(const Tile& rTile, std::span<const ActiveEffect_t*> effects,
                         StatId_t stat) const;
    TileResources_t ResolveResources_(const Tile& rTile,
                                      std::span<const ActiveEffect_t*> effects) const;

    // Pointer-partition + resolve into effective (clamped) / potential (unclamped) totals.
    // Both include bypass_clamp Adds.
    struct YieldLanes_t
    {
        TileResources_t effective;
        TileResources_t potential;
    };
    YieldLanes_t ResolveYieldLanes_(const Tile& rTile,
                                    const std::vector<ActiveEffect_t>& effects) const;

    void RemoveImprovementsThatCannotRemain_(Tile& rTile, std::string_view keepId);

    TileYieldView_t ResolveYieldFromEffects_(const Tile& rTile,
                                             const std::vector<ActiveEffect_t>& effects) const;

    WorldMap& m_rWorldMap;
    const ImprovementRegistry& m_rImprovements;
    const TileYieldRulesConfig_t& m_rYieldRules;
    const InteractionGridsConfig_t& m_rInteractionGrids;
    // Max ThisTile-scoped effect radius across improvement configs and unit components;
    // bounds the aura scan. Cached in the constructor.
    int m_maxRadius;
    IWorldEffectsSource* m_pWorldEffects = nullptr;
    bool m_bSweepingOccupancy = false;
    std::vector<Tile*> m_pendingOccupancy;
};

} // namespace ac
