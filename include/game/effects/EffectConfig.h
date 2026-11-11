#pragma once

#include "game/diplomacy/TradeKind.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/InteractionGridsConfig.h"
#include "game/GameCategory.h"
#include "game/units/UnitDomain.h"
#include "game/units/UnitPlan.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

// Terminal `else` for exhaustive std::visit, which has no -Wswitch: a new alternative breaks
// the build at every site that must decide about it.
template <typename T>
inline constexpr bool k_AlwaysFalse = false;

// Expands the target's effects onto this source only — no constructed copy, no upkeep. To
// actually build the facility, use the triggered AddBuildingEffect_t.
struct GrantBuildingEffect_t
{
    std::string buildingId;
};

// Standing datalink infiltration for as long as the effect is active: WorldGlobal reaches
// every other faction, or a factionFilter picks. SetInfiltrationEffect_t writes it once.
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
    // Scales `amount` by a runtime value instead of adding it. Each source needs its subject on
    // EffectContext_t (docs/architecture/effects-system.md).
    enum class AmountSource_t
    {
        // ceil(tile elevation / elevation_energy_step_meters), at least 0: all of a solar
        // collector's energy. Energy, ThisTile only.
        ElevationEnergy,
        // Minerals a stockpile converts; stockpile output stats, ThisBase only.
        MineralsConverted,
        // Base population (University drones 0.25).
        BaseSize,
        // Bases the faction owns; unit stats, ThisUnit only.
        BasesOwned,
        // The unit's own XP, not SE-shifted morale; unit stats, ThisUnit only.
        IntrinsicXp,
        // The base's facility upkeep; continuous econ MaxClamp only.
        BuildingUpkeep,
        // AddPercent per whole elevation level this unit stands above its opponent; 0 if lower.
        LevelsAboveOpponent,
        // A faction social rating, looked up through `amounts`; a missing key contributes 0.
        SocialRating,
    };
    struct AmountSourceSpec_t
    {
        AmountSource_t kind = AmountSource_t::BaseSize;
        SocialRatingId_t rating = SocialRatingId_t::Morale;
        std::optional<std::map<int, double>> amounts;
        AmountSourceSpec_t() = default;
        AmountSourceSpec_t(AmountSource_t sourceKind) : kind(sourceKind) {}
    };
    std::optional<AmountSourceSpec_t> amountSource;
    bool HasAmountSource(AmountSource_t kind) const
    {
        return amountSource.has_value() && amountSource->kind == kind;
    }
    // Applies to each worked tile whose features match, instead of once per base.
    std::optional<TileSelector_t> selector;
    // When true, this contribution is added after per-tile MaxClamp / MinClamp (classic SMAC
    // resource-bonus specials). Only valid on nutrients/minerals/energy with op Add.
    bool bypassClamp = false;
};

// Stats a MineralsConverted modifier may target: not minerals, the input. Energy runs through
// inefficiency and the sliders like collected energy; econ reaches the treasury whole.
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

// Blocks adopting the named social policy (faction opposition / society_avail).
struct ForbiddenSocialPolicyEffect_t
{
    SocialPolicyId_t policy;
};

// While the named policy is active, its negative SocialRatingModifier Adds do not apply.
struct SocialPolicyImpunityEffect_t
{
    SocialPolicyId_t policy;
};

// Adds or clamps one social engineering axis; social_rating_effects.json maps totals to
// effects. Only Add, MinClamp, and MaxClamp are valid ops.
struct SocialRatingModifierEffect_t
{
    SocialRatingId_t rating;
    int amount = 0;
    ModifierOp_t op = ModifierOp_t::Add;
};

// Hides a unit on a JSON-defined detection channel: itself (ThisUnit) or everything on and
// around a tile (ThisTile).
struct ConcealEffect_t
{
    std::string channel;
};

// Pierces concealment on a channel, typically ThisTile with a radius. Needs ownerFaction set,
// or it never pierces.
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
    // Ready when missionYear >= deployYear + cooldownTurns + 1. Required in JSON.
    int cooldownTurns;
    // Percent chance the attacking satellite is destroyed when the attempt fails (miss).
    // Optional in JSON (default 0).
    int chanceOfDestructionOnFail = 0;
};

// Before combat, rolls to destroy an attacker the condition selects. cooldownTurns >= 0
// deploys the source, shared with OrbitalAttack on the same building.
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

// This unit may scramble to defend against attackers the condition selects, from up to
// `range` tiles away (ScrambleRules).
struct ScrambleEffect_t
{
    // Required in JSON; set explicitly when hand-building. Must be > 0.
    int range;
};

// Passenger domains this unit carries, and whether loading needs a harbor. Carried cargo
// refuels aboard. Unions across matching ThisUnit effects.
struct TransportParamsEffect_t
{
    std::vector<UnitDomain_t> carries;
    bool requiresHarbor = false;
};

// Flips one coexistence answer for the acting former; the pair is unordered and one named
// occupant wild-cards the other. The tile keeps a waiver for what it built.
struct CoexistenceOverrideEffect_t
{
    InteractionCell_t cell = InteractionCell_t::Allow;
    std::string firstId;
    std::string secondId;
};

// Overrides an interaction grid cell (allow or deny); one restating the stock cell is
// skipped, so order never matters. Omit an axis to match any value.
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
    ForbiddenSocialPolicyEffect_t,
    SocialPolicyImpunityEffect_t,
    SocialRatingModifierEffect_t,
    ConcealEffect_t,
    DetectEffect_t,
    OrbitalAttackEffect_t,
    InterceptEffect_t,
    ScrambleEffect_t,
    TransportParamsEffect_t,
    InteractionOverrideEffect_t,
    CoexistenceOverrideEffect_t
>;

// Runtime predicates on EffectConfig_t, as a variant so AllOf_t can recurse. Visit through
// AsVariant().
struct Condition_t;

// The targeted tile (the defender's in combat) has a feature: terrain, river, fungus or any
// improvement. bPresent false inverts it, which covers every negation the rules need.
struct TargetTileHas_t
{
    std::string featureId;
    bool bPresent = true;
};

// Every nested condition holds. JSON `"values"` desugars to TargetTileHas; an empty list is
// rejected at parse and false when hand-built.
struct AllOf_t
{
    std::vector<Condition_t> conditions;
};

// At least one nested condition is satisfied (OR). Mirrors AllOf_t, including the parser's
// `values` desugaring to TargetTileHas alternatives and its rejection of an empty list.
struct AnyOf_t
{
    std::vector<Condition_t> conditions;
};

// True when EffectContext_t::combatRole is Defender, matching bMatches. Omit value in JSON
// to require Defender; "value": false matches the attacker. CombatRole None matches neither.
struct IsDefending_t
{
    bool bMatches = true;
};

// True when combatRole is Attacker or Defender. Display and other non-combat resolves
// leave it None, so rank-word callers skip combat-only rank shifts (riot).
struct IsInCombat_t
{
};

// True when ActiveEffect_t::originBase is EffectContext_t::pUnit's home base
// (home-base aura: Crèche stated-morale offsets, riot penalty).
struct OriginBaseIsHomeBase_t
{
};

// True when pUnit stands on the effect's originBase tile (in-square Crèche / Brood Pit).
// Distinct from OriginBaseIsHomeBase (home, any location). Fail closed if either is missing.
struct SubjectIsOnOriginBase_t
{
};

// True when the unit owner's faction-wide social rating satisfies CompareOp_t against value.
// Fail closed if pUnit is absent.
struct SocialRating_t
{
    SocialRatingId_t rating = SocialRatingId_t::Morale;
    CompareOp_t op = CompareOp_t::Eq;
    int value = 0;
};

// True when EffectiveMoraleLevel(pUnit) satisfies CompareOp_t against value.
// Fail closed if pUnit is absent. Needs a live game (morale calculator from GameData).
struct EffectiveMorale_t
{
    CompareOp_t op = CompareOp_t::Eq;
    int value = 0;
};

// True when the effect's originBase has a constructed copy of buildingId, matching bPresent.
// Distinct from BaseHasBuilding (EffectContext_t::pBase). Fail closed if originBase is absent.
struct OriginBaseHasBuilding_t
{
    std::string buildingId;
    bool bPresent = true;
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

// EffectContext_t::pDefender's design has native_life matching bMatches; fails closed without a
// defender. Omit the JSON value to require native life.
struct DefenderIsNativeLife_t
{
    bool bMatches = true;
};

// True when EffectContext_t::pBase has the Headquarters rule flag (Economy SE −1
// energy-at-HQ). Requires pBase in the resolve context.
struct IsHeadquarters_t
{
};

// True when the subject faction equals EffectContext_t::effectOwnerFaction. Subject is
// pFaction when set, else pUnit's faction. Fails closed if either is missing, or the owner
// is k_NoFactionOwner (unowned territory stamp).
struct SubjectIsOwner_t
{
};

// True when the subject faction differs from a real effectOwnerFaction. Same subject rule as
// SubjectIsOwner. Fails closed without a subject, without a stamped owner, or when the owner
// is k_NoFactionOwner.
struct SubjectIsForeign_t
{
};

// Identity predicates on the effect subject, pUnit, failing closed without one. Not the
// combat roles.
struct SubjectDomain_t
{
    UnitDomain_t domain = UnitDomain_t::Land;
};

struct HasComponent_t
{
    std::string component;
};

// True when EffectContext_t::pUnit's IDesign::GetId equals designId.
// Fail closed if pUnit is absent. Natives have no components, so HasComponent cannot name them.
struct SubjectDesign_t
{
    std::string designId;
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

// The design's native_life flag matches bMatches; omit the JSON value to require it.
struct IsNativeLife_t
{
    bool bMatches = true;
};

// True when EffectContext_t::pBase has a constructed copy of buildingId.
// Fail closed if pBase is absent. This is how a unit's on_hold_effects names a facility.
struct BaseHasBuilding_t
{
    std::string buildingId;
};

// Whether pUnit's IDesign::GetPlan is in plans matches bMatches. Fail closed if pUnit is absent.
// Empty plans is rejected by the parser.
struct DesignPlan_t
{
    std::vector<UnitPlan_t> plans;
    bool bMatches = true;
};

// Probe-cost relations: EffectContext_t::pPartner is the other side of the quote (the actor on a
// target-side resolve, the target faction on the actor-side one). Each fails closed without its
// subject.

// pPartner's control matches type.
struct PartnerPlayerType_t
{
    PlayerType_t type = PlayerType_t::Player;
};

// pBase's occupier window is active and its former owner is pPartner.
struct PartnerIsFormerOwner_t
{
};

// pFaction has committed an atrocity against pPartner.
struct VictimizedPartner_t
{
};

// pBase is set: on the actor-side probe resolve, the quote is for a base.
struct TargetIsBase_t
{
};

// The gift being valued is of kind. Fails closed outside a gift's resolve.
struct GiftKind_t
{
    TradeKind_t kind = TradeKind_t::Credits;
};

using ConditionVariant_t = std::variant<TargetTileHas_t, AllOf_t, AnyOf_t, IsDefending_t,
                                        IsInCombat_t, OriginBaseIsHomeBase_t, SubjectIsOnOriginBase_t,
                                        SocialRating_t, EffectiveMorale_t, OriginBaseHasBuilding_t,
                                        AttackerIsEmbarked_t, HasAirdroppedThisTurn_t,
                                        AttackerDomain_t, DefenderDomain_t,
                                        DefenderIsNativeLife_t, IsHeadquarters_t,
                                        SubjectIsOwner_t, SubjectIsForeign_t,
                                        SubjectDomain_t, HasComponent_t, SubjectDesign_t,
                                        HasFlag_t, IsPrototype_t, IsCombatUnit_t,
                                        IsNativeLife_t, BaseHasBuilding_t, DesignPlan_t,
                                        PartnerPlayerType_t, PartnerIsFormerOwner_t,
                                        VictimizedPartner_t, TargetIsBase_t, GiftKind_t>;

struct Condition_t : ConditionVariant_t
{
    using Variant = ConditionVariant_t;
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
    // Absent: always applies. Situational arms stay out of context-free resolution; identity
    // arms apply wherever pUnit is set.
    std::optional<Condition_t> condition;
    // Absent = applies to every building when resolving FacilityEnergyUpkeep. When present,
    // only matching building types receive the modifier (All / BuildingId / Category).
    std::optional<BuildingFilter_t> buildingFilter;
    // Absent = default target set from scope (WorldGlobal → all other factions). When present,
    // further restricts which factions a directed cross-faction effect applies to.
    std::optional<FactionFilter_t> factionFilter;
    // The faction pool drops this effect once the faction discovers the tech.
    std::string removedByTech;
    // For ThisTile-scoped effects: how far (Chebyshev tiles) beyond the host tile the effect
    // reaches. 0 (default) = the host tile only. Parsed from the effect entry's "radius" field.
    int radius = 0;
    // Nearest distance the effect reaches; 1 with radius 1 is a ring that skips its own tile.
    // At most radius.
    int minRadius = 0;
};

} // namespace ac
