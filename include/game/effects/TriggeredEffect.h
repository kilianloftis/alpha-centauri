#pragma once

#include "game/buildings/FacilityExclusions.h"
#include "game/atrocities/AtrocityConfig.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "lib/Rational.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

// One-shot changes fired by a named trigger. Separate from EffectConfig_t, which is queried
// rather than executed, so neither can be used as the other.

// Constructs the facility for real: the base gains a copy and pays its upkeep. Distinct from
// the continuous GrantBuildingEffect_t, which only expands the target's effects.
struct AddBuildingEffect_t
{
    std::string buildingId;
};

struct GrantTechEffect_t
{
    // Present: grant this tech id. Absent: pick uniformly from AvailableTechs() (SMAC
    // goody / artifact / Secrets — currently researchable only).
    std::optional<std::string> techId;
};

// A unit from component ids (ad-hoc design) or a named design_id on the subject's Military /
// NativeUnitRegistry. Parser requires exactly one of the two.
struct GrantUnitEffect_t
{
    std::vector<std::string> componentIds;
    int count = 1;
    std::optional<std::string> designId;
};

// Treasury credit. Fixed `amount`, or Lua `formula` (exclusive). Formula global: `turn`.
struct GrantEnergyEffect_t
{
    std::optional<int> amount;
    std::optional<std::string> formula;
};

// Places one improvement occupant on the context tile when CanBuildImprovement allows.
struct AddOccupantEffect_t
{
    std::string improvementId;
};

// Marks FactionExploredMap in a Chebyshev radius around the context tile.
struct RevealMapEffect_t
{
    int radius = 0;
};

// Moves the context unit to a random same-surface tile in the distance band.
struct TeleportUnitEffect_t
{
    int minDistance = 0;
    int maxDistance = 0;
};

// Sets the nearest own base's mineral stockpile to the current item's cost.
struct FinishProductionEffect_t
{
};

// Creates another unit of the context unit's design on its tile.
struct CloneUnitEffect_t
{
};

// Sets Known with a random other non-native unknown faction.
struct GrantCommlinkEffect_t
{
};

// Spawns a native-life Military design on one random Chebyshev-1 neighbor the design can hold
// that has no hostile occupants. Optional cargo_ids fill FreeCargoSlots after spawn.
struct SpawnNativeEffect_t
{
    std::string nativeId;
    std::vector<std::string> cargoIds;
};

// Places a cluster of improvements: uniform pick from improvementIds. chance[d] is the
// independent roll for Chebyshev distance d (index 0 is the origin).
struct TerraformClusterEffect_t
{
    std::vector<std::string> improvementIds;
    std::vector<Rational_t> chance;
};

// Places one randomly chosen buildable improvement from the list on the context tile.
struct AddRandomOccupantEffect_t
{
    std::vector<std::string> improvementIds;
};

// Credits a base resource stockpile. `amount` seeds the resolve; optional `modifierStat`
// (e.g. StartingMinerals) applies that base-domain stack on top. Floored at 0.
struct GrantResourceEffect_t
{
    StatId_t resource = StatId_t::Minerals;
    int amount = 0;
    std::optional<StatId_t> modifierStat;
};

// World-state change applied by PlanetaryCouncil (sea level / climate).
struct WorldParameterEffect_t
{
    WorldParameterId_t parameter = WorldParameterId_t::SeaLevel;
    // Signed delta applied when the effect fires (negative = cooling / falling seas).
    int amount = 0;
};

// Writes datalink infiltration into the DiplomacyLedger for good, to factionFilter or every
// other faction.
struct SetInfiltrationEffect_t
{
};

// Base-size change: Add is a signed delta, AddPercent truncates size * amount / 100 toward
// zero. Never below minSize.
struct ModifyPopulationEffect_t
{
    int amount = 0;
    ModifierOp_t op = ModifierOp_t::Add;
    int minSize = 0;
};

// XP credit on a unit, clamped to the morale maximum. removeHostChance may then remove the
// visited improvement (Monolith 1/32).
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

// Random facility destruction at the context base. Every field is required: what is
// off-limits is each caller's rule.
struct DestroyFacilityEffect_t
{
    int count = 0;
    FacilityExclusions_t exclusions;
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

// Raises the context tile through ApplyEarthquake. Exactly one of `levels` and `levelsStat`
// (read off the subject unit's reactor).
struct EarthquakeEffect_t
{
    int levels = 0;
    std::optional<StatId_t> levelsStat;
};

// Turns the context tile and a random sample of its neighbours to fungus. Exactly one mode:
// fixed `tiles`, unit `tilesStat`, or TerraformCluster-style `chance` (per-distance rolls;
// index 0 is the origin).
struct FungalBloomEffect_t
{
    int tiles = 0;
    std::optional<StatId_t> tilesStat;
    std::vector<Rational_t> chance;
};

// Destroys other units, razes bases and removes improvements in a disk, then lowers it a
// level. Exactly one of `radius` and `radiusStat`; the subject survives to be spent.
struct ExplosionEffect_t
{
    int radius = 0;
    std::optional<StatId_t> radiusStat;
};

// Adds or clears one terrain occupant on the context tile. Derived terrain (rockiness,
// moisture, depth bands) is rejected at load.
struct SetTerrainFeatureEffect_t
{
    std::string featureId;
    bool bPresent = true;
};

// Moves the context tile along the rockiness axis (Flat < Rolling < Rocky), clamped at both
// ends. Negative smooths — Level Terrain is one step down.
struct StepRockinessEffect_t
{
    int steps = -1;
};

// One Former level roll on the context tile. Its floor depends on the subject's domain, which
// is what sets it apart from EarthquakeEffect_t.
struct ElevationChangeEffect_t
{
    bool bRaise = true;
};

// Records an atrocity by the subject faction and applies its consequences from
// config/atrocities.json. The victim is derivedVictim if an earlier arm set one.
struct CommitAtrocityEffect_t
{
    // One of AtrocitySeverityId_t. The parser rejects any other name.
    AtrocitySeverityId_t severity = AtrocitySeverityId_t::Simple;
};

// Records mind control by the subject faction (the actor) in the MindControlLedger.
struct RecordMindControlEffect_t
{
    int weight = 0;
};

// Raises the subject faction's clean-minerals cap for good. In on_complete_effects, so only a
// built facility credits it.
struct GrantCleanMineralsEffect_t
{
    int amount = 0;
    bool bRequiresFirstBloom = false;
};

// Charges the subject faction virtual minerals in the EcologyLedger: eco damage no production
// caused (a tectonic detonation). Permanent, and ungated by the Charter.
struct AddVirtualMineralsEffect_t
{
    int amount = 0;
};

using TriggeredEffectVariant_t = std::variant<
    AddBuildingEffect_t,
    GrantTechEffect_t,
    GrantUnitEffect_t,
    GrantEnergyEffect_t,
    GrantResourceEffect_t,
    WorldParameterEffect_t,
    SetInfiltrationEffect_t,
    ModifyPopulationEffect_t,
    GrantXpEffect_t,
    RestoreHitPointsEffect_t,
    DestroyFacilityEffect_t,
    RebelEffect_t,
    DestroyUnitEffect_t,
    EarthquakeEffect_t,
    FungalBloomEffect_t,
    ExplosionEffect_t,
    SetTerrainFeatureEffect_t,
    StepRockinessEffect_t,
    ElevationChangeEffect_t,
    CommitAtrocityEffect_t,
    RecordMindControlEffect_t,
    GrantCleanMineralsEffect_t,
    AddVirtualMineralsEffect_t,
    AddOccupantEffect_t,
    RevealMapEffect_t,
    TeleportUnitEffect_t,
    FinishProductionEffect_t,
    CloneUnitEffect_t,
    GrantCommlinkEffect_t,
    SpawnNativeEffect_t,
    TerraformClusterEffect_t,
    AddRandomOccupantEffect_t
>;

// Which subject remembers that a once-only entry has already fired.
enum class OnceScope_t
{
    Unit,
    Base,
    Faction,
    World,
};

// Present: fires at most once per subject, under an authored key that spans instances, as
// every Monolith shares "monolith_xp".
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
