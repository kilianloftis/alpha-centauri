#pragma once

#include "game/faction/base/BaseTypes.h"
#include "lib/effects/BonusEffect.h"
#include <string>
#include <vector>

namespace ac
{

// Forward declarations
class BaseManager;
class Faction;
class IEffectsProvider;
class PopContainer;
class Tile;
class ImprovementRegistry;
struct BuildingConfig_t;
struct UnitComponentConfig_t;
struct PopTypeConfig_t;

struct ActiveEffect_t
{
    const EffectConfig_t* config;   // non-owning, points into static config data
    std::string sourceId;           // "command_nexus", "free_market", etc — for breakdown/UI
    const BaseManager* originBase = nullptr; // only set for ThisBase-scoped effects
};

// The faction-wide effect pool: what CollectActiveEffects gathers (buildings with grants
// expanded, social policies, pop/unit faction-lane effects), plus other factions' WorldGlobal
// contributions appended by the turn stages. Deliberately a distinct type from BaseEffects_t:
// the pool still holds every base's ThisBase effects and the FactionUnits lane, so resolving
// base stats directly against it would count effects that don't apply — FilterForBase is the
// only path from a pool to a base's effect list.
struct FactionEffects_t
{
    std::vector<ActiveEffect_t> effects;
};

// One base's effect list: FilterForBase over the faction pool, then the base's pop-generated
// ThisBase effects (CollectFromPops) and expanded social-rating effects
// (ExpandSocialRatingEffects) merged in — see BaseManager::BuildBaseEffects_. Every entry
// applies to that single base, which is the precondition for flat base-level resolution
// (FilterFlatByStatId) and the per-tile selector pass (TileEffectsContext::ResolveTileYield).
struct BaseEffects_t
{
    std::vector<ActiveEffect_t> effects;
};

// Runtime context an effect's condition is evaluated against. Fields are optional; a
// condition that references an absent field evaluates false. Combat sets targetTile to the
// defender's tile so TargetTileHas conditions can inspect it.
struct EffectContext_t
{
    const Tile* targetTile = nullptr;
};

// True if config carries no condition, or its condition is satisfied by ctx.
bool ConditionSatisfied(const EffectConfig_t& config, const EffectContext_t& ctx);

// Appends non-Instantaneous effects from a config list as ActiveEffect_t instances.
// Used by building, pop, unit, and tile effect collection; pOriginBase is set only
// when the effect's scope is ThisBase. This (and its filtered variants below) is the
// single config->ActiveEffect_t conversion — new effect sources should collect through
// one of these rather than hand-rolling the loop.
void AppendActiveEffects(const std::vector<EffectConfig_t>& rEffects,
                         const BaseManager* pOriginBase,
                         const std::string& sourceId,
                         std::vector<ActiveEffect_t>& rOut);

// As AppendActiveEffects, but keeps only faction-lane scopes (see IsFactionLane): what a
// source contributes to the faction pool when its base/pop/unit/tile-local scopes are
// resolved elsewhere. Used by Faction's pop and unit collectors.
void AppendFactionLaneEffects(const std::vector<EffectConfig_t>& rEffects,
                              const std::string& sourceId,
                              std::vector<ActiveEffect_t>& rOut);

// True if rEffect projects onto a tile `distance` tiles from its host: ThisTile lane,
// Continuous, and radius >= distance. distance 0 = the host tile itself. The single
// filter for a tile's own effects, neighboring improvement/terrain auras, and
// unit-projected auras.
bool TileEffectReaches(const EffectConfig_t& rEffect, int distance);

// As AppendActiveEffects, but keeps only effects satisfying TileEffectReaches(e, distance).
void AppendTileEffects(const std::vector<EffectConfig_t>& rEffects,
                       const std::string& sourceId,
                       int distance,
                       std::vector<ActiveEffect_t>& rOut);

class BuildingRegistry;

// Expands any GrantBuildingEffect_t entries in `effects`, appending the granted
// building's own effects. rBases is used to attribute ThisBase-scoped sub-effects
// to the correct base when the grant itself has no originBase (faction-global scope).
// Returns the expanded vector by value.
std::vector<ActiveEffect_t> ExpandGrantBuildingEffects(
    std::vector<ActiveEffect_t> effects,
    const BuildingRegistry& rRegistry,
    const std::vector<const BaseManager*>& rBases);

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

FactionEffects_t CollectActiveEffects(const IEffectsProvider& rProvider);

// Apply a stack of modifier contributions to a base value using the standard formula:
//   result = (base + sumOfAdds) * (1 + sumOf(percent/100)) * productOfGeometric
// Each pair is {amount, op}. Contributions are applied in the order given.
double ApplyModifierStack(double base, const std::vector<std::pair<double, ModifierOp>>& contributions);

// baseValue seeds the additive total before contributions are summed. It is deliberately
// NOT defaulted: stats resolved purely through multiplicative modifiers (e.g.
// CostMultiplier, tile defense) silently resolve to 0 from a 0 base, so every caller must
// state its seed — 0.0 for additive stats, 1.0 for pure multipliers, or a raw value the
// modifiers scale (tile yield, growth rate's 100).
StatBreakdown_t ResolveStatModifiers(const std::vector<ActiveEffect_t>& matching, double baseValue);

// Returns effects whose target stat matches the given StatId.
// Only includes StatModifierEffect_t instances. Condition-carrying effects are excluded:
// they only apply through a matching runtime context (see FilterByStatIdInContext).
std::vector<ActiveEffect_t> FilterByStatId(const std::vector<ActiveEffect_t>& effects, StatId statId);

// Like FilterByStatId, but for a specific runtime context: includes unconditional effects
// plus any condition-carrying effect whose condition is satisfied by ctx. This is the entry
// point for context-dependent resolution such as combat (attack/defense vs a given target).
std::vector<ActiveEffect_t> FilterByStatIdInContext(const std::vector<ActiveEffect_t>& effects,
                                                    StatId statId, const EffectContext_t& ctx);

// Like FilterByStatId, but excludes per-tile modifiers (StatModifiers carrying a tile
// selector). Base-level resolution only: selector-carrying modifiers have already been
// applied per worked tile and must not be counted a second time. Accepting BaseEffects_t
// (never a raw vector or the pool) makes running this filter at any other stage a compile
// error instead of a doc violation.
std::vector<ActiveEffect_t> FilterFlatByStatId(const BaseEffects_t& rBaseEffects, StatId statId);

// Narrows the faction pool to the effects that apply to the given base: ThisBase effects
// originating from it, plus all AllOwnerBases, FactionGlobal, and WorldGlobal effects.
// The only constructor of a BaseEffects_t from a pool.
BaseEffects_t FilterForBase(const FactionEffects_t& rFactionEffects, const BaseManager& rBase);

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

// Collects every ThisTile-scoped effect from rTile's own features only: terrain feature ids
// (rockiness, moisture, river, fungus) looked up in rImprovements, plus each improvement
// config held directly on the tile. sourceId is the matching feature's id. Does NOT include
// aura effects from nearby tiles —
// use TileEffectsContext::CollectAreaEffects for that (which needs WorldMap).
// Never enters the base-wide active effects pool (FilterForBase always excludes ThisTile).
std::vector<ActiveEffect_t> CollectTileEffects(const Tile& rTile, const ImprovementRegistry& rImprovements);

// Fire all Instantaneous effects declared on rBuilding against rBase.
// GrantBuilding: calls rBase.AddBuilding immediately.
// GrantTech / GrantUnit: logged as TODO stubs until those systems are wired.
// Call this right after a building is added to the base (e.g. from on_production_completed).
void DispatchInstantaneousEffects(const BuildingConfig_t& rBuilding, BaseManager& rBase);

} // namespace ac
