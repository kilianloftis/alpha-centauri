#pragma once

#include "game/faction/base/BaseTypes.h"
#include "lib/effects/BonusEffect.h"
#include <string>
#include <vector>

namespace ac
{

// Forward declarations
class BaseManager;
class BuildingRegistry;
class Faction;
class PopContainer;
class Tile;
class ImprovementRegistry;
struct UnitComponentConfig_t;
struct PopTypeConfig_t;

struct ActiveEffect_t
{
    const EffectConfig_t* config;   // non-owning, points into static config data
    std::string sourceId;           // "command_nexus", "free_market", etc — for breakdown/UI
    const BaseManager* originBase = nullptr; // only set for ThisBase-scoped effects
};

struct StatBreakdown_t
{
    double total = 0.0;

    struct Contribution
    {
        std::string sourceId;
        double amount;
        ModifierOp op;
    };

    std::vector<Contribution> contributions;
};

std::vector<ActiveEffect_t> CollectActiveEffects(const Faction& rFaction,
                                                  const BuildingRegistry& rBuildingRegistry);

// baseValue seeds the additive total before contributions are summed — required for stats
// that are resolved purely through multiplicative modifiers (e.g. CostMultiplier, which has
// no Add contributions of its own and needs a base of 1.0 for MultiplyGeometric to act on).
StatBreakdown_t ResolveStatModifiers(const std::vector<ActiveEffect_t>& matching, double baseValue = 0.0);

// Returns effects whose target stat matches the given StatId.
// Only includes StatModifierEffect_t instances.
std::vector<ActiveEffect_t> FilterByStatId(const std::vector<ActiveEffect_t>& effects, StatId statId);

// Returns effects that apply to the given base.
// Includes ThisBase effects originating from this base, plus all AllOwnerBases, FactionGlobal, and WorldGlobal effects.
std::vector<ActiveEffect_t> FilterForBase(const std::vector<ActiveEffect_t>& effects, const BaseManager& rBase);

// Returns effects whose scope matches exactly.
std::vector<ActiveEffect_t> FilterByScope(const std::vector<ActiveEffect_t>& effects, EffectScope_t scope);

// Collects all effects from a list of unit components as ActiveEffect_t instances.
std::vector<ActiveEffect_t> CollectUnitEffects(const std::vector<const UnitComponentConfig_t*>& components);

// Collects a single pop type's own effects (both ThisPop-scoped tile multipliers and
// ThisBase-scoped flat generation bonuses). sourceId is the pop type's id.
std::vector<ActiveEffect_t> CollectPopEffects(const PopTypeConfig_t& rConfig);

// Collects the ThisBase-scoped flat generation effects from every pop in rPops, tagged
// with rOriginBase so they can be merged into a base's active effects before resolving
// production. ThisPop-scoped tile multiplier effects are excluded — those are resolved
// locally by Pop::ApplyTileMultipliers and never enter the base-wide pool.
std::vector<ActiveEffect_t> CollectFromPops(const PopContainer& rPops, const BaseManager& rOriginBase);

// Collects every ThisTile-scoped effect from a tile's own feature ids (rockiness, moisture,
// river, fungus, landmark, improvements), each looked up in rImprovements. sourceId is the
// matching feature's id. Resolved locally by ResolveTileYield/ResolveTileDefenseMultiplier —
// never enters the base-wide active effects pool (FilterForBase always excludes ThisTile).
std::vector<ActiveEffect_t> CollectTileEffects(const Tile& rTile, const ImprovementRegistry& rImprovements);

// Resolves the combined defense multiplier for a unit defending on rTile (e.g. 1.25 for a
// single +25% source, 1.5 for two stacked +25% sources). 1.0 if the tile grants no bonus.
double ResolveTileDefenseMultiplier(const Tile& rTile, const ImprovementRegistry& rImprovements);

// Resolves this tile's intrinsic nutrient/mineral/energy yield from its own ThisTile-scoped
// effects. Energy is seeded from GetElevationEnergySeed() before resolving so River/Fungus/
// improvement Add effects layer on top of the elevation-derived value; Nutrients/Minerals
// have no continuous seed and are purely effects-driven (Moisture/Rockiness/improvements).
TileResources_t ResolveTileYield(const Tile& rTile, const ImprovementRegistry& rImprovements);

} // namespace ac
