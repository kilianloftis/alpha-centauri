#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/effects/EffectConfig.h"
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
struct TileYieldRulesConfig_t;

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
    // the host tile's territory at collection time. Also set for unit-projected ThisTile auras
    // from the projecting unit's faction. When set, tile defense / fog vision / area Conceal
    // only apply for that faction (unset = universal, e.g. Rocky defense, Fungus Conceal).
    // Detect requires a stamped owner — see AppliesForFaction vs UnitVisibility's fail-closed
    // Detect gate. Unowned territory stores k_NoFactionOwner (has_value, matches nobody).
    std::optional<FactionId_t> ownerFaction;
};

// General faction gate for attributed tile effects: unset ownerFaction ⇒ applies to all
// (terrain defense / Fungus Conceal). Detect must additionally require has_value at the
// consumer — an unattributed Detect never pierces.
inline bool AppliesForFaction(const ActiveEffect_t& rEffect, FactionId_t forFaction)
{
    return !rEffect.ownerFaction.has_value() || *rEffect.ownerFaction == forFaction;
}

// The faction-wide effect pool: what CollectActiveEffects gathers (buildings with grants
// expanded, social policies, pop/unit faction-lane effects), plus — when the faction is bound
// to a session world source — other factions' WorldGlobal and council extras. Deliberately a
// distinct type from BaseEffects_t: the pool still holds every base's ThisBase effects and the
// FactionUnits lane, so resolving base stats directly against it would count effects that don't
// apply — FilterForBase is the only path from a pool to a base's effect list.
// Subject is constructor-injected so ResolveFactionStat / amount sources cannot omit it.
struct FactionEffects_t
{
    explicit FactionEffects_t(const Faction& rFaction)
        : pFaction(&rFaction)
    {}
    FactionEffects_t(const Faction& rFaction, std::vector<ActiveEffect_t> effectsIn)
        : pFaction(&rFaction)
        , effects(std::move(effectsIn))
    {}

    const Faction* pFaction;
    std::vector<ActiveEffect_t> effects;
};

// One base's effect list: FilterForBase over the faction pool, then the base's pop-generated
// ThisBase effects (CollectFromPops) and expanded social-rating effects
// (ResolveSocialRatingLevelEffects) merged in — see BaseManager::BuildBaseEffects_. Every entry
// applies to that single base, which is the precondition for base-level resolution
// (FilterBaseLevelByStatId) and the per-tile selector pass (TileEffectsContext::ResolveTileYield).
// Subject is constructor-injected so ResolveBaseStat / amount sources cannot omit it.
struct BaseEffects_t
{
    explicit BaseEffects_t(const BaseManager& rBase)
        : pBase(&rBase)
    {}
    BaseEffects_t(const BaseManager& rBase, std::vector<ActiveEffect_t> effectsIn)
        : pBase(&rBase)
        , effects(std::move(effectsIn))
    {}

    const BaseManager* pBase;
    std::vector<ActiveEffect_t> effects;
};

// Live-unit or design-only effect list for Unit-domain resolve. Design-only leaves pUnit null
// (preview / intrinsic); amount sources that need a live Unit throw if evaluated then.
struct UnitEffects_t
{
    explicit UnitEffects_t(const Unit& rUnit);
    explicit UnitEffects_t(const UnitDesign& rDesign);
    UnitEffects_t(const Unit& rUnit, std::vector<ActiveEffect_t> effectsIn);
    UnitEffects_t(const UnitDesign& rDesign, std::vector<ActiveEffect_t> effectsIn);

    const Unit* pUnit = nullptr;
    const UnitDesign* pDesign = nullptr;
    std::vector<ActiveEffect_t> effects;
};

// Which side of a fight is resolving stats. None for non-combat resolution.
enum class CombatRole_t
{
    None,
    Attacker,
    Defender,
};

// Stockpile conversion subject for the MineralsConverted amount_source (not a ResolveDomain
// DomainFor target — resource stats stay Base). A named subject rather than a bare int on
// EffectContext_t: conversion can grow subject fields without widening the context, and the
// subject has one home instead of two.
struct StockpileConversionSubject_t
{
    int mineralsConverted = 0;
};

// Runtime context an effect's condition is evaluated against, and the bag of subjects
// amount_source evaluation reads. Fields are optional; a condition that references an absent
// field evaluates false, and an amount_source whose subject is absent is dropped by the
// filters (StatModifierMatchesInContext / FilterBaseLevelByStatId) before resolve.
// Combat sets targetTile to the defender's tile so TargetTileHas conditions can inspect it.
// Tile yield sets targetTile to the *receiving* tile — for a radius aura that is the tile
// being resolved, not the aura's host — plus pTileYieldRules for amount_source ElevationEnergy.
// combatRole enables IsDefending (SE Morale defense-in-base extras).
// pAttacker enables AttackerIsEmbarked (and future attacker-side conditions).
// pBase enables IsHeadquarters (Economy SE energy-at-HQ) and amount_source BaseSize
// (population size × amount) for base-level resolve.
// pFaction enables amount_source BasesOwned (owned-base count × amount); unit resolve stamps
// it from the live unit via UnitSubjectContext.
// pStockpile carries this turn's consumed minerals for the MineralsConverted amount_source.
struct EffectContext_t
{
    const Tile* targetTile = nullptr;
    CombatRole_t combatRole = CombatRole_t::None;
    const Unit* pAttacker = nullptr;
    const BaseManager* pBase = nullptr;
    const StockpileConversionSubject_t* pStockpile = nullptr;
    const Faction* pFaction = nullptr;
    // World yield rules for terrain-scaled amount sources. Stamped by TileEffectsContext,
    // which is the only place tile yield is resolved.
    const TileYieldRulesConfig_t* pTileYieldRules = nullptr;
};

// Post-combat promotion uses MoraleConfig_t::promotionSeedFormula (Lua), not amount_source.

// Per-subject amount_source evaluation: `scale` is the modifier's `amount`, and the subject
// supplies the runtime value it multiplies. One overload per subject type; a new
// AmountSource_t adds a case to the one overload that owns its subject, not to all four.
// The others keep `default:` — the dispatch below routes each source to its own subject, so
// a wrong-subject call is unreachable and enumerating the rejects would be per-source
// bookkeeping for a branch that never fires.
double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const BaseManager& rBase);
double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const Tile& rTile, const TileYieldRulesConfig_t& rYieldRules);
double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const StockpileConversionSubject_t& rStockpile);
double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const Faction& rFaction);

// Literal `amount` when amountSource is absent; otherwise dispatches to the subject overload
// above using the matching EffectContext_t field. Missing required subject throws (no silent
// 0.0) — the filters drop such modifiers first, so reaching the throw means a resolve path
// admitted a modifier it could not evaluate.
//
// This switch carries no `default:`, so -Werror=switch (see src/CMakeLists.txt) makes a new
// AmountSource_t a compile error here. It is the single place that must name a source's
// subject; ValidateAmountSourceLegality_ in the parser is the single place that must state
// its legality. Two forced edits per source, not one per subject overload.
double AmountSourceValue(const StatModifierEffect_t& rMod, const EffectContext_t* pCtx);

// Fills the faction subject from a live unit when the caller left it unset, so Unit-domain
// amount sources (BasesOwned) resolve without every unit call site remembering to stamp it.
// pUnit null (design-only preview) leaves the subject absent and those modifiers filter out.
EffectContext_t UnitSubjectContext(const Unit* pUnit, const EffectContext_t& rCtx);

// True if config carries no condition, or its condition is satisfied by ctx.
// OriginBaseIsTargetBase requires pOriginBase (ActiveEffect_t::originBase).
bool ConditionSatisfied(const EffectConfig_t& config, const EffectContext_t& ctx,
                        const BaseManager* pOriginBase = nullptr);

// True if config carries no unitFilter, or its unitFilter matches rUnit (Domain /
// HasComponent / HasFlag / IsPrototype / IsCombatUnit). Used by CollectLiveUnitEffects to
// drop FactionUnits (and any other) effects that do not apply to this unit.
bool UnitFilterSatisfied(const EffectConfig_t& config, const Unit& rUnit);

struct BuildingConfig_t;

// True if config carries no buildingFilter, or its buildingFilter matches rBuilding
// (All / BuildingId / Category). Used when resolving FacilityEnergyUpkeep per type.
bool BuildingFilterSatisfied(const EffectConfig_t& config, const BuildingConfig_t& rBuilding);

// True when config has no PlayerType factionFilter, or that filter matches the owning
// faction's player/AI control. Used when injecting continuous effects into a faction pool
// (difficulty). Distinct from FactionFilterCoversTarget (cross-faction targets only).
bool FactionFilterMatchesOwner(const EffectConfig_t& rConfig, bool bPlayerControlled);

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
// Continuous, and minRadius <= distance <= radius. distance 0 = the host tile itself, which
// minRadius 1 excludes (the Echelon Mirror's ring). The single filter for a tile's own
// effects, neighboring improvement/terrain auras, and unit-projected auras.
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
// CostMultiplier, TileDefense) silently resolve to 0 from a 0 base, so every caller must
// state its seed — 0.0 for additive stats, 1.0 for pure multipliers, or a raw value the
// modifiers scale (tile yield, growth rate's 100). Templated on the input range so it
// accepts both an owned vector and a lazy Filter* view below without materializing one.
// pCtx resolves amount_source modifiers; pass nullptr only for context-free resolves, whose
// filters drop every amount_source effect. Whatever pCtx the matching range was filtered with
// must be the pCtx passed here — the filters admit an amount source on the strength of a
// subject this call then has to evaluate against.
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
        contribution.amount = AmountSourceValue(*pStatModifier, pCtx);
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

// Same filter loop and ApplyModifierStack as ResolveStatModifiers, but skips the sorted
// Contribution list — for hot paths that only need .total (e.g. per-tile yield).
template <std::ranges::input_range Range>
double ResolveStatModifiersTotal(Range&& matching, double baseValue,
                                 const EffectContext_t* pCtx = nullptr)
{
    std::vector<std::pair<double, ModifierOp_t>> stack;
    for (const ActiveEffect_t& active : matching)
    {
        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&active.config->effect);
        if (!pStatModifier)
        {
            continue;
        }
        stack.emplace_back(AmountSourceValue(*pStatModifier, pCtx), pStatModifier->op);
    }
    return ApplyModifierStack(baseValue, stack);
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

// The single in-context matching rule: a StatModifier on statId whose condition ctx satisfies.
// Shared by FilterByStatIdInContext and by callers filtering something other than a
// vector<ActiveEffect_t> (e.g. the tile-yield pointer lanes) so the rule cannot drift.
inline bool StatModifierMatchesInContext(const ActiveEffect_t& effect, StatId_t statId,
                                         const EffectContext_t& ctx)
{
    const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
    if (!pStatModifier || pStatModifier->stat != statId)
    {
        return false;
    }
    // Drop an amount_source whose subject this context lacks, rather than letting
    // AmountSourceValue throw on it further down. MineralsConverted is not listed: it only
    // ever resolves on the stockpile path, which builds its own filter.
    if (pStatModifier->amountSource == StatModifierEffect_t::AmountSource_t::BaseSize
        && ctx.pBase == nullptr)
    {
        return false;
    }
    if (pStatModifier->amountSource == StatModifierEffect_t::AmountSource_t::BasesOwned
        && ctx.pFaction == nullptr)
    {
        return false;
    }
    if (pStatModifier->amountSource == StatModifierEffect_t::AmountSource_t::ElevationEnergy
        && (ctx.targetTile == nullptr || ctx.pTileYieldRules == nullptr))
    {
        return false;
    }
    return ConditionSatisfied(*effect.config, ctx, effect.originBase);
}

// Like FilterByStatId, but for a specific runtime context: includes unconditional effects
// plus any condition-carrying effect whose condition is satisfied by ctx. This is the entry
// point for context-dependent resolution such as combat (attack/defense vs a given target).
inline auto FilterByStatIdInContext(const std::vector<ActiveEffect_t>& effects,
                                    StatId_t statId, const EffectContext_t& ctx)
{
    return effects | std::views::filter([statId, ctx](const ActiveEffect_t& effect)
    {
        return StatModifierMatchesInContext(effect, statId, ctx);
    });
}
inline auto FilterByStatIdInContext(std::vector<ActiveEffect_t>&& effects,
                                    StatId_t statId, const EffectContext_t& ctx) = delete;

// Like FilterByStatId, but for base-level resolution only: excludes per-tile modifiers
// (StatModifiers carrying a tile selector). ElevationEnergySeed / MineralsConverted
// amount_sources stay off this filter (tile yield / stockpile paths).
// Accepting BaseEffects_t (never a raw vector or the pool) makes running this filter at any
// other stage a compile error instead of a doc violation.
//
// BaseSize is admitted only when pCtx->pBase is set — deliberately keyed on the context and
// NOT on the bundle's own subject, because the same pCtx is what ResolveStatModifiers later
// evaluates the amount source against. Admitting on the bundle while resolving on the context
// is what let a base-level BaseSize modifier pass the filter and then throw at resolve.
// Prefer ResolveBaseStat, which stamps pBase from the bundle into both.
inline auto FilterBaseLevelByStatId(const BaseEffects_t& rBaseEffects, StatId_t statId,
                                    const EffectContext_t* pCtx = nullptr)
{
    return rBaseEffects.effects
           | std::views::filter([statId, pCtx](const ActiveEffect_t& effect)
    {
        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        if (!pStatModifier || pStatModifier->stat != statId || pStatModifier->selector)
        {
            return false;
        }
        if (pStatModifier->amountSource)
        {
            if (*pStatModifier->amountSource != StatModifierEffect_t::AmountSource_t::BaseSize)
            {
                return false;
            }
            if (pCtx == nullptr || pCtx->pBase == nullptr)
            {
                return false;
            }
        }
        if (!effect.config->condition)
        {
            return true;
        }
        return pCtx != nullptr
            && ConditionSatisfied(*effect.config, *pCtx, effect.originBase);
    });
}
inline auto FilterBaseLevelByStatId(BaseEffects_t&& rBaseEffects, StatId_t statId,
                                    const EffectContext_t* pCtx = nullptr) = delete;

// Domain-scoped resolve: asserts DomainFor(stat), filters, and evaluates amount sources
// against the bundle subject. Callers cannot omit the subject or mismatch filter to domain.
double ResolveBaseStat(const BaseEffects_t& rBaseEffects, StatId_t statId, double seed,
                       const EffectContext_t* pCtx = nullptr);
double ResolveFactionStat(const FactionEffects_t& rFactionEffects, StatId_t statId, double seed,
                          const EffectContext_t* pCtx = nullptr);
double ResolveUnitStat(const UnitEffects_t& rUnitEffects, StatId_t statId, double seed,
                       const EffectContext_t* pCtx = nullptr);

// Live-unit Attack/Defense for combat: same Unit-domain resolve as ResolveUnitStat /
// ResolveStat (amount sources included), then folds moraleLevelEffects (from the unit's
// effective MoraleLevel_t) into the same stack. Morale Attack/Defense AddPercents must
// share the AddPercent bucket with other unit modifiers — not a post-multiply.
int ResolveCombatUnitStat(const Unit& rUnit, StatId_t statId, const EffectContext_t& rCtx,
                          std::span<const EffectConfig_t> moraleLevelEffects);
double ResolveCombatUnitMultiplicativeStat(const Unit& rUnit, StatId_t statId, double baseValue,
                                           const EffectContext_t& rCtx,
                                           std::span<const EffectConfig_t> moraleLevelEffects);

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

// Design-only UnitEffects_t (pUnit null) from a design's components.
UnitEffects_t CollectUnitEffects(const UnitDesign& rDesign);

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
UnitEffects_t CollectLiveUnitEffects(const Unit& rUnit);

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
// (ResolveSocialRatingLevelEffects) and are NOT visible here — use ResolveFlag(BaseManager).
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
// naming which improvements or components supply the capability. Flags use this function's
// on-tile unit faction check; projected auras stamp ownerFaction from the unit separately.
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

// Apply one Instantaneous ModifyPopulation mutation. Returns the signed size delta actually
// applied (negative when pops were removed). Honors minSize when shrinking; stops adding when
// CanGrow() is false.
int ApplyModifyPopulation(BaseManager& rBase, const ModifyPopulationEffect_t& rEffect);

// Signed size delta for one ModifyPopulation against a base of `size` (no mutation). The one
// place the op is interpreted — ApplyModifyPopulation applies exactly this. Shrinks honor
// minSize; growth is uncapped here (only the applying side knows CanGrow, and the completion
// gates below only care about emptying).
int PredictModifyPopulationDelta(int size, const ModifyPopulationEffect_t& rEffect);

// Final population size after applying every Instantaneous ModifyPopulation in order.
int PredictInstantaneousPopulationSize(std::span<const EffectConfig_t> rEffects, int size);

// Same for a unit design's filled components (production Instantaneous costs).
int PredictUnitProductionPopulationSize(const UnitDesign& rDesign, int size);

// Fire all Instantaneous effects in rEffects against rBase.
// GrantBuilding: adds the granted building to the base immediately.
// GrantTech / GrantUnit: logged as TODO stubs until those systems are wired.
// Infiltration: always applies via ApplyInfiltrationEffect (needs a live session GameState).
// ModifyPopulation: ApplyModifyPopulation.
// Continuous Infiltration is honored at query time via HasInfiltration — no dispatch.
void DispatchInstantaneousEffects(std::span<const EffectConfig_t> rEffects, BaseManager& rBase,
                                  GameState& rGameState);

// Call right after a building is added to the base (e.g. from OnProductionCompleted).
void DispatchInstantaneousEffects(const BuildingConfig_t& rBuilding, BaseManager& rBase,
                                  GameState& rGameState);

// Instantaneous effects on a design's filled components (e.g. colony-pod pop cost). Call right
// after CreateUnit on production complete — not from CreateUnit itself (escape pods / starting
// units must not pay production Instantaneous costs).
void DispatchInstantaneousEffects(const UnitDesign& rDesign, BaseManager& rBase,
                                  GameState& rGameState);

} // namespace ac
