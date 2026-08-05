#pragma once

#include "game/effects/EffectEnums.h"
#include "game/units/UnitDomain.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

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
    // Units produced at the originating base (Unit::GetProducedAtBase). Distinct from home
    // base and from FactionUnits: train-at-this-base bonuses (Command Center, Aerospace).
    ProducedAtThisBase,
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
    // GameState::CollectWorldExtras / Faction composition). Lives in the faction pool;
    // FilterForBase includes it.
    FactionWide,
    // Merged into every live unit's stat resolution. Lives in the faction pool; consumed by
    // Unit::Get* via FilterByScope(FactionUnits), never applies at base level.
    FactionUnits,
    // Merged into live units whose production base matches originBase. Lives in the faction
    // pool tagged with originBase; never applies at base level.
    ProducedAtBase,
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
        case EffectScope_t::ProducedAtThisBase: return EffectLane_t::ProducedAtBase;
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

// Instantaneous treasury credit. Council proposals (Salvage Unity Fusion Core) dispatch this
// via PlanetaryCouncil rather than the building construction path.
struct GrantEnergyEffect_t
{
    int amount = 0;
};

// Instantaneous world-state mutation applied by PlanetaryCouncil (sea level / climate).
enum class WorldParameterId_t
{
    SeaLevel,
};

struct WorldParameterEffect_t
{
    WorldParameterId_t parameter = WorldParameterId_t::SeaLevel;
    // Signed delta applied when the effect fires (negative = cooling / falling seas).
    int amount = 0;
};

// Directed datalink infiltration. The beneficiary (effect owner / acting faction) gains
// visibility into other factions. Target set is scope + optional factionFilter:
//   WorldGlobal, no filter     → every other faction
//   FactionGlobal + CouncilMembers / ActionTarget → filter selects targets
// Instantaneous: written to DiplomacyLedger at apply time.
// Continuous: honored at query time (HasInfiltration) while the effect remains active.
struct InfiltrationEffect_t
{
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
    // When set, `amount` scales a runtime tile value instead of being a literal add.
    // ElevationEnergySeed: contribution = GetElevationEnergySeed() * amount (per-band scale).
    // Requires EffectContext_t::targetTile at resolve time; excluded from context-free filters.
    enum class AmountSource_t
    {
        ElevationEnergySeed,
    };
    std::optional<AmountSource_t> amountSource;
    // When set, this modifier is a per-tile yield modifier: it applies to each worked tile
    // whose features satisfy the selector (e.g. "+1 mineral to every worked Mine"), rather
    // than once at the base level. Absent selector: intrinsic tile yield (ThisTile scope) or
    // a flat base modifier (ThisBase scope), depending on the effect's scope.
    std::optional<TileSelector_t> selector;
    // When true, this contribution is added after per-tile resource caps (classic SMAC
    // resource-bonus specials). Only valid on nutrients/minerals/energy.
    bool applyAfterRestriction = false;
};

// Caps one tile resource at `max`. Lift the cap by putting `removed_by_tech` on the
// EffectConfig_t (FactionEffectsPool drops it once that tech is discovered).
struct TileResourceCapEffect_t
{
    StatId_t stat = StatId_t::Nutrients;
    int max = 2;
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

// Player/AI-initiated attack against another faction's orbital buildings (ASAT).
// Each ready building copy with this effect is one charge; attempting deploys the source.
struct OrbitalAttackEffect_t
{
    // Success percent points (50 = 50%).
    int chance = 50;
    // Intervening mission years the source stays deployed after an attempt.
    // Ready when missionYear >= deployYear + cooldownTurns + 1 (default 1 → ready at Y+2).
    int cooldownTurns = 1;
    // Percent chance the attacking satellite is destroyed when the attempt fails (miss).
    int chanceOfDestructionOnFail = 0;
};

// Generic pre-combat intercept: before Resolve, roll to destroy the attacker.
// unitFilter (on EffectConfig_t) selects eligible attackers; condition gates the situation.
// Optional cooldownTurns: when >= 0, attempting deploys the source (shared with OrbitalAttack
// when both live on the same building id).
struct InterceptAttemptEffect_t
{
    int chance = 50;
    // -1 = no deploy cooldown (may attempt every attack). >= 0 uses the same ready-year formula
    // as OrbitalAttackEffect_t::cooldownTurns.
    int cooldownTurns = -1;
    // Percent chance the intercepting source is destroyed when the attempt fails (miss).
    int chanceOfDestructionOnFail = 0;
};

// Declares which passenger domains this unit may carry, and optionally where it may load.
// Capacity remains cargo_capacity. Contributions union across matching ThisUnit effects
// (see unitFilter for carrier domain).
struct TransportParamsEffect_t
{
    std::vector<UnitDomain_t> passengerDomains;
    // Tile capabilities required to load or unload cargo, ORed together (and across every
    // TransportParams on the carrier). Empty means load anywhere. These name capabilities,
    // not sites: a tile qualifies when an improvement on it or a friendly unit standing on
    // it declares the flag (TileProvidesFlag), so a new load site — an improvement, a
    // carrier deck, a future variant of one — participates by declaring the capability
    // without any carrier's config changing.
    std::vector<RuleFlagId_t> loadSiteFlags;
};

enum class PermissionId_t
{
    // Lifts a channel-crossing attack that reachability already allows.
    Attack,
    // Lifts land -> water entry onto a qualifying tile (sea base).
    Enter,
};

// Grants a capability the rules otherwise deny. Enter almost always carries a condition
// selecting which tiles; Attack on stock pods is unconditional (any channel cross).
struct PermissionEffect_t
{
    PermissionId_t permission = PermissionId_t::Attack;
};

using EffectVariant_t = std::variant<
    GrantBuildingEffect_t,
    GrantTechEffect_t,
    GrantUnitEffect_t,
    GrantEnergyEffect_t,
    WorldParameterEffect_t,
    InfiltrationEffect_t,
    StatModifierEffect_t,
    TileResourceCapEffect_t,
    RuleFlagEffect_t,
    SocialEngineeringOverrideEffect_t,
    DiplomaticModifierEffect_t,
    SocialRatingModifierEffect_t,
    ConcealEffect_t,
    DetectEffect_t,
    OrbitalAttackEffect_t,
    InterceptAttemptEffect_t,
    TransportParamsEffect_t,
    PermissionEffect_t
>;

enum class ConditionKind_t
{
    // The tile targeted by this effect has the named feature id. Evaluated via
    // Tile::HasFeature, so a single kind covers terrain classification (e.g. "Rocky"),
    // river/fungus, and any improvement id — including "Base", which a
    // founded base registers as an improvement. In combat the target is the defender's tile,
    // so this expresses both "+X% attacking into Forest" and "+X% attacking a Base".
    TargetTileHas,
    // Every feature id in `values` is present on the target tile (AND of TargetTileHas),
    // and/or every nested condition in `conditions` is satisfied.
    AllOf,
    // True when EffectContext_t::combatRole is Defender (defense-only SE Morale extras).
    IsDefending,
    // True when ActiveEffect_t::originBase is the base sitting on EffectContext_t::targetTile
    // (Creche combat bonus for the base being defended, not the unit's home).
    OriginBaseIsTargetBase,
    // True when EffectContext_t::pAttacker is non-null and embarked.
    AttackerIsEmbarked,
};

struct Condition_t
{
    ConditionKind_t kind;
    // Parameter for TargetTileHas: feature id passed to Tile::HasFeature.
    std::string value;
    // Parameter for AllOf: every id must be present on the target tile.
    std::vector<std::string> values;
    // Parameter for AllOf: nested conditions (e.g. IsDefending + TargetTileHas Base).
    std::vector<Condition_t> conditions;
};

// Restricts which units an effect applies to when merged into a live unit's effect list
// (CollectLiveUnitEffects). Absent = all units. Distinct from condition: filters are
// unit-identity predicates evaluated context-free (domain, component loadout), not combat
// situational predicates.
enum class UnitFilterKind_t
{
    Domain,
    HasComponent,
    // Unit resolves true for the named RuleFlag (design + FactionUnits).
    HasFlag,
};

struct UnitFilter_t
{
    UnitFilterKind_t kind;
    // Set when kind == Domain.
    std::optional<UnitDomain_t> domain;
    // Component id; set when kind == HasComponent.
    std::optional<std::string> component;
    // Set when kind == HasFlag.
    std::optional<RuleFlagId_t> flag;
};

// Restricts which *other* factions a cross-faction effect (Infiltration, future
// DiplomaticModifier, …) applies to. Orthogonal to EffectScope_t: scope is the resolution
// lane (FactionGlobal / WorldGlobal); factionFilter narrows the diplomatic target set.
// Absent + WorldGlobal → every other faction. Absent + other scopes → no automatic targets.
enum class FactionFilterKind_t
{
    // Only the faction supplied as actionTarget at apply/query time (probe mission target).
    ActionTarget,
    // Other factions that sit on the Planetary Council (or participatesInCouncil if none).
    CouncilMembers,
};

struct FactionFilter_t
{
    FactionFilterKind_t kind = FactionFilterKind_t::ActionTarget;
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
    // Absent = default target set from scope (WorldGlobal → all other factions). When present,
    // further restricts which factions a directed cross-faction effect applies to.
    std::optional<FactionFilter_t> factionFilter;
    // When set, FactionEffectsPool omits this effect once the faction has discovered the tech.
    // Empty / absent = never removed by research. Parsed from the effect entry's
    // "removed_by_tech" field (alongside condition / unitFilter).
    std::string removedByTech;
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
    CouncilProposal,
    ProbeAction,
};

} // namespace ac
