#pragma once

#include <stdexcept>
#include <string>

namespace ac
{

enum class StatId_t
{
    // Base resources
    Nutrients,
    Minerals,
    Energy,

    // Base output: seed from this base's post-inefficiency energy split, plus flat Add
    // from facilities / specialists.
    Econ,
    Labs,
    Psych,

    // Composition modifiers: add/remove drones or talents at a base (Police SE, facilities).
    Drones,
    Talents,

    // Unit stats
    Attack,
    Defense,
    Movement,
    Vision,
    HitPoints,
    // Damage received per lost psi-combat round. Reactors set this to their tier.
    PsiDamage,
    DisengageChance,
    // Turns of fuel capacity; max fuel pool = TurnsOfFuel × Movement. 0 = unlimited / no tracking.
    TurnsOfFuel,
    // Percent of max HP applied when a fueled unit ends a turn at 0 fuel away from a refuel site.
    DamageFromOutOfFuel,
    CargoCapacity,
    DifficultTerrainCost,
    CostMultiplier,
    // Multiplier on enemy probe mind-control / subversion energy costs (PureMultiplier).
    // SE Probe levels emit AddPercent; resolved from the target base's effect list.
    ProbeActionCost,
    // Additive local probe defense (Covert Ops Center +2, etc.). Added to SE Probe before
    // the success/escape clamp.
    ProbeDefense,
    // Scales mission/escape failure rates for the acting probe (PureMultiplier). Algorithmic
    // Enhancement emits AddPercent -50. Not applied against a target with BlocksProbeTeams.
    ProbeFailureScale,
    // Scales mission/escape success rates against this target (PureMultiplier). Hunter-Seeker
    // Algorithm emits AddPercent -50.
    ProbeSuccessScale,
    // XP granted when a unit is created (seeded into Unit::m_xp at spawn; not a live max).
    StartingExperience,
    // Live morale-level offset (SE Morale, Creche in-base, etc.). Added to Unit::m_xp when
    // computing effective combat morale; not seeded into m_xp.
    MoraleBonus,
    // Scales positive *conditional* morale_bonus contributions (Creche in-base, etc.).
    // PureMultiplier (seed 1.0); SE Morale ≤ -2 uses AddPercent -50 ("+ modifiers halved").
    PositiveMoraleScale,

    // Population growth rate modifier (AddPercent, base = 100%)
    GrowthRate,

    // Research tech cost percentage modifier (Add, base = 0; negative = cheaper)
    TechCost,

    // Tile terrain mutation (resolved back into Tile::SetMoisture, not a runtime-queried stat)
    MoistureTier,

    // Planetary commerce income multiplier (PureMultiplier; Global Trade Pact uses AddPercent).
    CommerceRate,
    // Extra council votes (Additive). Population elections seed with total population;
    // representative elections seed with 1. Buildings / projects / faction bonuses modify this.
    CouncilVotes,
    // Bonus energy credited per commerce transaction at each base (Additive; Planetary Governor).
    CommerceEnergyBonus,

    // Absolute denominator for energy inefficiency: loss = Energy × Distance / denom.
    // Efficiency SE levels emit this as Add with the table value (64, 56, …, 0). Denom ≤ 0
    // means 100% loss. Resolved from the efficiency rating table, not stacked as a live seed.
    InefficiencyDenominator
    // TODO: add more stats as they are defined
};

// How a stat's modifier stack is seeded — the seed-semantics counterpart of LaneFor's scope
// routing (LaneFor in this header). Adding a StatId_t forces a kind decision in KindFor's exhaustive
// switch, and SeedFor derives the context-free seed from it, so a resolve site can no longer
// default a pure-multiplier stat to a 0.0 seed (which silently resolves to 0 — see
// ResolveStatModifiers).
enum class StatKind_t
{
    // Contributions add onto an empty base; the context-free seed is 0.0.
    Additive,
    // The stat IS a multiplier, resolved purely through AddPercent/MultiplyGeometric
    // contributions; the seed is the identity 1.0 (a 0.0 seed collapses the result to 0).
    PureMultiplier,
    // Modifiers scale a raw value only the resolve site knows (GrowthRate's 100% baseline,
    // MoistureTier's base tier). No universal seed exists — SeedFor throws, forcing the
    // caller to pass the raw value explicitly.
    RawScaled,
};

constexpr StatKind_t KindFor(StatId_t stat)
{
    switch (stat)
    {
        case StatId_t::Nutrients:
        case StatId_t::Minerals:
        case StatId_t::Energy:
        case StatId_t::Econ:
        case StatId_t::Labs:
        case StatId_t::Psych:
        case StatId_t::Drones:
        case StatId_t::Talents:
        case StatId_t::Attack:
        case StatId_t::Defense:
        case StatId_t::Movement:
        case StatId_t::Vision:
        case StatId_t::HitPoints:
        case StatId_t::PsiDamage:
        case StatId_t::DisengageChance:
        case StatId_t::TurnsOfFuel:
        case StatId_t::DamageFromOutOfFuel:
        case StatId_t::CargoCapacity:
        case StatId_t::DifficultTerrainCost:
        case StatId_t::StartingExperience:
        case StatId_t::MoraleBonus:
        case StatId_t::ProbeDefense:
        case StatId_t::TechCost:
        case StatId_t::CouncilVotes:
        case StatId_t::CommerceEnergyBonus:
        case StatId_t::InefficiencyDenominator: return StatKind_t::Additive;
        case StatId_t::CostMultiplier:
        case StatId_t::ProbeActionCost:
        case StatId_t::ProbeFailureScale:
        case StatId_t::ProbeSuccessScale:
        case StatId_t::PositiveMoraleScale:
        case StatId_t::CommerceRate:         return StatKind_t::PureMultiplier;
        case StatId_t::GrowthRate:
        case StatId_t::MoistureTier:         return StatKind_t::RawScaled;
    }
    return StatKind_t::Additive; // unreachable; all enumerators handled above
}

// The ResolveStatModifiers seed for a context-free resolve of `stat`: 0.0 for Additive,
// 1.0 for PureMultiplier. RawScaled stats throw — their resolve site passes the raw value
// being scaled instead. A site that deliberately resolves an Additive stat against a raw
// base (tile yield's elevation energy seed, pop tile multipliers, the tile defense
// multiplier) also passes its seed explicitly rather than calling this.
constexpr double SeedFor(StatId_t stat)
{
    switch (KindFor(stat))
    {
        case StatKind_t::Additive:       return 0.0;
        case StatKind_t::PureMultiplier: return 1.0;
        case StatKind_t::RawScaled:
            throw std::logic_error("SeedFor: RawScaled stat has no universal seed - pass the raw value it scales");
    }
    return 0.0; // unreachable; all enumerators handled above
}

// Snake_case JSON wire form differs from enumerator names — one explicit map next to the enum.
inline StatId_t ParseStatId(const std::string& rStat)
{
    if (rStat == "nutrients")               return StatId_t::Nutrients;
    if (rStat == "minerals")                return StatId_t::Minerals;
    if (rStat == "energy")                  return StatId_t::Energy;
    if (rStat == "econ")                    return StatId_t::Econ;
    if (rStat == "labs")                    return StatId_t::Labs;
    if (rStat == "psych")                   return StatId_t::Psych;
    if (rStat == "drones")                  return StatId_t::Drones;
    if (rStat == "talents")                 return StatId_t::Talents;
    if (rStat == "attack")                  return StatId_t::Attack;
    if (rStat == "defense")                 return StatId_t::Defense;
    if (rStat == "movement")                return StatId_t::Movement;
    if (rStat == "vision")                  return StatId_t::Vision;
    if (rStat == "hit_points")              return StatId_t::HitPoints;
    if (rStat == "psi_damage")              return StatId_t::PsiDamage;
    if (rStat == "disengage_chance")        return StatId_t::DisengageChance;
    if (rStat == "turns_of_fuel")           return StatId_t::TurnsOfFuel;
    if (rStat == "damage_from_out_of_fuel") return StatId_t::DamageFromOutOfFuel;
    if (rStat == "cargo_capacity")          return StatId_t::CargoCapacity;
    if (rStat == "difficult_terrain_cost")  return StatId_t::DifficultTerrainCost;
    if (rStat == "cost_multiplier")         return StatId_t::CostMultiplier;
    if (rStat == "probe_action_cost")       return StatId_t::ProbeActionCost;
    if (rStat == "probe_defense")           return StatId_t::ProbeDefense;
    if (rStat == "probe_failure_scale")     return StatId_t::ProbeFailureScale;
    if (rStat == "probe_success_scale")     return StatId_t::ProbeSuccessScale;
    if (rStat == "starting_experience")     return StatId_t::StartingExperience;
    if (rStat == "morale_bonus")            return StatId_t::MoraleBonus;
    if (rStat == "positive_morale_scale")   return StatId_t::PositiveMoraleScale;
    if (rStat == "growth_rate")             return StatId_t::GrowthRate;
    if (rStat == "tech_cost")               return StatId_t::TechCost;
    if (rStat == "moisture_tier")           return StatId_t::MoistureTier;
    if (rStat == "commerce_rate")           return StatId_t::CommerceRate;
    if (rStat == "council_votes")           return StatId_t::CouncilVotes;
    if (rStat == "commerce_energy_bonus")   return StatId_t::CommerceEnergyBonus;
    if (rStat == "inefficiency_denominator") return StatId_t::InefficiencyDenominator;
    throw std::runtime_error("Unknown stat id: '" + rStat + "'");
}

enum class SocialRatingId_t
{
    Economy,
    Efficiency,
    Support,
    Police,
    Morale,
    Growth,
    Planet,
    Research,
    Industry,
    Probe
};

// Snake_case JSON wire form differs from enumerator names — one explicit map next to the enum.
inline SocialRatingId_t ParseSocialRatingId(const std::string& rRating)
{
    if (rRating == "economy")    return SocialRatingId_t::Economy;
    if (rRating == "efficiency") return SocialRatingId_t::Efficiency;
    if (rRating == "support")    return SocialRatingId_t::Support;
    if (rRating == "police")     return SocialRatingId_t::Police;
    if (rRating == "morale")     return SocialRatingId_t::Morale;
    if (rRating == "growth")     return SocialRatingId_t::Growth;
    if (rRating == "planet")     return SocialRatingId_t::Planet;
    if (rRating == "research")   return SocialRatingId_t::Research;
    if (rRating == "industry")   return SocialRatingId_t::Industry;
    if (rRating == "probe")      return SocialRatingId_t::Probe;
    throw std::runtime_error("Unknown social rating id: '" + rRating + "'");
}

enum class RuleFlagId_t
{
    // Unit flags
    Flight,
    // After a successful use-action, ExpendIfSingleUse_ reports Expended and the caller
    // DestroyUnit's (PlayerActions under deferral, or TryAttack / TryFoundBase directly).
    // ResolveFlag ORs across every component on the design.
    SingleUse,
    IgnoreZoneOfControl,
    IgnoreDifficultTerrain,
    TreatFungusAsRoad,
    // Any combat involving a unit with this flag uses psi strengths and damage.
    ForcesPsiCombat,

    // Non-combat special equipment (weapon-slot) capability gates.
    FoundBase,
    Terraform,
    SupplyCrawl,
    ProbeTeam,

    // Sole capture veto: chassis (Needlejet / Missile) or noncombat weapon modules.
    CannotCaptureBases,
    // Attack spends all remaining moves (Needlejet: one strike per turn).
    AttackingEndsTurn,
    // Capturing this unit does not fully repair it (e.g. Battle Ogre).
    NoConquestRepair,
    // Base flag: skip population loss when the last defender falls.
    PreventsConquestPopLoss,

    // Unit / tile flags
    // Blocks the *opponent* from disengaging when carried ThisUnit (Comm Jammer), or blocks
    // a unit on this tile from disengaging when declared ThisTile (Base, Bunker, Airbase).
    PreventsDisengage,

    // Tile service capabilities. Declared ThisTile — by an improvement on the tile, or
    // projected onto the tile by a component of a friendly unit standing there (Carrier
    // Deck). Queried via TileProvidesFlag, never named directly by the consumer, so a new
    // site kind only has to declare the capability to participate.
    // Air units restore Fuel here.
    RefuelsAir,
    // Air transports may exchange cargo here. Separate from RefuelsAir because the two are
    // independent capabilities that SMAC happens to co-locate: Base and Airbase declare
    // both, while a Carrier Deck refuels without being a load site. Granting a carrier deck
    // this flag is the supported way to opt into loading at sea.
    LoadsAirTransport,

    // Faction/global flags
    PopulationBoom,
    NearZeroGrowth,
    // Children's Crèche at a base: softens negative morale_bonus for units home-based there.
    Creche,
    // Base is the faction headquarters (assassinate / MC eligibility).
    Headquarters,
    // SE Probe +3 / Thought Control: bases and units cannot be subverted by standard probes.
    ProbeSubversionImmune,
    // Target blocks probe actions unless the probe IgnoresProbeBlock (Hunter-Seeker, etc.).
    BlocksProbeTeams,
    // Probe may attempt actions against BlocksProbeTeams / ProbeSubversionImmune targets.
    IgnoresProbeBlock,

    // Map visibility. RemoveShroud permanently explores the map (satellite); RemoveFog
    // clears current fog while active (secret project). See VisibilityRules helpers.
    RemoveShroud,
    RemoveFog,

    // U.N. Charter: atrocities are illegal while this flag is in force planet-wide.
    AtrocitiesForbidden
};

// Snake_case JSON wire form differs from enumerator names — one explicit map next to the enum.
inline RuleFlagId_t ParseRuleFlagId(const std::string& rFlag)
{
    if (rFlag == "flight")                      return RuleFlagId_t::Flight;
    if (rFlag == "population_boom")             return RuleFlagId_t::PopulationBoom;
    if (rFlag == "near_zero_growth")            return RuleFlagId_t::NearZeroGrowth;
    if (rFlag == "remove_shroud")               return RuleFlagId_t::RemoveShroud;
    if (rFlag == "remove_fog")                  return RuleFlagId_t::RemoveFog;
    if (rFlag == "single_use")                  return RuleFlagId_t::SingleUse;
    if (rFlag == "ignore_zone_of_control")      return RuleFlagId_t::IgnoreZoneOfControl;
    if (rFlag == "ignores_difficult_terrain")   return RuleFlagId_t::IgnoreDifficultTerrain;
    if (rFlag == "treat_fungus_as_road")         return RuleFlagId_t::TreatFungusAsRoad;
    if (rFlag == "forces_psi_combat")            return RuleFlagId_t::ForcesPsiCombat;
    if (rFlag == "found_base")                   return RuleFlagId_t::FoundBase;
    if (rFlag == "terraform")                   return RuleFlagId_t::Terraform;
    if (rFlag == "supply_crawl")                return RuleFlagId_t::SupplyCrawl;
    if (rFlag == "probe_team")                  return RuleFlagId_t::ProbeTeam;
    if (rFlag == "cannot_capture_bases")        return RuleFlagId_t::CannotCaptureBases;
    if (rFlag == "attacking_ends_turn")         return RuleFlagId_t::AttackingEndsTurn;
    if (rFlag == "no_conquest_repair")          return RuleFlagId_t::NoConquestRepair;
    if (rFlag == "prevents_conquest_pop_loss")  return RuleFlagId_t::PreventsConquestPopLoss;
    if (rFlag == "prevents_disengage")          return RuleFlagId_t::PreventsDisengage;
    if (rFlag == "refuels_air")                 return RuleFlagId_t::RefuelsAir;
    if (rFlag == "loads_air_transport")         return RuleFlagId_t::LoadsAirTransport;
    if (rFlag == "creche")                      return RuleFlagId_t::Creche;
    if (rFlag == "headquarters")                return RuleFlagId_t::Headquarters;
    if (rFlag == "probe_subversion_immune")     return RuleFlagId_t::ProbeSubversionImmune;
    if (rFlag == "blocks_probe_teams")          return RuleFlagId_t::BlocksProbeTeams;
    if (rFlag == "ignores_probe_block")         return RuleFlagId_t::IgnoresProbeBlock;
    if (rFlag == "atrocities_forbidden")        return RuleFlagId_t::AtrocitiesForbidden;
    throw std::runtime_error("Unknown rule flag id: '" + rFlag + "'");
}

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

// True when AppendActiveEffects should record pOriginBase on the ActiveEffect_t.
// Derived from LaneFor (plus FactionUnits, which keeps origin for per-base conditions).
constexpr bool TagsOriginBase(EffectScope_t scope)
{
    const EffectLane_t lane = LaneFor(scope);
    return lane == EffectLane_t::Base
        || lane == EffectLane_t::ProducedAtBase
        || scope == EffectScope_t::FactionUnits;
}

enum class ModifierOp_t
{
    Add,
    // amount is in percent points (25 = +25%, -25 = -25%), matching the UI's bonus display.
    // All AddPercent contributions sum into a single arithmetic factor before the geometric step.
    AddPercent,
    MultiplyGeometric
};

// Instantaneous world-state mutation id (sea level / climate) for WorldParameterEffect_t.
enum class WorldParameterId_t
{
    SeaLevel,
};

enum class PermissionId_t
{
    // Lifts a channel-crossing attack that reachability already allows.
    Attack,
    // Lifts land -> water entry onto a qualifying tile (sea base).
    Enter,
};

// Restricts which *other* factions a cross-faction effect (Infiltration, future
// DiplomaticModifier, …) applies to. Orthogonal to EffectScope_t: scope is the resolution
// lane (FactionGlobal / WorldGlobal); factionFilter narrows the diplomatic target set.
enum class FactionFilterKind_t
{
    // Only the faction supplied as actionTarget at apply/query time (probe mission target).
    ActionTarget,
    // Other factions on the PlanetaryCouncil member list. Matches nobody when no council exists.
    CouncilMembers,
};

// Which kind of config declared an effects array. Used for the minimal load-time scope
// validation: scopes that can only ever be resolved against one source kind (a specific pop,
// a specific unit, or an origin base) are rejected on any other source instead of silently
// doing nothing.
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
    CouncilRules,
    ProbeAction,
    TileYieldRules,
};

} // namespace ac
