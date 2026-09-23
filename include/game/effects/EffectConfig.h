#pragma once

#include "game/effects/EffectEnums.h"
#include "game/effects/InteractionGridsConfig.h"
#include "game/GameCategory.h"
#include "game/units/UnitDomain.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

// Terminal `else` marker for exhaustive std::visit over the sum types below. The `-Wswitch`
// /`-Werror=switch` pair that guards this project's enum switches has no variant equivalent,
// so every visitor ends in `else { static_assert(k_AlwaysFalse<T>); }` — adding an
// alternative then breaks the build at each site that must decide about it, instead of
// falling off a non-void lambda (or silently doing nothing in a void one).
template <typename T>
inline constexpr bool k_AlwaysFalse = false;

// Expands the target's effects onto this source only — no constructed copy, no upkeep. To
// actually build the facility, use the triggered AddBuildingEffect_t.
struct GrantBuildingEffect_t
{
    std::string buildingId;
};

// Directed datalink infiltration as a standing law: the beneficiary (effect owner) sees into
// other factions for as long as this effect remains active, honored at query time by
// HasInfiltration. Target set is scope + optional factionFilter:
//   WorldGlobal, no filter                        → every other faction
//   FactionGlobal + CouncilMembers / PlayerType   → filter selects targets
// For a one-shot write that outlives its source, use the triggered SetInfiltrationEffect_t.
struct InfiltrationEffect_t
{
};

// Which worked tiles a selector-carrying StatModifier applies to. Sum type so BaseTile vs
// HasImprovement vs AnyTile cannot carry mismatched fields.
struct TileSelectorBaseTile_t
{
};

struct TileSelectorHasImprovement_t
{
    std::string improvement; // feature id matched via Tile::HasFeature
};

// Matches every worked tile (and the free base-center tile). Used by Economy SE ≥ 2
// ("+1 energy each square").
struct TileSelectorAnyTile_t
{
};

using TileSelector_t = std::variant<TileSelectorBaseTile_t, TileSelectorHasImprovement_t,
                                    TileSelectorAnyTile_t>;

struct StatModifierEffect_t
{
    StatId_t stat = StatId_t::Nutrients;
    double amount = 0.0;
    ModifierOp_t op = ModifierOp_t::Add;
    // When set, `amount` scales a runtime value instead of being a literal add.
    // ElevationEnergy: contribution = ceil(tile elevation / elevation_energy_step_meters)
    //   * amount, clamped at 0. This is the whole of a solar collector's yield — a bare tile
    //   produces no energy of its own. Requires EffectContext_t::targetTile and
    //   pTileYieldRules; energy + ThisTile only. Not in FilterBaseLevel.
    // MineralsConverted: contribution = mineralsConverted * amount (output per mineral).
    //   Requires EffectContext_t::mineralsConverted; stockpile-output stats + ThisBase only.
    //   Resolved only during stockpile conversion, not FilterBaseLevelByStatId.
    // BaseSize: contribution = base population size * amount (e.g. University drones 0.25).
    //   Requires EffectContext_t::pBase; included in FilterBaseLevelByStatId when pBase is set.
    // BasesOwned: contribution = faction base count * amount (e.g. +1 Attack per owned base).
    //   Requires EffectContext_t::pFaction (stamped from the live unit on Unit resolve);
    //   Unit-domain stats + ThisUnit only. IDesign-only resolve drops it (no faction subject).
    // IntrinsicXp: contribution = unit intrinsic XP * amount (e.g. Isle cargo 1×XP).
    //   Requires EffectContext_t::pUnit; Unit-domain stats + ThisUnit only. IDesign-only
    //   resolve drops it (no unit subject). Uses GetXp(), not SE-shifted effective morale.
    // BuildingUpkeep: contribution = base facility upkeep * amount (e.g. MaxClamp econ at
    //   upkeep). Requires EffectContext_t::pBase; Continuous MaxClamp on econ only.
    enum class AmountSource_t
    {
        ElevationEnergy,
        MineralsConverted,
        BaseSize,
        BasesOwned,
        IntrinsicXp,
        BuildingUpkeep,
    };
    std::optional<AmountSource_t> amountSource;
    // When set, this modifier is a per-tile yield modifier: it applies to each worked tile
    // whose features satisfy the selector (e.g. "+1 mineral to every worked Mine"), rather
    // than once at the base level. Absent selector: intrinsic tile yield (ThisTile scope) or
    // a flat base modifier (ThisBase scope), depending on the effect's scope.
    std::optional<TileSelector_t> selector;
    // When true, this contribution is added after per-tile MaxClamp / MinClamp (classic SMAC
    // resource-bonus specials). Only valid on nutrients/minerals/energy with op Add.
    bool bypassClamp = false;
};

// Stats a MineralsConverted modifier may target.
//
// Minerals are excluded because they are the input — converting minerals into minerals is a
// feedback loop, not a recipe. Everything else here is a per-turn base bank a stockpile can
// credit directly, except Energy: "energy" at a base is not a bank, so converted energy is
// run through inefficiency and the econ/labs/psych slider split exactly like collected
// energy (ResourceManager::AddAllocatedEnergy). Crediting `econ` instead skips the sliders
// and reaches the treasury whole — IncomeCollection runs after BaseProduction.
inline constexpr StatId_t k_StockpileOutputStats[] = {
    StatId_t::Nutrients,
    StatId_t::Energy,
    StatId_t::Econ,
    StatId_t::Labs,
    StatId_t::Psych,
};

inline bool IsStockpileOutputStat(StatId_t stat)
{
    return std::find(std::begin(k_StockpileOutputStats), std::end(k_StockpileOutputStats), stat)
           != std::end(k_StockpileOutputStats);
}

struct RuleFlagEffect_t
{
    RuleFlagId_t flag;
    std::optional<UnitDomain_t> domain;
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
// Detect "terrain" at radius 2). Collectors must stamp ActiveEffect_t::ownerFaction
// (territory owner or projecting unit); Detect without attribution never pierces.
struct DetectEffect_t
{
    std::string channel;
};

// Player/AI-initiated attack against another faction's orbital buildings (ASAT).
// Each ready building copy with this effect is one charge; attempting deploys the source.
struct OrbitalAttackEffect_t
{
    // Success percent points (50 = 50%). Required in JSON; set explicitly when hand-building.
    int chance;
    // Intervening mission years the source stays deployed after an attempt.
    // Ready when missionYear >= deployYear + cooldownTurns + 1 (e.g. 1 → ready at Y+2).
    // Required in JSON; set explicitly when hand-building.
    int cooldownTurns;
    // Percent chance the attacking satellite is destroyed when the attempt fails (miss).
    // Optional in JSON (default 0).
    int chanceOfDestructionOnFail = 0;
};

// Generic pre-combat intercept: before Resolve, roll to destroy the attacker.
// condition (typically AttackerDomain) selects eligible attackers and gates the situation.
// Optional cooldownTurns: when >= 0, attempting deploys the source (shared with OrbitalAttack
// when both live on the same building id).
struct InterceptEffect_t
{
    // Success percent points. Required in JSON; set explicitly when hand-building.
    int chance;
    // -1 = no deploy cooldown (may attempt every attack). >= 0 uses the same ready-year formula
    // as OrbitalAttackEffect_t::cooldownTurns. Omitted in JSON → -1.
    int cooldownTurns = -1;
    // Percent chance the intercepting source is destroyed when the attempt fails (miss).
    // Optional in JSON (default 0).
    int chanceOfDestructionOnFail = 0;
};

// This unit may scramble to become the combat defender against an attacker that satisfies
// condition (required; typically AttackerDomain). `range` is Chebyshev tiles from the
// candidate to the original defender's tile; eligibility and move-onto-tile live in
// ScrambleRules.
struct ScrambleEffect_t
{
    // Required in JSON; set explicitly when hand-building. Must be > 0.
    int range;
};

// Declares which passenger domains this unit may carry (`carries`), and whether loading
// requires a matching harbor tile (`requires_harbor`). Embarked cargo of a carried domain
// refuels on the carrier — that follows from carries, not a separate flag. Capacity remains
// cargo_capacity. Contributions union across matching ThisUnit effects (see condition for
// carrier SubjectDomain).
struct TransportParamsEffect_t
{
    std::vector<UnitDomain_t> carries;
    bool requiresHarbor = false;
};

// Overrides one cell (or a wild-card slice) of an interaction grid to a non-default value.
// `cell` is required and may be allow or deny; at resolve, an override that restates the
// stock cell is skipped, so for any concrete query only one polarity can fire and first-match
// order among overrides never matters. Omit an axis to match any value on that axis.
// Optional EffectConfig_t::condition still applies (e.g. Water+Base). actorDomain is the row
// on every grid; the parser rejects a column axis that does not belong to the selected grid,
// and rejects any scope other than ThisUnit / FactionUnits, so typos fail at load rather
// than silently never matching.
struct InteractionOverrideEffect_t
{
    InteractionGridId_t grid = InteractionGridId_t::Enter;
    InteractionCell_t cell = InteractionCell_t::Allow;
    std::optional<UnitDomain_t> actorDomain;             // row, every grid
    std::optional<InteractionSurface_t> surface;         // Enter column
    std::optional<InteractionFooting_t> footing;         // AttackTile column
    std::optional<UnitDomain_t> targetDomain;            // AttackUnit / Zoc column
};

using EffectVariant_t = std::variant<
    GrantBuildingEffect_t,
    InfiltrationEffect_t,
    StatModifierEffect_t,
    RuleFlagEffect_t,
    SocialEngineeringOverrideEffect_t,
    DiplomaticModifierEffect_t,
    SocialRatingModifierEffect_t,
    ConcealEffect_t,
    DetectEffect_t,
    OrbitalAttackEffect_t,
    InterceptEffect_t,
    ScrambleEffect_t,
    TransportParamsEffect_t,
    InteractionOverrideEffect_t
>;

// Runtime predicates on EffectConfig_t. Sum type so kind/parameter mismatches are
// unrepresentable. Inherits std::variant so AllOf_t can recurse (vector<Condition_t>).
// Visit/get via AsVariant() — std::visit requires the std::variant specialization.
struct Condition_t;

// The tile targeted by this effect has the named feature id. Evaluated via Tile::HasFeature,
// so one alternative covers terrain classification (e.g. "Rocky"), river/fungus, and any
// improvement id — including "Base". In combat the target is the defender's tile.
struct TargetTileHas_t
{
    std::string featureId;
};

// Every nested condition is satisfied (AND). Parser desugars AllOf JSON `"values": ["A","B"]`
// into TargetTileHas alternatives; after parse only nested Condition_t nodes remain. An empty
// conditions list is the one invalid state the variant cannot rule out: the parser rejects it,
// and ConditionBodySatisfied_ evaluates it as false for hand-built structs.
struct AllOf_t
{
    std::vector<Condition_t> conditions;
};

// True when EffectContext_t::combatRole is Defender (defense-only SE Morale extras).
struct IsDefending_t
{
};


// True when ActiveEffect_t::originBase is the base sitting on EffectContext_t::targetTile
// (Creche combat bonus for the base being defended, not the unit's home).
struct OriginBaseIsTargetBase_t
{
};

// True when ActiveEffect_t::originBase is EffectContext_t::pUnit's home base
// (home-base aura, distinct from OriginBaseIsTargetBase which keys on the combat tile).
struct OriginBaseIsHomeBase_t
{
};

// True when EffectContext_t::pAttacker is non-null and embarked.
struct AttackerIsEmbarked_t
{
};

// True when the unit subject (pUnit, else pAttacker) airdropped this turn — post-drop
// attack penalty on Drop Pods.
struct HasAirdroppedThisTurn_t
{
};

// True when EffectContext_t::pAttacker is non-null and its domain is in `domains`
// (e.g. AAA Tracking vs air / orbital attackers). Empty is rejected by the parser.
struct AttackerDomain_t
{
    std::vector<UnitDomain_t> domains;
};

// True when EffectContext_t::pDefender is non-null and its domain is in `domains`
// (e.g. Air Superiority attack vs air / orbital). Empty is rejected by the parser.
struct DefenderDomain_t
{
    std::vector<UnitDomain_t> domains;
};

// True when EffectContext_t::pBase has the Headquarters rule flag (Economy SE −1
// energy-at-HQ). Requires pBase in the resolve context.
struct IsHeadquarters_t
{
};

// Identity predicates on EffectContext_t::pUnit — the *effect subject* (the unit carrying
// or receiving the effect: CollectLiveUnitEffects carrier, GrantXp produced unit, stamps).
// Fail closed if pUnit is absent. Distinct from AttackerDomain / DefenderDomain (combat roles).
struct SubjectDomain_t
{
    UnitDomain_t domain = UnitDomain_t::Land;
};

struct HasComponent_t
{
    std::string component;
};

struct HasFlag_t
{
    RuleFlagId_t flag = RuleFlagId_t::Flight;
};

// True when the unit was created carrying a component its faction had never fielded.
// Reads Unit::IsPrototype, latched at construction — not a live ledger read.
struct IsPrototype_t
{
};

// True when UnitDesign::IsCombatUnit (additive Attack > 0 or ForcesPsiCombat). IDesign-only
// flag resolve so FactionUnits collection stays recursion-safe.
struct IsCombatUnit_t
{
};

struct Condition_t : std::variant<TargetTileHas_t, AllOf_t, IsDefending_t,
                                  OriginBaseIsTargetBase_t, OriginBaseIsHomeBase_t,
                                  AttackerIsEmbarked_t, HasAirdroppedThisTurn_t,
                                  AttackerDomain_t, DefenderDomain_t, IsHeadquarters_t,
                                  SubjectDomain_t, HasComponent_t, HasFlag_t, IsPrototype_t,
                                  IsCombatUnit_t>
{
    using Variant = std::variant<TargetTileHas_t, AllOf_t, IsDefending_t,
                                 OriginBaseIsTargetBase_t, OriginBaseIsHomeBase_t,
                                 AttackerIsEmbarked_t, HasAirdroppedThisTurn_t,
                                 AttackerDomain_t, DefenderDomain_t, IsHeadquarters_t,
                                 SubjectDomain_t, HasComponent_t, HasFlag_t, IsPrototype_t,
                                 IsCombatUnit_t>;
    using Variant::Variant;
    using Variant::operator=;

    Variant& AsVariant() & { return *this; }
    const Variant& AsVariant() const & { return *this; }
};

// Restricts which buildings a FacilityEnergyUpkeep (or similar) modifier applies to.
// Absent buildingFilter = all buildings.
struct BuildingFilterAll_t
{
};

struct BuildingFilterId_t
{
    std::string buildingId;
};

struct BuildingFilterCategory_t
{
    GameCategory_t category = GameCategory_t::Build;
};

using BuildingFilter_t =
    std::variant<BuildingFilterAll_t, BuildingFilterId_t, BuildingFilterCategory_t>;

// Restricts which *other* factions a cross-faction effect applies to (see FactionFilterKind_t).
// Absent + WorldGlobal → every other faction. Absent + other scopes → no automatic targets.
struct FactionFilter_t
{
    FactionFilterKind_t kind = FactionFilterKind_t::ActionTarget;
    // Only valid when kind == PlayerType: whether the filter matches Player-controlled
    // factions or AI-controlled factions.
    PlayerType_t playerType = PlayerType_t::Player;
};

struct EffectConfig_t
{
    EffectVariant_t effect;
    EffectScope_t scope;
    // Absent = the effect always applies. When present, the effect only applies in a runtime
    // context that satisfies the condition (see ConditionSatisfied / EffectContext_t).
    // Situational arms (TargetTileHas, IsDefending, …) are excluded from context-free
    // resolution; identity arms (Domain, IsPrototype, …) are applied in CollectLiveUnitEffects
    // / in-context resolve when pUnit is set.
    std::optional<Condition_t> condition;
    // Absent = applies to every building when resolving FacilityEnergyUpkeep. When present,
    // only matching building types receive the modifier (All / BuildingId / Category).
    std::optional<BuildingFilter_t> buildingFilter;
    // Absent = default target set from scope (WorldGlobal → all other factions). When present,
    // further restricts which factions a directed cross-faction effect applies to.
    std::optional<FactionFilter_t> factionFilter;
    // When set, FactionEffectsPool omits this effect once the faction has discovered the tech.
    // Empty / absent = never removed by research. Parsed from the effect entry's
    // "removed_by_tech" field (alongside condition / buildingFilter).
    std::string removedByTech;
    // For ThisTile-scoped effects: how far (Chebyshev tiles) beyond the host tile the effect
    // reaches. 0 (default) = the host tile only. Parsed from the effect entry's "radius" field.
    int radius = 0;
    // Nearest Chebyshev distance the effect reaches, so an aura can skip its own host tile.
    // 0 (default) = includes the host. 1 with radius 1 is a ring: the Echelon Mirror boosts
    // neighbouring solar collectors but not the one it counts as itself. Parsed from
    // "min_radius"; must be <= radius.
    int minRadius = 0;
};

} // namespace ac
