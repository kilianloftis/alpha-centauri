#pragma once

#include "game/effects/EffectEnums.h"
#include "game/units/UnitDomain.h"

#include <optional>
#include <string>
#include <variant>

namespace ac
{

enum class EffectScope_t
{
    ThisBase,
    AllOwnerBases,
    ThisUnit,
    FactionUnits,
    FactionGlobal,
    WorldGlobal,
    // Only the specific pop instance the effect belongs to. Resolved locally by Pop
    // (e.g. ApplyTileMultipliers) and must never enter the base-wide active effects pool.
    ThisPop,
    // Only the specific tile the effect belongs to (terrain classification, river, fungus,
    // or improvement). Resolved locally via CollectTileEffects/ResolveTileYield/
    // ResolveTileDefenseMultiplier and must never enter the base-wide active effects pool.
    ThisTile,
};

enum class EffectPersistence_t
{
    Instantaneous,
    Continuous,
};

// Where an effect is resolved — its scope's "lane". This is the single source of truth for
// scope routing: collectors and filters derive their decisions from LaneFor instead of
// hand-maintained scope lists. Adding a value to EffectScope_t forces an update to LaneFor's
// exhaustive switch, and every collector/filter picks up the new scope's routing from there.
enum class EffectLane_t
{
    // Resolved by the owning base: lives in the faction pool tagged with originBase,
    // included per base by FilterForBase (pop ThisBase effects merge via CollectFromPops
    // instead, so they never enter the pool).
    Base,
    // Resolved at every base of the faction (WorldGlobal additionally crosses factions via
    // GameState::CollectWorldEffects). Lives in the faction pool; FilterForBase includes it.
    FactionWide,
    // Merged into every live unit's stat resolution. Lives in the faction pool; consumed by
    // Unit::Get* via FilterByScope(FactionUnits), never applies at base level.
    FactionUnits,
    // Resolved by the unit's own design (intrinsic component stats). Never enters the pool.
    UnitLocal,
    // Resolved by the pop itself (tile multipliers). Never enters the pool.
    PopLocal,
    // Resolved by the tile resolvers (CollectTileEffects/CollectAreaEffects). Never enters
    // the pool.
    TileLocal,
};

constexpr EffectLane_t LaneFor(EffectScope_t scope)
{
    switch (scope)
    {
        case EffectScope_t::ThisBase:      return EffectLane_t::Base;
        case EffectScope_t::AllOwnerBases:
        case EffectScope_t::FactionGlobal:
        case EffectScope_t::WorldGlobal:   return EffectLane_t::FactionWide;
        case EffectScope_t::FactionUnits:  return EffectLane_t::FactionUnits;
        case EffectScope_t::ThisUnit:      return EffectLane_t::UnitLocal;
        case EffectScope_t::ThisPop:       return EffectLane_t::PopLocal;
        case EffectScope_t::ThisTile:      return EffectLane_t::TileLocal;
    }
    return EffectLane_t::FactionWide; // unreachable; all enumerators handled above
}

// True for scopes resolved faction-wide through the pool (at bases or units) rather than
// locally by a specific base/pop/unit/tile. This is what Faction's pop/unit collectors feed
// into CollectActiveEffects.
constexpr bool IsFactionLane(EffectScope_t scope)
{
    const EffectLane_t lane = LaneFor(scope);
    return lane == EffectLane_t::FactionWide || lane == EffectLane_t::FactionUnits;
}

enum class ModifierOp_t
{
    Add,
    // amount is in percent points (25 = +25%, -25 = -25%), matching the UI's bonus display.
    // All AddPercent contributions sum into a single arithmetic factor before the geometric step.
    AddPercent,
    MultiplyGeometric
};

struct GrantBuildingEffect_t
{
    std::string buildingId;
};

struct GrantTechEffect_t
{
    std::string techId;
};

struct GrantUnitEffect_t
{
    std::string unitId;
};

enum class TileSelectorKind_t
{
    BaseTile,
    HasImprovement
};

struct TileSelector_t
{
    TileSelectorKind_t kind;
    std::optional<std::string> improvement; // improvement id, set only when kind == HasImprovement
};

struct StatModifierEffect_t
{
    StatId_t stat = StatId_t::Nutrients;
    double amount = 0.0;
    ModifierOp_t op = ModifierOp_t::Add;
    // When set, this modifier is a per-tile yield modifier: it applies to each worked tile
    // whose features satisfy the selector (e.g. "+1 mineral to every worked Mine"), rather
    // than once at the base level. Absent selector: intrinsic tile yield (ThisTile scope) or
    // a flat base modifier (ThisBase scope), depending on the effect's scope.
    std::optional<TileSelector_t> selector;
};

struct RuleFlagEffect_t
{
    RuleFlagId_t flag;
};

struct SocialEngineeringOverrideEffect_t
{
    // TODO: define parameters when social engineering rules are finalized
    std::string category;
    std::string choice;
};

// Modifies one of the ten social engineering rating axes by an integer amount.
// The total accumulated rating for each axis is then looked up in social_rating_effects.json
// to produce the final gameplay effects (non-linear mapping).
struct SocialRatingModifierEffect_t
{
    SocialRatingId_t rating;
    int amount = 0;
};

struct DiplomaticModifierEffect_t
{
    // TODO: define parameters when diplomatic modifier rules are finalized
    std::string targetFactionId;
    int value;
};

// Hides a unit on an arbitrary detection channel (e.g. "cloak", "terrain"). Channel ids
// are free-form strings defined in JSON — not enumerated in code. ThisUnit: the unit itself
// is concealed. ThisTile: every unit standing on (or within radius of) the tile is concealed.
struct ConcealEffect_t
{
    std::string channel;
};

// Pierces concealment on a matching channel. Typically ThisTile with a radius (e.g. Sensor
// Detect "terrain" at radius 2). Territory-owned improvements stamp ActiveEffect_t::ownerFaction.
struct DetectEffect_t
{
    std::string channel;
};

using EffectVariant_t = std::variant<
    GrantBuildingEffect_t,
    GrantTechEffect_t,
    GrantUnitEffect_t,
    StatModifierEffect_t,
    RuleFlagEffect_t,
    SocialEngineeringOverrideEffect_t,
    DiplomaticModifierEffect_t,
    SocialRatingModifierEffect_t,
    ConcealEffect_t,
    DetectEffect_t
>;

enum class ConditionKind_t
{
    // The tile targeted by this effect has the named feature id. Evaluated via
    // Tile::HasFeature, so a single kind covers terrain classification (e.g. "Rocky"),
    // river/fungus, and any improvement id — including "Base", which a
    // founded base registers as an improvement. In combat the target is the defender's tile,
    // so this expresses both "+X% attacking into Forest" and "+X% attacking a Base".
    TargetTileHas,
};

struct Condition_t
{
    ConditionKind_t kind;
    // Parameter for the condition. For TargetTileHas this is the feature id passed to
    // Tile::HasFeature (e.g. "Base", "Forest", "Rocky").
    std::string value;
};

// Restricts which units an effect applies to when merged into a live unit's effect list
// (CollectLiveUnitEffects). Absent = all units. Distinct from condition: filters are
// unit-identity predicates evaluated context-free (domain, component loadout), not combat
// situational predicates.
enum class UnitFilterKind_t
{
    Domain,
    HasComponent,
};

struct UnitFilter_t
{
    UnitFilterKind_t kind;
    // Set when kind == Domain.
    std::optional<UnitDomain_t> domain;
    // Component id; set when kind == HasComponent.
    std::optional<std::string> component;
};

struct EffectConfig_t
{
    EffectVariant_t effect;
    EffectScope_t scope;
    EffectPersistence_t persistence;
    // Absent = the effect always applies. When present, the effect only applies in a runtime
    // context that satisfies the condition (see ConditionSatisfied / EffectContext_t). Such
    // effects are excluded from context-free resolution (base economy, intrinsic unit stats).
    std::optional<Condition_t> condition;
    // Absent = applies to every unit that receives this effect. When present, CollectLiveUnitEffects
    // drops the effect for units that do not match (e.g. Domain=Air for Aerospace Complex).
    std::optional<UnitFilter_t> unitFilter;
    // For ThisTile-scoped effects: how far (Chebyshev tiles) beyond the host tile the effect
    // reaches. 0 (default) = the host tile only. Parsed from the effect entry's "radius" field.
    int radius = 0;
};

// Which kind of config declared an effects array. Used for the minimal load-time scope
// validation: scopes that can only ever be resolved against one source kind (a specific pop,
// a specific unit) are rejected on any other source instead of silently doing nothing.
enum class EffectSourceKind_t
{
    Building,
    UnitComponent,
    PopType,
    Improvement,
    SocialPolicy,
    SocialRating,
    Faction,
};

} // namespace ac
