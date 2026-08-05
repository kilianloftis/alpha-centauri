#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/effects/BonusEffect.h"
#include "game/map/Tile.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ac
{

// Forward declarations
class BaseManager;
class Faction;
class GameState;
class IEffectsProvider;
class PopulationManager;
class Unit;
class UnitDesign;
class WorldMap;
struct BuildingConfig_t;
struct UnitComponentConfig_t;
struct PopTypeConfig_t;

struct ActiveEffect_t
{
    // config is always non-null after construction (points into static or stable store data).
    ActiveEffect_t(const EffectConfig_t& rConfig, std::string sourceId,
                   const BaseManager* pOriginBase = nullptr)
        : config(&rConfig)
        , sourceId(std::move(sourceId))
        , originBase(pOriginBase)
    {
    }

    const EffectConfig_t* config;
    std::string sourceId;           // "command_nexus", "free_market", etc — for breakdown/UI
    // Set for ThisBase / ProducedAtThisBase, and for FactionUnits collected from a base
    // (per-base attribution for conditions like OriginBaseIsTargetBase — not a membership filter).
    const BaseManager* originBase = nullptr;
    // Set for improvements with owned_by_territory (e.g. Sensor): the FactionId_t that owns
    // the host tile's territory at collection time. When set, tile defense / detection / fog
    // vision only apply the effect for that faction. Unowned territory stores k_NoFactionOwner.
    std::optional<FactionId_t> ownerFaction;
};

// The faction-wide effect pool: what CollectActiveEffects gathers (buildings with grants
// expanded, social policies, pop/unit faction-lane effects), plus — when the faction is bound
// to a session world source — other factions' WorldGlobal and council extras. Deliberately a
// distinct type from BaseEffects_t: the pool still holds every base's ThisBase effects and the
// FactionUnits lane, so resolving base stats directly against it would count effects that don't
// apply — FilterForBase is the only path from a pool to a base's effect list.
struct FactionEffects_t
{
    std::vector<ActiveEffect_t> effects;
};

// One base's effect list: FilterForBase over the faction pool, then the base's pop-generated
// ThisBase effects (CollectFromPops) and expanded social-rating effects
// (ExpandSocialRatingEffects) merged in — see BaseManager::BuildBaseEffects_. Every entry
// applies to that single base, which is the precondition for base-level resolution
// (FilterBaseLevelByStatId) and the per-tile selector pass (TileEffectsContext::ResolveTileYield).
struct BaseEffects_t
{
    std::vector<ActiveEffect_t> effects;
};

// Which side of a fight is resolving stats. None for non-combat resolution.
enum class CombatRole_t
{
    None,
    Attacker,
    Defender,
};

// Runtime context an effect's condition is evaluated against. Fields are optional; a
// condition that references an absent field evaluates false. Combat sets targetTile to the
// defender's tile so TargetTileHas conditions can inspect it. Tile yield also sets
// targetTile so amount_source (e.g. ElevationEnergySeed) can read the host tile.
// combatRole enables IsDefending (SE Morale defense-in-base extras).
// pAttacker enables AttackerIsEmbarked (and future attacker-side conditions).
struct EffectContext_t
{
    const Tile* targetTile = nullptr;
    CombatRole_t combatRole = CombatRole_t::None;
    const Unit* pAttacker = nullptr;
};

// Resolves a StatModifier's effective contribution amount. Literal `amount` when
// amountSource is absent; otherwise scales a tile-derived value (requires targetTile).
inline double EffectiveStatModifierAmount(const StatModifierEffect_t& rMod,
                                          const EffectContext_t* pCtx)
{
    if (!rMod.amountSource.has_value())
    {
        return rMod.amount;
    }
    switch (*rMod.amountSource)
    {
        case StatModifierEffect_t::AmountSource_t::ElevationEnergySeed:
            if (!pCtx || !pCtx->targetTile)
            {
                return 0.0;
            }
            return static_cast<double>(pCtx->targetTile->GetElevationEnergySeed()) * rMod.amount;
    }
    return rMod.amount;
}

// True if config carries no condition, or its condition is satisfied by ctx.
// OriginBaseIsTargetBase requires pOriginBase (ActiveEffect_t::originBase).
bool ConditionSatisfied(const EffectConfig_t& config, const EffectContext_t& ctx,
                        const BaseManager* pOriginBase = nullptr);

// True if config carries no unitFilter, or its unitFilter matches rUnit (Domain /
// HasComponent). Used by CollectLiveUnitEffects to drop FactionUnits (and any other)
// effects that do not apply to this unit.
bool UnitFilterSatisfied(const EffectConfig_t& config, const Unit& rUnit);

// Appends non-Instantaneous effects from a config list as ActiveEffect_t instances.
// Used by building, pop, unit, and tile effect collection; pOriginBase is recorded when
// TagsOriginBase(scope) (ThisBase, ProducedAtThisBase, FactionUnits). This (and its
// filtered variants below) is the single config->ActiveEffect_t conversion — new effect
// sources should collect through one of these rather than hand-rolling the loop.
// Accepts span so stable stores (e.g. CouncilEffects deques) can append one config at a time.
void AppendActiveEffects(std::span<const EffectConfig_t> rEffects,
                         const BaseManager* pOriginBase,
                         const std::string& sourceId,
                         std::vector<ActiveEffect_t>& rOut);

// As AppendActiveEffects, but keeps only faction-lane scopes (see IsFactionLane): what a
// source contributes to the faction pool when its base/pop/unit/tile-local scopes are
// resolved elsewhere. Used by Faction's pop and unit collectors.
void AppendFactionLaneEffects(std::span<const EffectConfig_t> rEffects,
                              const std::string& sourceId,
                              std::vector<ActiveEffect_t>& rOut);

// True if rEffect projects onto a tile `distance` tiles from its host: ThisTile lane,
// Continuous, and radius >= distance. distance 0 = the host tile itself. The single
// filter for a tile's own effects, neighboring improvement/terrain auras, and
// unit-projected auras.
bool TileEffectReaches(const EffectConfig_t& rEffect, int distance);

// As AppendActiveEffects, but keeps only effects satisfying TileEffectReaches(e, distance).
void AppendTileEffects(std::span<const EffectConfig_t> rEffects,
                       const std::string& sourceId,
                       int distance,
                       std::vector<ActiveEffect_t>& rOut);

class BuildingRegistry;

// Expands any GrantBuildingEffect_t entries in `effects`, appending the granted
// building's own effects. rBases is used to attribute ThisBase-scoped sub-effects
// to the correct base when the grant itself has no originBase (faction-global scope).
// Constructed buildings on rBases pre-seed the grant dedupe set so a grant of a
// building already built does not double-count that building's continuous effects.
// Returns the expanded vector by value.
std::vector<ActiveEffect_t> ExpandGrantBuildingEffects(
    std::vector<ActiveEffect_t> effects,
    const BuildingRegistry& rRegistry,
    const std::vector<const BaseManager*>& rBases);

struct StatBreakdown_t
{
    double total = 0.0;

    struct Contribution_t
    {
        std::string sourceId;
        double amount;
        ModifierOp_t op;
    };

    std::vector<Contribution_t> contributions;
};

const FactionEffects_t& CollectActiveEffects(const IEffectsProvider& rProvider);

// Apply a stack of modifier contributions to a base value using the standard formula:
//   result = (base + sumOfAdds) * (1 + sumOf(percent/100)) * productOfGeometric
// Each pair is {amount, op}. Ops are partitioned by kind (all Adds, then all AddPercents,
// then all MultiplyGeometrics) — contribution order within a kind does not change the result.
double ApplyModifierStack(double base, const std::vector<std::pair<double, ModifierOp_t>>& contributions);

// Single float→int rule for any resolved modifier total (ResolveStat, combat, council votes,
// ResourceManager, pop tile multipliers). Half-away from zero via std::lround — do not truncate
// or std::round the same stack elsewhere.
inline int FinalizeResolvedStat(double value)
{
    return static_cast<int>(std::lround(value));
}

// baseValue seeds the additive total before contributions are summed. It is deliberately
// NOT defaulted: stats resolved purely through multiplicative modifiers (e.g.
// CostMultiplier, tile defense) silently resolve to 0 from a 0 base, so every caller must
// state its seed — 0.0 for additive stats, 1.0 for pure multipliers, or a raw value the
// modifiers scale (tile yield, growth rate's 100). Templated on the input range so it
// accepts both an owned vector and a lazy Filter* view below without materializing one.
// pCtx resolves amount_source modifiers (e.g. ElevationEnergySeed); pass nullptr for
// context-free resolves (those filters already drop amount_source effects).
template <std::ranges::input_range Range>
StatBreakdown_t ResolveStatModifiers(Range&& matching, double baseValue,
                                     const EffectContext_t* pCtx = nullptr)
{
    StatBreakdown_t breakdown;
    breakdown.total = 0.0;

    for (const ActiveEffect_t& active : matching)
    {
        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&active.config->effect);
        if (!pStatModifier)
        {
            continue;
        }

        StatBreakdown_t::Contribution_t contribution;
        contribution.sourceId = active.sourceId;
        contribution.amount = EffectiveStatModifierAmount(*pStatModifier, pCtx);
        contribution.op = pStatModifier->op;
        breakdown.contributions.push_back(contribution);
    }

    std::sort(breakdown.contributions.begin(), breakdown.contributions.end(),
              [](const StatBreakdown_t::Contribution_t& a, const StatBreakdown_t::Contribution_t& b)
              {
                  return a.sourceId < b.sourceId;
              });

    std::vector<std::pair<double, ModifierOp_t>> stack;
    stack.reserve(breakdown.contributions.size());
    for (const StatBreakdown_t::Contribution_t& c : breakdown.contributions)
    {
        stack.emplace_back(c.amount, c.op);
    }
    breakdown.total = ApplyModifierStack(baseValue, stack);
    return breakdown;
}

// Lazy Filter* views borrow the lvalue `effects` vector — materialize into a named local
// before filtering if the source is a temporary (rvalue overloads are deleted).
inline auto FilterByStatId(const std::vector<ActiveEffect_t>& effects, StatId_t statId)
{
    return effects | std::views::filter([statId](const ActiveEffect_t& effect)
    {
        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        // Conditional / amount_source effects are excluded from context-free resolution.
        return pStatModifier && pStatModifier->stat == statId && !effect.config->condition
            && !pStatModifier->amountSource;
    });
}
inline auto FilterByStatId(std::vector<ActiveEffect_t>&& effects, StatId_t statId) = delete;

// Like FilterByStatId, but for a specific runtime context: includes unconditional effects
// plus any condition-carrying effect whose condition is satisfied by ctx. This is the entry
// point for context-dependent resolution such as combat (attack/defense vs a given target).
inline auto FilterByStatIdInContext(const std::vector<ActiveEffect_t>& effects,
                                    StatId_t statId, const EffectContext_t& ctx)
{
    return effects | std::views::filter([statId, ctx](const ActiveEffect_t& effect)
    {
        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        return pStatModifier && pStatModifier->stat == statId
            && ConditionSatisfied(*effect.config, ctx, effect.originBase);
    });
}
inline auto FilterByStatIdInContext(std::vector<ActiveEffect_t>&& effects,
                                    StatId_t statId, const EffectContext_t& ctx) = delete;

// Like FilterByStatId, but for base-level resolution only: excludes per-tile modifiers
// (StatModifiers carrying a tile selector) and condition-carrying effects. Selector
// modifiers have already been applied per worked tile and must not be counted a second
// time. Accepting BaseEffects_t (never a raw vector or the pool) makes running this
// filter at any other stage a compile error instead of a doc violation.
inline auto FilterBaseLevelByStatId(const BaseEffects_t& rBaseEffects, StatId_t statId)
{
    return rBaseEffects.effects | std::views::filter([statId](const ActiveEffect_t& effect)
    {
        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        return pStatModifier && pStatModifier->stat == statId && !pStatModifier->selector
            && !effect.config->condition && !pStatModifier->amountSource;
    });
}
inline auto FilterBaseLevelByStatId(BaseEffects_t&& rBaseEffects, StatId_t statId) = delete;

// Narrows the faction pool to the effects that apply to the given base: ThisBase effects
// originating from it, plus all AllOwnerBases, FactionGlobal, and WorldGlobal effects.
// The only constructor of a BaseEffects_t from a pool. Eager, not a view: the result is an
// independently-owned collection meant to be cached (see BaseManager::BuildBaseEffects_).
BaseEffects_t FilterForBase(const FactionEffects_t& rFactionEffects, const BaseManager& rBase);

// Returns a lazy view of effects whose scope matches exactly.
inline auto FilterByScope(const std::vector<ActiveEffect_t>& effects, EffectScope_t scope)
{
    return effects | std::views::filter([scope](const ActiveEffect_t& effect)
    {
        return effect.config->scope == scope;
    });
}
inline auto FilterByScope(std::vector<ActiveEffect_t>&& effects, EffectScope_t scope) = delete;

// Collects all effects from a list of unit components as ActiveEffect_t instances.
std::vector<ActiveEffect_t> CollectUnitEffects(const std::vector<const UnitComponentConfig_t*>& components);

// Resolve a unit design's intrinsic (component-only) stats / flags — no faction pool.
// Context-free: effects carrying a condition are skipped (same rule as FilterByStatId).
int ResolveStat(const UnitDesign& rDesign, StatId_t statId);
int ResolveStat(const UnitDesign& rDesign, StatId_t statId, const EffectContext_t& rCtx);
bool ResolveFlag(const UnitDesign& rDesign, RuleFlagId_t flagId);

// Sum of ModifierOp_t::Add contributions only (component effects). Ignores AddPercent /
// MultiplyGeometric — the SMAC-style base combat rating (e.g. laser 2, not 2 * 1.25).
int ResolveAdditiveStat(const UnitDesign& rDesign, StatId_t statId);

// A live unit's full effect list: design components, FactionUnits (all faction units),
// and ProducedAtThisBase matching Unit::GetProducedAtBase. Returned effects already
// satisfy UnitFilterSatisfied and the ProducedAt origin match — consumers (ResolveStat,
// HasPermission, etc.) need not re-check the unitFilter.
std::vector<ActiveEffect_t> CollectLiveUnitEffects(const Unit& rUnit);

// Resolve a live unit's stats / flags: design effects plus FactionUnits from the owner.
int ResolveStat(const Unit& rUnit, StatId_t statId);
int ResolveStat(const Unit& rUnit, StatId_t statId, const EffectContext_t& rCtx);
// Resolves only multiplicative contributions (AddPercent / MultiplyGeometric), seeded at
// baseValue. Add contributions are deliberately ignored. Used by psi combat, whose Attack
// and Defense strengths start at 1 instead of using conventional additive weapon/armour.
double ResolveMultiplicativeStat(const Unit& rUnit, StatId_t statId, double baseValue,
                                 const EffectContext_t& rCtx = {});
// Context-free: effects carrying a condition are skipped (same rule as FilterByStatId).
bool ResolveFlag(const Unit& rUnit, RuleFlagId_t flagId);

// Resolve a faction-wide RuleFlag from the continuous effect pool (buildings, SPs, etc.).
// Context-free: effects carrying a condition are skipped.
// Note: Social-rating expansions of FactionGlobal effects are applied per-base
// (ExpandSocialRatingEffects) and are NOT visible here — use ResolveFlag(BaseManager) for those.
bool ResolveFlag(const Faction& rFaction, RuleFlagId_t flagId);

// Resolve a RuleFlag from a base's final effect list (FilterForBase + SE rating expansion +
// buildings). Prefer this for ThisBase flags (Headquarters) and for FactionGlobal SE effects
// (e.g. probe_subversion_immune). Context-free: effects carrying a condition are skipped.
bool ResolveFlag(const BaseManager& rBase, RuleFlagId_t flagId);

// True when rUnit has a live PermissionEffect of the given id whose condition is satisfied
// in rCtx. unitFilter is already applied by CollectLiveUnitEffects.
bool HasPermission(const Unit& rUnit, PermissionId_t permission, const EffectContext_t& rCtx);

// True if any of rTile's own features (terrain + improvements) declares flagId as an
// unconditional ThisTile rule flag on the tile itself. Radius auras don't project flags —
// a flag describes the host tile, not its neighbourhood.
bool ResolveFlag(const Tile& rTile, RuleFlagId_t flagId);

// The above, OR a non-embarked unit of factionId standing on rTile whose design projects
// flagId at ThisTile (Carrier Deck). Lets a consumer ask what a tile can do for it without
// naming which improvements or components supply the capability. Note the faction check is
// this function's own: TileEffectsContext's unit auras are deliberately not territory-owned.
bool TileProvidesFlag(const Tile& rTile, RuleFlagId_t flagId, const WorldMap& rWorldMap,
                      FactionId_t factionId);

// Collects a single pop type's own effects (both ThisPop-scoped tile multipliers and
// ThisBase-scoped flat generation bonuses). sourceId is the pop type's id.
std::vector<ActiveEffect_t> CollectPopEffects(const PopTypeConfig_t& rConfig);

// Collects the ThisBase-scoped flat generation effects from every pop in rPops.
// Origin is tagged at AppendActiveEffects time; ThisPop tile multipliers are excluded.
std::vector<ActiveEffect_t> CollectFromPops(const PopulationManager& rPops, const BaseManager& rOriginBase);

// Collects every ThisTile-scoped effect from rTile's own features only: terrain feature
// configs (rockiness, moisture, river, fungus) plus each improvement config held on the tile.
// sourceId is the matching feature's id. Does NOT include aura effects from nearby tiles —
// use TileEffectsContext::CollectAreaEffects for that (which needs WorldMap).
// Never enters the base-wide active effects pool (FilterForBase always excludes ThisTile).
std::vector<ActiveEffect_t> CollectTileEffects(const Tile& rTile);

// Fire all Instantaneous effects declared on rBuilding against rBase.
// GrantBuilding: adds the granted building to the base immediately.
// GrantTech / GrantUnit: logged as TODO stubs until those systems are wired.
// Infiltration: always applies via ApplyInfiltrationEffect (needs a live session GameState).
// Continuous Infiltration is honored at query time via HasInfiltration — no dispatch.
// Call this right after a building is added to the base (e.g. from OnProductionCompleted).
void DispatchInstantaneousEffects(const BuildingConfig_t& rBuilding, BaseManager& rBase,
                                  GameState& rGameState);

} // namespace ac
