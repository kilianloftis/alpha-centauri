#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "lib/Rational.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

// One-shot changes fired by a named trigger, as opposed to the continuous modifiers in
// EffectConfig_t. The two are separate types because they are separate machines: a continuous
// effect is *queried* (pooled, filtered by context, idempotent), a triggered effect is
// *executed* (RNG, ledger writes, ownership transfers, exactly once). Keeping them apart is
// what makes "a StatModifier that fires once" and "a Rebel that applies continuously"
// unrepresentable instead of silently doing nothing.
//
// Containers declare these in trigger-named lists (`on_complete_effects`, `on_enter_effects`,
// `on_visit_effects`, `on_hold_effects`, …); the slot says when they fire, so there is no
// persistence field.

// Constructs the facility for real: the base gains a copy and pays its upkeep. Distinct from
// the continuous GrantBuildingEffect_t, which only expands the target's effects.
struct AddBuildingEffect_t
{
    std::string buildingId;
};

struct GrantTechEffect_t
{
    // Present: grant this tech id. Absent: pick uniformly from GetAvailableTechs() (Secrets
    // first-discover bonus). Parser requires exactly one of tech_id / selection Available.
    std::optional<std::string> techId;
};

// A unit assembled from component ids, the same shape as EscapeColonyPodConfig_t — there is no
// registry of named designs to look a single id up in, because designs are per-faction and
// player-authored with ids derived from their components (UnitDesign::GetId).
struct GrantUnitEffect_t
{
    std::vector<std::string> componentIds;
    int count = 1;
};

// Treasury credit. Council proposals (Salvage Unity Fusion Core) fire this.
struct GrantEnergyEffect_t
{
    int amount = 0;
};

// World-state change applied by PlanetaryCouncil (sea level / climate).
struct WorldParameterEffect_t
{
    WorldParameterId_t parameter = WorldParameterId_t::SeaLevel;
    // Signed delta applied when the effect fires (negative = cooling / falling seas).
    int amount = 0;
};

// Writes directed datalink infiltration into the DiplomacyLedger for good. The target set is
// factionFilter (ActionTarget for a probe mission, CouncilMembers, PlayerType); absent means
// every other faction. Distinct from the continuous InfiltrationEffect_t, which is a standing
// law honored at query time by HasInfiltration and lapses with the effect.
struct SetInfiltrationEffect_t
{
};

// Base-size change (colony-pod production cost, genetic plague, …).
// Add: `amount` is a signed absolute delta. AddPercent: delta = size * amount / 100
// (integer division toward zero; −50 on size 5 → −2). Never shrinks below minSize.
struct ModifyPopulationEffect_t
{
    int amount = 0;
    ModifierOp_t op = ModifierOp_t::Add;
    int minSize = 0;
};

// Instant XP credit on a unit subject (train bonuses, prototype first-fielding, monolith).
// Stacks via ApplyModifierStack onto the unit's current XP; SetXp clamps to morale max.
// Optional removeHostChance: after XP actually increases, roll to remove the visit host
// improvement named on TriggeredEffectContext_t::hostImprovementId (Monolith 1/32).
struct GrantXpEffect_t
{
    int amount = 0;
    ModifierOp_t op = ModifierOp_t::Add;
    std::optional<Rational_t> removeHostChance;
};

// Ops for RestoreHitPoints. AddPercent / SetPercent are relative to the unit's max HitPoints,
// not ApplyModifierStack's percent-of-current semantics — hence a dedicated enum.
enum class RestoreHitPointsOp_t
{
    Add,
    AddPercent,
    MaxClamp,
    MinClamp,
    SetPercent,
};

// Instant heal on a unit subject (monolith visit). SetCurrentHp clamps to live max.
struct RestoreHitPointsEffect_t
{
    int amount = 0;
    RestoreHitPointsOp_t op = RestoreHitPointsOp_t::Add;
};

// Random facility destruction at the context base (riot escalation, probe sabotage).
// Every field is required in JSON: which facilities are off-limits is a game rule per caller,
// and a C++ default here is how the shipping config and the test fixture came to disagree
// about whether sabotage can destroy a secret project.
struct DestroyFacilityEffect_t
{
    int count = 0;
    bool excludeHq = false;
    bool excludeSecretProjects = false;
};

// Base ownership transfer to a weighted other faction (riot rebellion). Candidate selection
// uses pop_composition rebel_selection + RebelJoinWeight.
struct RebelEffect_t
{
};

// Destroys the context unit. No parameters: the subject is whichever unit the trigger stamped.
// Absent unit does nothing.
struct DestroyUnitEffect_t
{
};

// Raises the context tile through ApplyEarthquake: levelCount rolls from map_rules.json, then
// the neighbor slope relaxation. Exactly one of the two magnitude sources is authored —
// `levels` is a literal (a scripted quake), `levelsStat` reads that stat off the subject unit
// (a tectonic payload takes its size from the reactor's EarthquakeLevels).
struct EarthquakeEffect_t
{
    int levels = 0;
    std::optional<StatId_t> levelsStat;
};

using TriggeredEffectVariant_t = std::variant<
    AddBuildingEffect_t,
    GrantTechEffect_t,
    GrantUnitEffect_t,
    GrantEnergyEffect_t,
    WorldParameterEffect_t,
    SetInfiltrationEffect_t,
    ModifyPopulationEffect_t,
    GrantXpEffect_t,
    RestoreHitPointsEffect_t,
    DestroyFacilityEffect_t,
    RebelEffect_t,
    DestroyUnitEffect_t,
    EarthquakeEffect_t
>;

// Which subject remembers that a once-only entry has already fired.
enum class OnceScope_t
{
    Unit,
    Base,
    Faction,
    World,
};

// Absent = fires every time its trigger does. Present = fires at most once per subject.
// The key is authored in config rather than derived from the entry, because the rule usually
// spans instances: every Monolith shares "monolith_xp", so visiting a second one grants nothing.
struct OncePer_t
{
    OnceScope_t scope = OnceScope_t::Unit;
    std::string key;
};

struct TriggeredEffectConfig_t
{
    TriggeredEffectVariant_t effect;
    // Only cross-faction entries (SetInfiltration) need this; the context supplies the natural
    // target set for everything else.
    std::optional<FactionFilter_t> factionFilter;
    std::optional<OncePer_t> oncePer;
    // Optional gate against TriggeredEffectContext_t::subjects (same Condition_t as continuous).
    std::optional<Condition_t> condition;
};

} // namespace ac
