#pragma once

#include "game/base/BaseTypes.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
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

class BaseManager;
class Faction;
class GameState;
class IEffectsProvider;
class PopulationManager;
class TerritoryMap;
class Unit;
class IDesign;
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
                   const BaseManager* pOriginBase)
        : config(&rConfig)
        , sourceId(std::move(sourceId))
        , originBase(pOriginBase)
    {
    }

    const EffectConfig_t* config;
    std::string sourceId;           // "command_nexus", "free_market", etc — for breakdown/UI
    // The base an effect is tagged with or stamped from, for ThisBase, ProducedAtThisBase and
    // FactionUnits. Cleared on a unit's production grants.
    const BaseManager* originBase = nullptr;
    // The faction a territory-owned improvement or a unit-projected aura serves; unset applies to
    // everyone. k_NoFactionOwner matches nobody.
    std::optional<FactionId_t> ownerFaction;
    // Set for FactionPair effects: the other faction of the pair. They match only a context
    // whose pPartner is that faction.
    std::optional<FactionId_t> partnerFaction;
};

// Unset ownerFaction applies to all. Detect must also require one: unattributed, it never
// pierces.
inline bool AppliesForFaction(const ActiveEffect_t& rEffect, FactionId_t forFaction)
{
    return !rEffect.ownerFaction.has_value() || *rEffect.ownerFaction == forFaction;
}

// The faction-wide pool, with other factions' WorldGlobal and council extras when bound to a
// session. Holds every base's ThisBase effects, so FilterForBase is the only way to a base.
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

// One base's effects: FilterForBase over the pool plus its pops' and social ratings' ThisBase
// effects. Everything applies to this base.
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

// Live-unit or design-only effect list for Unit-domain resolve. IDesign-only leaves pUnit null
// (preview / intrinsic); amount sources that need a live Unit throw if evaluated then.
struct UnitEffects_t
{
    explicit UnitEffects_t(const Unit& rUnit);
    explicit UnitEffects_t(const IDesign& rDesign);
    UnitEffects_t(const Unit& rUnit, std::vector<ActiveEffect_t> effectsIn);
    UnitEffects_t(const IDesign& rDesign, std::vector<ActiveEffect_t> effectsIn);

    const Unit* pUnit = nullptr;
    const IDesign* pDesign = nullptr;
    std::vector<ActiveEffect_t> effects;
};

// Which side of a fight is resolving stats. None for non-combat resolution.
enum class CombatRole_t
{
    None,
    Attacker,
    Defender,
};

// Stockpile conversion subject for MineralsConverted, named so conversion can grow fields.
struct StockpileConversionSubject_t
{
    int mineralsConverted = 0;
};

// What conditions and amount sources read. A condition on an absent field is false; an amount
// source without its subject is filtered out before resolve.
struct EffectContext_t
{
    const Tile* targetTile = nullptr;
    CombatRole_t combatRole = CombatRole_t::None;
    const Unit* pAttacker = nullptr;
    const Unit* pDefender = nullptr;
    const BaseManager* pBase = nullptr;
    const StockpileConversionSubject_t* pStockpile = nullptr;
    const Faction* pFaction = nullptr;
    const Unit* pUnit = nullptr;
    // The other faction when resolving pFaction's dealings with it (commerce). Admits that
    // pair's FactionPair effects.
    const Faction* pPartner = nullptr;
    // World yield rules for terrain-scaled amount sources. Stamped by TileEffectsContext,
    // which is the only place tile yield is resolved.
    const TileYieldRulesConfig_t* pTileYieldRules = nullptr;
};

// Post-combat promotion uses MoraleConfig_t::promotionSeedFormula (Lua), not amount_source.

// One overload per subject: a new AmountSource_t adds a case only to its subject's overload.
// Dispatch routes every source to its own subject, so the others keep `default:`.
double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const BaseManager& rBase);
double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const Tile& rTile, const TileYieldRulesConfig_t& rYieldRules);
double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const StockpileConversionSubject_t& rStockpile);
double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const Faction& rFaction);
double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const Unit& rUnit);

// The literal amount, or the source's value from the matching subject; a missing subject
// throws. No `default:`, so a new source fails to compile here.
double AmountSourceValue(const StatModifierEffect_t& rMod, const EffectContext_t* pCtx);

// Stamps the faction and unit subjects from a live unit where unset, so unit sources and
// conditions resolve without every call site doing it.
EffectContext_t UnitSubjectContext(const Unit* pUnit, const EffectContext_t& rCtx);

// True if config has no condition or ctx satisfies it. Origin-base conditions need
// pOriginBase.
bool ConditionSatisfied(const EffectConfig_t& config, const EffectContext_t& ctx,
                        const BaseManager* pOriginBase);
bool ConditionSatisfied(const Condition_t& rCondition, const EffectContext_t& ctx,
                        const BaseManager* pOriginBase);

// True when the condition needs subjects beyond pUnit, keeping it out of context-free
// resolution.
bool ConditionNeedsSituationalContext(const Condition_t& rCondition);

struct BuildingConfig_t;

// True if config carries no buildingFilter, or its buildingFilter matches rBuilding
// (All / BuildingId / Category). Used when resolving FacilityEnergyUpkeep per type.
bool BuildingFilterSatisfied(const EffectConfig_t& config, const BuildingConfig_t& rBuilding);

// True unless a PlayerType factionFilter excludes the owning faction.
bool FactionFilterMatchesOwner(const EffectConfig_t& rConfig, bool bPlayerControlled);

// The one config-to-ActiveEffect_t conversion, recording pOriginBase where TagsOriginBase
// says. New sources collect through it or its filtered variants.
void AppendActiveEffects(std::span<const EffectConfig_t> rEffects,
                         const BaseManager* pOriginBase,
                         const std::string& sourceId,
                         std::vector<ActiveEffect_t>& rOut);

// Faction-lane scopes only, for sources whose local scopes resolve elsewhere.
void AppendFactionLaneEffects(std::span<const EffectConfig_t> rEffects,
                              const BaseManager* pOriginBase,
                              const std::string& sourceId,
                              std::vector<ActiveEffect_t>& rOut);

// Base-lane scopes only; with AppendFactionLaneEffects it partitions a config array.
void AppendBaseLaneEffects(std::span<const EffectConfig_t> rEffects,
                           const BaseManager* pOriginBase,
                           const std::string& sourceId,
                           std::vector<ActiveEffect_t>& rOut);

// True if rEffect, a continuous ThisTile effect, reaches `distance` tiles from its host:
// minRadius <= distance <= radius.
bool TileEffectReaches(const EffectConfig_t& rEffect, int distance);

// As AppendActiveEffects, but keeps only effects satisfying TileEffectReaches(e, distance).
void AppendTileEffects(std::span<const EffectConfig_t> rEffects,
                       const std::string& sourceId,
                       int distance,
                       std::vector<ActiveEffect_t>& rOut);

class BuildingRegistry;

// Appends each granted building's effects, attributing ThisBase ones to rBases. Buildings
// already built pre-seed the dedupe set, so a grant does not double-count them.
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

// result = (base + adds) * (1 + percents/100) * geometric; order within a kind does not
// matter.
double ApplyModifierStack(double base, const std::vector<std::pair<double, ModifierOp_t>>& contributions);

// The one float-to-int rule for resolved totals: half away from zero.
inline int FinalizeResolvedStat(double value)
{
    return static_cast<int>(std::lround(value));
}

// Never defaulted: a pure multiplier from a 0 base resolves to 0, so callers state the seed.
// Pass the pCtx the range was filtered with, or nullptr for context-free resolves.
template <std::ranges::input_range Range>
StatBreakdown_t ResolveStatModifiers(Range&& matching, double baseValue,
                                     const EffectContext_t* pCtx)
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
                                 const EffectContext_t* pCtx)
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
        // Conditions and amount sources stay out of context-free resolution.
        return pStatModifier && pStatModifier->stat == statId && !effect.config->condition
            && !pStatModifier->amountSource
            && LaneFor(effect.config->scope) != EffectLane_t::FactionPair;
    });
}
inline auto FilterByStatId(std::vector<ActiveEffect_t>&& effects, StatId_t statId) = delete;

// The one in-context matching rule, shared by every caller so it cannot drift.
// The other unit in a combat pair, or null when pUnit is not one of them.
inline const Unit* OpposingCombatant_(const EffectContext_t& ctx)
{
    if (ctx.pUnit == nullptr || ctx.pAttacker == nullptr || ctx.pDefender == nullptr)
    {
        return nullptr;
    }
    if (ctx.pUnit == ctx.pAttacker)
    {
        return ctx.pDefender;
    }
    if (ctx.pUnit == ctx.pDefender)
    {
        return ctx.pAttacker;
    }
    return nullptr;
}

// Whether a FactionPair effect's partner is ctx.pPartner. False when ctx names no partner.
bool MatchesPartner(const ActiveEffect_t& effect, const EffectContext_t& ctx);

// Drops an amount source this context cannot evaluate, rather than throwing at resolve.
inline bool AmountSourceEvaluable_(const StatModifierEffect_t& rModifier,
                                   const EffectContext_t& ctx)
{
    using Source_t = StatModifierEffect_t::AmountSource_t;
    if (ctx.pBase == nullptr
        && (rModifier.HasAmountSource(Source_t::BaseSize)
            || rModifier.HasAmountSource(Source_t::BuildingUpkeep)))
    {
        return false;
    }
    if (ctx.pFaction == nullptr && rModifier.HasAmountSource(Source_t::BasesOwned))
    {
        return false;
    }
    if (ctx.pUnit == nullptr
        && (rModifier.HasAmountSource(Source_t::IntrinsicXp)
            || rModifier.HasAmountSource(Source_t::SocialRating)))
    {
        return false;
    }
    if ((ctx.targetTile == nullptr || ctx.pTileYieldRules == nullptr)
        && rModifier.HasAmountSource(Source_t::ElevationEnergy))
    {
        return false;
    }
    return !rModifier.HasAmountSource(Source_t::LevelsAboveOpponent)
        || OpposingCombatant_(ctx) != nullptr;
}

inline bool StatModifierMatchesInContext(const ActiveEffect_t& effect, StatId_t statId,
                                         const EffectContext_t& ctx)
{
    const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
    if (!pStatModifier || pStatModifier->stat != statId)
    {
        return false;
    }
    if (LaneFor(effect.config->scope) == EffectLane_t::FactionPair
        && !MatchesPartner(effect, ctx))
    {
        return false;
    }
    if (!AmountSourceEvaluable_(*pStatModifier, ctx))
    {
        return false;
    }
    return ConditionSatisfied(*effect.config, ctx, effect.originBase);
}

// FilterByStatId for a runtime context such as combat: conditions satisfied by ctx pass.
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

// Base-level resolution only, with no per-tile selectors; takes BaseEffects_t so no other
// stage can run it. Amount sources pass on pCtx's subject, which resolve then evaluates.
inline auto FilterBaseLevelByStatId(const BaseEffects_t& rBaseEffects, StatId_t statId,
                                    const EffectContext_t* pCtx)
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
            const auto source = pStatModifier->amountSource->kind;
            if (source != StatModifierEffect_t::AmountSource_t::BaseSize
                && source != StatModifierEffect_t::AmountSource_t::BuildingUpkeep)
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
                                    const EffectContext_t* pCtx) = delete;

// Domain-scoped resolve: asserts DomainFor(stat), filters, and evaluates amount sources
// against the bundle subject. Callers cannot omit the subject or mismatch filter to domain.
double ResolveBaseStat(const BaseEffects_t& rBaseEffects, StatId_t statId, double seed,
                       const EffectContext_t* pCtx);
double ResolveFactionStat(const FactionEffects_t& rFactionEffects, StatId_t statId, double seed,
                          const EffectContext_t* pCtx);
double ResolveUnitStat(const UnitEffects_t& rUnitEffects, StatId_t statId, double seed,
                       const EffectContext_t* pCtx);

// Live-unit Attack or Defense for combat. moraleAddPercent joins the AddPercent bucket.
int ResolveCombatUnitStat(const Unit& rUnit, StatId_t statId, const EffectContext_t& rCtx,
                          std::span<const EffectConfig_t> extraEffects);
int ResolveCombatUnitStat(const Unit& rUnit, StatId_t statId, const EffectContext_t& rCtx,
                          double moraleAddPercent);
double ResolveCombatUnitMultiplicativeStat(const Unit& rUnit, StatId_t statId, double baseValue,
                                           const EffectContext_t& rCtx,
                                           std::span<const EffectConfig_t> extraEffects);
double ResolveCombatUnitMultiplicativeStat(const Unit& rUnit, StatId_t statId, double baseValue,
                                           const EffectContext_t& rCtx, double moraleAddPercent);

// The base's share of the pool: its own ThisBase effects and every faction-wide one. The only
// way to build a BaseEffects_t, and eager so it can be cached.
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


// Resolve a design's intrinsic stats / flags — no faction pool.
// Context-free: effects carrying a condition are skipped (same rule as FilterByStatId).
int ResolveStat(const IDesign& rDesign, StatId_t statId);
int ResolveStat(const IDesign& rDesign, StatId_t statId, const EffectContext_t& rCtx);
bool ResolveFlag(const IDesign& rDesign, RuleFlagId_t flagId);

// Sum of ModifierOp_t::Add contributions only. Ignores AddPercent /
// MultiplyGeometric — the SMAC-style base combat rating (e.g. laser 2, not 2 * 1.25).
int ResolveAdditiveStat(const IDesign& rDesign, StatId_t statId);

// A live unit's effects: design, FactionUnits, WorldGlobal and production grants, already
// filtered by identity conditions. Situational conditions remain.
UnitEffects_t CollectLiveUnitEffects(const Unit& rUnit);

// WorldGlobal stat modifiers in `domain`, and rule flags.
void AppendWorldGlobalEffects(const std::vector<ActiveEffect_t>& rPool, ResolveDomain_t domain,
                              std::vector<ActiveEffect_t>& rOut);

// Resolve a live unit's stats / flags: design effects plus FactionUnits from the owner.
int ResolveStat(const Unit& rUnit, StatId_t statId);
int ResolveStat(const Unit& rUnit, StatId_t statId, const EffectContext_t& rCtx);
// Only AddPercent and MultiplyGeometric, from baseValue: psi combat strengths start at 1.
double ResolveMultiplicativeStat(const Unit& rUnit, StatId_t statId, double baseValue,
                                 const EffectContext_t& rCtx);
// Context-free: effects carrying a condition are skipped (same rule as FilterByStatId).
bool ResolveFlag(const Unit& rUnit, RuleFlagId_t flagId);

// Context-free. Misses social-rating effects, which expand per base: use
// ResolveFlag(BaseManager).
bool ResolveFlag(const Faction& rFaction, RuleFlagId_t flagId);

// From the base's full effect list, social ratings included. Context-free.
bool ResolveFlag(const BaseManager& rBase, RuleFlagId_t flagId);

// True if one of rTile's own features declares the flag; auras do not project flags.
bool ResolveFlag(const Tile& rTile, RuleFlagId_t flagId);

// The above, or a non-embarked unit of factionId on rTile projecting it (Carrier Deck).
bool TileProvidesFlag(const Tile& rTile, RuleFlagId_t flagId, const WorldMap& rWorldMap,
                      FactionId_t factionId);

// True when rTile is owned by factionId and a terrain feature or improvement on it declares
// RuleFlag Harbors for domain (ThisTile, radius 0, no condition).
bool TileHarbors(const Tile& rTile, UnitDomain_t domain, FactionId_t factionId,
                 const TerritoryMap& rTerritory);

// Collects a single pop type's own effects (both ThisPop-scoped tile multipliers and
// ThisBase-scoped flat generation bonuses). sourceId is the pop type's id.
std::vector<ActiveEffect_t> CollectPopEffects(const PopTypeConfig_t& rConfig);

// Collects the ThisBase-scoped flat generation effects from every pop in rPops.
// Origin is tagged at AppendActiveEffects time; ThisPop tile multipliers are excluded.
std::vector<ActiveEffect_t> CollectFromPops(const PopulationManager& rPops, const BaseManager& rOriginBase);

// ThisTile effects from rTile's own terrain and improvements, without auras
// (TileEffectsContext::CollectAreaEffects).
std::vector<ActiveEffect_t> CollectTileEffects(const Tile& rTile);

} // namespace ac
