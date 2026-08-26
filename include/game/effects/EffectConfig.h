#pragma once

#include "game/effects/EffectEnums.h"
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
    //   Unit-domain stats + ThisUnit only. Design-only resolve drops it (no faction subject).
    enum class AmountSource_t
    {
        ElevationEnergy,
        MineralsConverted,
        BaseSize,
        BasesOwned,
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

// Stats a MineralsConverted modifier may target.
//
// Minerals are excluded because they are the input — converting minerals into minerals is a
// feedback loop, not a recipe. Everything else here is a per-turn base bank a stockpile can
// credit directly, except Energy: "energy" at a base is not a bank, so converted energy is
// run through inefficiency and the econ/labs/psych slider split exactly like collected
// energy (ResourceManager::AddAllocatedEnergy). Crediting `econ` instead skips the sliders
// and reaches the treasury whole — IncomeCollection runs after MineralConversion.
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

// Caps one tile resource at `max`. Lift the cap by putting `removed_by_tech` on the
// EffectConfig_t (FactionEffectsPool drops it once that tech is discovered).
struct TileResourceCapEffect_t
{
    StatId_t stat = StatId_t::Nutrients;
    // Required in JSON (`max`); no SMAC balance default — set explicitly when hand-building.
    int max;
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
// unitFilter (on EffectConfig_t) selects eligible attackers; condition gates the situation.
// Optional cooldownTurns: when >= 0, attempting deploys the source (shared with OrbitalAttack
// when both live on the same building id).
struct InterceptAttemptEffect_t
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

// Grants a capability the rules otherwise deny. Enter almost always carries a condition
// selecting which tiles; Attack on stock pods is unconditional (any channel cross).
struct PermissionEffect_t
{
    PermissionId_t permission = PermissionId_t::Attack;
};

// Instantaneous base-size mutation (colony-pod production cost, genetic plague, …).
// Add: `amount` is a signed absolute delta. AddPercent: delta = size * amount / 100
// (integer division toward zero; −50 on size 5 → −2). Never shrinks below minSize.
struct ModifyPopulationEffect_t
{
    int amount = 0;
    ModifierOp_t op = ModifierOp_t::Add;
    int minSize = 0;
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
    PermissionEffect_t,
    ModifyPopulationEffect_t
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

// True when EffectContext_t::pAttacker is non-null and embarked.
struct AttackerIsEmbarked_t
{
};

// True when EffectContext_t::pBase has the Headquarters rule flag (Economy SE −1
// energy-at-HQ). Requires pBase in the resolve context.
struct IsHeadquarters_t
{
};

struct Condition_t : std::variant<TargetTileHas_t, AllOf_t, IsDefending_t,
                                  OriginBaseIsTargetBase_t, AttackerIsEmbarked_t,
                                  IsHeadquarters_t>
{
    using Variant = std::variant<TargetTileHas_t, AllOf_t, IsDefending_t,
                                 OriginBaseIsTargetBase_t, AttackerIsEmbarked_t,
                                 IsHeadquarters_t>;
    using Variant::Variant;
    using Variant::operator=;

    Variant& AsVariant() & { return *this; }
    const Variant& AsVariant() const & { return *this; }
};

// Restricts which units an effect applies to when merged into a live unit's effect list
// (CollectLiveUnitEffects). Absent = all units. Distinct from condition: filters are
// unit-identity predicates evaluated context-free (domain, component loadout), not combat
// situational predicates.
struct UnitFilterDomain_t
{
    UnitDomain_t domain = UnitDomain_t::Land;
};

struct UnitFilterHasComponent_t
{
    std::string component;
};

// Unit resolves true for the named RuleFlag (design + FactionUnits).
struct UnitFilterHasFlag_t
{
    RuleFlagId_t flag = RuleFlagId_t::Flight;
};

// True when the unit was created carrying a component its faction had never fielded.
// Reads Unit::IsPrototype, latched at construction, so it stays a context-free identity
// predicate like the filters above rather than a live read of the faction's build ledger.
struct UnitFilterIsPrototype_t
{
};

// True when UnitDesign::IsCombatUnit (additive Attack > 0 or ForcesPsiCombat).
// Design-only (same recursion rule as HasFlag) so FactionUnits collection stays safe.
struct UnitFilterIsCombatUnit_t
{
};

using UnitFilter_t =
    std::variant<UnitFilterDomain_t, UnitFilterHasComponent_t, UnitFilterHasFlag_t,
                 UnitFilterIsPrototype_t, UnitFilterIsCombatUnit_t>;

// Restricts which buildings a FacilityEnergyUpkeep (or similar) modifier applies to.
// Absent buildingFilter = all buildings. Distinct from unitFilter.
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
    EffectPersistence_t persistence;
    // Absent = the effect always applies. When present, the effect only applies in a runtime
    // context that satisfies the condition (see ConditionSatisfied / EffectContext_t). Such
    // effects are excluded from context-free resolution (base economy, intrinsic unit stats).
    std::optional<Condition_t> condition;
    // Absent = applies to every unit that receives this effect. When present, CollectLiveUnitEffects
    // drops the effect for units that do not match (e.g. Domain=Air for Aerospace Complex).
    std::optional<UnitFilter_t> unitFilter;
    // Absent = applies to every building when resolving FacilityEnergyUpkeep. When present,
    // only matching building types receive the modifier (All / BuildingId / Category).
    std::optional<BuildingFilter_t> buildingFilter;
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
    // Nearest Chebyshev distance the effect reaches, so an aura can skip its own host tile.
    // 0 (default) = includes the host. 1 with radius 1 is a ring: the Echelon Mirror boosts
    // neighbouring solar collectors but not the one it counts as itself. Parsed from
    // "min_radius"; must be <= radius.
    int minRadius = 0;
};

} // namespace ac
