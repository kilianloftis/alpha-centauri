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

    // Spendable faction treasury (`EconomyManager`). Not tile energy yield.
    EnergyCredits,

    // Base output: seed from this base's post-inefficiency energy split, plus flat Add
    // from facilities / specialists.
    Econ,
    Labs,
    Psych,

    // Composition modifiers: add/remove drones or talents at a base (Police SE, facilities).
    // Drones is drone *pressure*, not a headcount — a Super Drone body absorbs two of it.
    Drones,
    Talents,

    // Mood weights, resolved per seated pop from its own ThisPop effects. Only composition-pool
    // types declare them; specialists carry neither, which is what keeps them out of both sums.
    RiotWeight,
    GoldenAgeWeight,

    // Unit stats
    Attack,
    Defense,
    // Tile defense multiplier (terrain / sensors / bunkers). Distinct quantity from unit
    // Defense armor — combat folds ResolveTileDefenseMultiplier on top of unit Defense.
    TileDefense,
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
    // Minerals spent each turn to keep a live unit supported by its home base. Chassis
    // baseline is typically 1; abilities / FactionUnits SE can raise or zero it. Floor at 0.
    MineralUpkeep,
    // Free population before size-based drones (Additive). Difficulty emits 6…1; every pop
    // past this count is a size drone. Not a SMAC-style size divisor.
    SizeFreeDrones,
    // How many positive-upkeep home units a base may support at zero mineral cost.
    // Support SE levels emit absolute FreeUnitSupport Adds (level 0 = 2); facilities Add on top.
    FreeUnitSupport,
    // How many garrison combat units at this base may suppress drones (Additive). Police SE
    // levels emit absolute Adds (level 0 = 1).
    MaxPolice,
    // Drones one selected police unit suppresses (Additive). police_rules baseline +1 for
    // combat units; Police SE +3 and Non-Lethal Methods Add on top.
    PoliceEffectiveness,
    // Float weight toward away-from-home drones when the unit is off owner territory
    // (Additive). police_rules baseline +1 for combat units; Police SE MaxClamp /
    // MultiplyGeometric scale the per-base sum.
    AwayFromHomeDrones,
    CostMultiplier,
    // Scales the prototype mineral surcharge term only (PureMultiplier, seed 1.0). Production
    // cost uses baseCost * CostMultiplier * (1 + surchargePercent/100 * this). Skunkworks emits
    // MultiplyGeometric 0 on ThisBase so the extra is cancelled without making the unit free.
    PrototypeSurchargeScale,
    // Scales the retool mineral forfeit (PureMultiplier, seed 1.0). ApplyRetoolPenalty_ uses
    // stockpile * retoolPercent/100 * this. Skunkworks emits MultiplyGeometric 0 on ThisBase.
    RetoolPenaltyScale,
    // Constructed-facility energy upkeep (RawScaled: seed is BuildingConfig_t::upkeep).
    // Tech tiers emit Add; percent cuts (and later difficulty MaxClamp) scale the raw value.
    // Resolved per building type with optional buildingFilter.
    FacilityEnergyUpkeep,
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
    // Minerals credited to a newly founded base's production stockpile (resolved once at
    // founding from the new base's effect list plus the founding unit). Not a live yield.
    StartingMinerals,
    // Live morale-level offset (SE Morale, Creche in-base, etc.). Added to Unit::m_xp when
    // computing effective combat morale; not seeded into m_xp.
    MoraleBonus,
    // Scales positive *conditional* morale_bonus contributions (Creche in-base, etc.).
    // PureMultiplier (seed 1.0); SE Morale ≤ -2 uses AddPercent -50 ("+ modifiers halved").
    PositiveMoraleScale,
    // Post-combat promotion probability (RawScaled). Resolve site seeds D/(A+D) from final
    // combat weapon/armor strengths; morale level effects apply MinClamp / MultiplyGeometric /
    // MaxClamp. Not resolved during combat stat passes.
    PromotionChance,

    // Population growth rate modifier (AddPercent, base = 100%)
    GrowthRate,

    // Research tech cost percentage modifier (Add, base = 0; negative = cheaper)
    TechCost,

    // Difficulty ordinal fed to tech_cost.lua as `diff` (Additive; shipping 1=Citizen … 5=
    // Transcend). Difficulty-only; not a percentage and not stacked with TechCost.
    TechCostDiff,

    // Tile terrain mutation (resolved back into Tile::SetMoisture, not a runtime-queried stat)
    MoistureTier,

    // Planetary commerce income multiplier (PureMultiplier; Global Trade Pact uses AddPercent).
    CommerceRate,
    // Bureaucracy base-limit product factor (PureMultiplier; seed 1.0). Difficulty and
    // Efficiency SE emit MultiplyGeometric; pop_composition.json multiplies by map root.
    Bureaucracy,
    // Extra council votes (Additive). Population elections seed with total population;
    // representative elections seed with 1. Buildings / projects / faction bonuses modify this.
    CouncilVotes,
    // Bonus energy credited per commerce transaction at each base (Additive; Planetary Governor).
    CommerceEnergyBonus,

    // Absolute denominator for energy inefficiency: loss = Energy × Distance / denom.
    // Efficiency SE levels emit this as Add with the table value (64, 56, …, 0). Denom ≤ 0
    // means 100% loss. Resolved from the efficiency rating table, not stacked as a live seed.
    InefficiencyDenominator,

    // Player-scrap refund after the kind formula (or config override). RawScaled: seed is
    // the formula amount. Add / AddPercent stack, then production.json refund_ceiling_percent
    // clamps. Not a live yield.
    ScrapRefund,

    // Population removed when the last defender of a base falls (Additive). base_conquest.json
    // supplies the baseline Add; Perimeter Defense and Citizen difficulty emit MaxClamp 0.
    // Floored at 0 at the resolve site — population cannot go negative.
    LastDefenderPopLoss,

    // Lower bound on facilities destroyed when a base is captured (Additive).
    // base_conquest.json Adds the baseline; clamped against the eligible count at the
    // resolve site, so no modifier can invert the range.
    CaptureFacilitiesDestroyedMin,

    // Upper bound on facilities destroyed on capture, as a percent of the eligible count
    // (Additive — the value is a percent, so a percent-of-a-percent AddPercent would not
    // mean what a modder expects; use Add). Clamped against the eligible count.
    CaptureFacilitiesDestroyedMaxPercent,

    // Population removed when a base is captured by a same-species faction (Additive).
    // base_conquest.json supplies the baseline Add. Independent of LastDefenderPopLoss —
    // nothing in the shipping config modifies it. Floored at 0 at the resolve site.
    CapturePopLoss,

    // Offset on the recently-conquered drone cap (Additive). The formula is
    // floor(base_size/4 + this), i.e. (BaseSize + Difficulty - 2) / 4 when difficulty
    // Adds 0.25 per level (Citizen = 1) and base_conquest.json Adds -0.5. Passed to Lua
    // as a double so the 0.25 steps are not rounded away before the cap is taken.
    ConqueredDroneCap,

    // Ecological damage accrued from terraforming / population (RawScaled: seed is the raw
    // accrued amount the resolve site holds). Difficulty emits MultiplyGeometric.
    EcologicalDamage,

    // Weight toward receiving a rebelling base (Additive, Faction domain). RebelFactionPicker
    // resolves with seed 1.0 so factions without modifiers still participate equally.
    RebelJoinWeight
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
    // MoistureTier's base tier, FacilityEnergyUpkeep's building upkeep,
    // ScrapRefund's formula amount). No universal seed
    // exists — SeedFor throws, forcing the caller to pass the raw value explicitly.
    RawScaled,
};

constexpr StatKind_t KindFor(StatId_t stat)
{
    switch (stat)
    {
        case StatId_t::Nutrients:
        case StatId_t::Minerals:
        case StatId_t::Energy:
        case StatId_t::EnergyCredits:
        case StatId_t::Econ:
        case StatId_t::Labs:
        case StatId_t::Psych:
        case StatId_t::Drones:
        case StatId_t::Talents:
        case StatId_t::RiotWeight:
        case StatId_t::GoldenAgeWeight:
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
        case StatId_t::MineralUpkeep:
        case StatId_t::FreeUnitSupport:
        case StatId_t::MaxPolice:
        case StatId_t::PoliceEffectiveness:
        case StatId_t::AwayFromHomeDrones:
        case StatId_t::StartingExperience:
        case StatId_t::StartingMinerals:
        case StatId_t::MoraleBonus:
        case StatId_t::ProbeDefense:
        case StatId_t::TechCost:
        case StatId_t::LastDefenderPopLoss:
        case StatId_t::CapturePopLoss:
        case StatId_t::ConqueredDroneCap:
        case StatId_t::CaptureFacilitiesDestroyedMin:
        case StatId_t::CaptureFacilitiesDestroyedMaxPercent:
        case StatId_t::TechCostDiff:
        case StatId_t::SizeFreeDrones:
        case StatId_t::CouncilVotes:
        case StatId_t::CommerceEnergyBonus:
        case StatId_t::InefficiencyDenominator:
        case StatId_t::RebelJoinWeight: return StatKind_t::Additive;
        case StatId_t::CostMultiplier:
        case StatId_t::PrototypeSurchargeScale:
        case StatId_t::RetoolPenaltyScale:
        case StatId_t::ProbeActionCost:
        case StatId_t::ProbeFailureScale:
        case StatId_t::ProbeSuccessScale:
        case StatId_t::PositiveMoraleScale:
        case StatId_t::CommerceRate:
        case StatId_t::Bureaucracy:
        case StatId_t::TileDefense:          return StatKind_t::PureMultiplier;
        case StatId_t::PromotionChance:
        case StatId_t::GrowthRate:
        case StatId_t::MoistureTier:
        case StatId_t::FacilityEnergyUpkeep:
        case StatId_t::ScrapRefund:
        case StatId_t::EcologicalDamage:    return StatKind_t::RawScaled;
    }
    return StatKind_t::Additive; // unreachable; all enumerators handled above
}

// The ResolveStatModifiers seed for a context-free resolve of `stat`: 0.0 for Additive,
// 1.0 for PureMultiplier. RawScaled stats throw — their resolve site passes the raw value
// being scaled instead. A site that deliberately resolves an Additive stat against a raw
// base (tile yield's elevation energy seed, pop tile multipliers, ResourceManager worked
// totals) also passes its seed explicitly rather than calling this.
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

// Consumer-side subject for typed resolve / amount_source evaluation — distinct from
// EffectLane_t / LaneFor (producer routing). DomainFor is which subject resolves the
// number, not which single effect list may contribute: sources may still emit from any
// pool; the consumer assembles applicable lists then resolves under one subject.
enum class ResolveDomain_t
{
    Base,
    Faction,
    Unit,
    Tile,
    // Amount-source subject for MineralsConverted (stockpile conversion event). No StatId
    // uses DomainFor → Stockpile; resource stats stay Base.
    Stockpile,
};

constexpr ResolveDomain_t DomainFor(StatId_t stat)
{
    switch (stat)
    {
        case StatId_t::Nutrients:
        case StatId_t::Minerals:
        case StatId_t::Energy:
        case StatId_t::EnergyCredits:
        case StatId_t::Econ:
        case StatId_t::Labs:
        case StatId_t::Psych:
        case StatId_t::Drones:
        case StatId_t::Talents:
        case StatId_t::RiotWeight:
        case StatId_t::GoldenAgeWeight:
        case StatId_t::SizeFreeDrones:
        case StatId_t::FreeUnitSupport:
        case StatId_t::MaxPolice:
        case StatId_t::CostMultiplier:
        case StatId_t::PrototypeSurchargeScale:
        case StatId_t::RetoolPenaltyScale:
        case StatId_t::FacilityEnergyUpkeep:
        case StatId_t::ProbeActionCost:
        case StatId_t::ProbeDefense:
        case StatId_t::ProbeSuccessScale:
        case StatId_t::StartingMinerals:
        case StatId_t::GrowthRate:
        case StatId_t::Bureaucracy:
        case StatId_t::CommerceEnergyBonus:
        case StatId_t::InefficiencyDenominator:
        case StatId_t::ScrapRefund:
        case StatId_t::LastDefenderPopLoss:
        case StatId_t::CaptureFacilitiesDestroyedMin:
        case StatId_t::CaptureFacilitiesDestroyedMaxPercent:
        case StatId_t::CapturePopLoss:
        case StatId_t::ConqueredDroneCap:
        case StatId_t::EcologicalDamage: return ResolveDomain_t::Base;

        case StatId_t::TechCost:
        case StatId_t::TechCostDiff:
        case StatId_t::CommerceRate:
        case StatId_t::CouncilVotes:
        case StatId_t::RebelJoinWeight: return ResolveDomain_t::Faction;

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
        case StatId_t::MineralUpkeep:
        case StatId_t::PoliceEffectiveness:
        case StatId_t::AwayFromHomeDrones:
        case StatId_t::ProbeFailureScale:
        case StatId_t::StartingExperience:
        case StatId_t::MoraleBonus:
        case StatId_t::PositiveMoraleScale:
        case StatId_t::PromotionChance: return ResolveDomain_t::Unit;

        case StatId_t::MoistureTier:
        case StatId_t::TileDefense: return ResolveDomain_t::Tile;
    }
    return ResolveDomain_t::Base; // unreachable; all enumerators handled above
}

// Snake_case JSON wire form differs from enumerator names — one explicit map next to the enum.
inline StatId_t ParseStatId(const std::string& rStat)
{
    if (rStat == "nutrients")               return StatId_t::Nutrients;
    if (rStat == "minerals")                return StatId_t::Minerals;
    if (rStat == "energy")                  return StatId_t::Energy;
    if (rStat == "energy_credits")          return StatId_t::EnergyCredits;
    if (rStat == "econ")                    return StatId_t::Econ;
    if (rStat == "labs")                    return StatId_t::Labs;
    if (rStat == "psych")                   return StatId_t::Psych;
    if (rStat == "drones")                  return StatId_t::Drones;
    if (rStat == "talents")                 return StatId_t::Talents;
    if (rStat == "riot_weight")             return StatId_t::RiotWeight;
    if (rStat == "golden_age_weight")       return StatId_t::GoldenAgeWeight;
    if (rStat == "size_free_drones")        return StatId_t::SizeFreeDrones;
    if (rStat == "attack")                  return StatId_t::Attack;
    if (rStat == "defense")                 return StatId_t::Defense;
    if (rStat == "tile_defense")            return StatId_t::TileDefense;
    if (rStat == "movement")                return StatId_t::Movement;
    if (rStat == "vision")                  return StatId_t::Vision;
    if (rStat == "hit_points")              return StatId_t::HitPoints;
    if (rStat == "psi_damage")              return StatId_t::PsiDamage;
    if (rStat == "disengage_chance")        return StatId_t::DisengageChance;
    if (rStat == "turns_of_fuel")           return StatId_t::TurnsOfFuel;
    if (rStat == "damage_from_out_of_fuel") return StatId_t::DamageFromOutOfFuel;
    if (rStat == "cargo_capacity")          return StatId_t::CargoCapacity;
    if (rStat == "difficult_terrain_cost")  return StatId_t::DifficultTerrainCost;
    if (rStat == "mineral_upkeep")          return StatId_t::MineralUpkeep;
    if (rStat == "free_unit_support")       return StatId_t::FreeUnitSupport;
    if (rStat == "max_police")              return StatId_t::MaxPolice;
    if (rStat == "police_effectiveness")    return StatId_t::PoliceEffectiveness;
    if (rStat == "away_from_home_drones")   return StatId_t::AwayFromHomeDrones;
    if (rStat == "cost_multiplier")         return StatId_t::CostMultiplier;
    if (rStat == "prototype_surcharge_scale") return StatId_t::PrototypeSurchargeScale;
    if (rStat == "retool_penalty_scale")     return StatId_t::RetoolPenaltyScale;
    if (rStat == "facility_energy_upkeep")  return StatId_t::FacilityEnergyUpkeep;
    if (rStat == "probe_action_cost")       return StatId_t::ProbeActionCost;
    if (rStat == "probe_defense")           return StatId_t::ProbeDefense;
    if (rStat == "probe_failure_scale")     return StatId_t::ProbeFailureScale;
    if (rStat == "probe_success_scale")     return StatId_t::ProbeSuccessScale;
    if (rStat == "starting_experience")     return StatId_t::StartingExperience;
    if (rStat == "starting_minerals")       return StatId_t::StartingMinerals;
    if (rStat == "morale_bonus")            return StatId_t::MoraleBonus;
    if (rStat == "positive_morale_scale")   return StatId_t::PositiveMoraleScale;
    if (rStat == "promotion_chance")        return StatId_t::PromotionChance;
    if (rStat == "growth_rate")             return StatId_t::GrowthRate;
    if (rStat == "tech_cost")               return StatId_t::TechCost;
    if (rStat == "tech_cost_diff")          return StatId_t::TechCostDiff;
    if (rStat == "moisture_tier")           return StatId_t::MoistureTier;
    if (rStat == "commerce_rate")           return StatId_t::CommerceRate;
    if (rStat == "bureaucracy")             return StatId_t::Bureaucracy;
    if (rStat == "council_votes")           return StatId_t::CouncilVotes;
    if (rStat == "commerce_energy_bonus")   return StatId_t::CommerceEnergyBonus;
    if (rStat == "inefficiency_denominator") return StatId_t::InefficiencyDenominator;
    if (rStat == "scrap_refund")             return StatId_t::ScrapRefund;
    if (rStat == "last_defender_pop_loss")   return StatId_t::LastDefenderPopLoss;
    if (rStat == "capture_pop_loss")         return StatId_t::CapturePopLoss;
    if (rStat == "conquered_drone_cap")      return StatId_t::ConqueredDroneCap;
    if (rStat == "capture_facilities_destroyed_min")
        return StatId_t::CaptureFacilitiesDestroyedMin;
    if (rStat == "capture_facilities_destroyed_max_percent")
        return StatId_t::CaptureFacilitiesDestroyedMaxPercent;
    if (rStat == "ecological_damage")        return StatId_t::EcologicalDamage;
    if (rStat == "rebel_join_weight")         return StatId_t::RebelJoinWeight;
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
    AtrocitiesForbidden,

    // Base cannot bank minerals into production, complete construction, or hurry.
    // ConvertMinerals still drains the leftover bank (discarded); stockpile is preserved.
    DisableProduction,
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
    if (rFlag == "prevents_disengage")          return RuleFlagId_t::PreventsDisengage;
    if (rFlag == "refuels_air")                 return RuleFlagId_t::RefuelsAir;
    if (rFlag == "loads_air_transport")         return RuleFlagId_t::LoadsAirTransport;
    if (rFlag == "creche")                      return RuleFlagId_t::Creche;
    if (rFlag == "headquarters")                return RuleFlagId_t::Headquarters;
    if (rFlag == "probe_subversion_immune")     return RuleFlagId_t::ProbeSubversionImmune;
    if (rFlag == "blocks_probe_teams")          return RuleFlagId_t::BlocksProbeTeams;
    if (rFlag == "ignores_probe_block")         return RuleFlagId_t::IgnoresProbeBlock;
    if (rFlag == "atrocities_forbidden")        return RuleFlagId_t::AtrocitiesForbidden;
    if (rFlag == "disable_production")          return RuleFlagId_t::DisableProduction;
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
    // ResolveTileDefenseMultiplier (TileDefense) and must never enter the base-wide
    // active effects pool.
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
    MultiplyGeometric,
    // Clamps bound the value *after* the Add / AddPercent / MultiplyGeometric math. The
    // tightest clamp of each kind wins; when a MinClamp and a MaxClamp cross, MinClamp wins.
    // Clamp the resolved stat value to be <= amount.
    MaxClamp,
    // Clamp the resolved stat value to be >= amount.
    MinClamp,
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
    // Matches factions by whether they are player-controlled or AI-controlled. When used,
    // the concrete value is stored on the FactionFilter_t as PlayerType (see EffectConfig.h).
    PlayerType,
};

// Whether the faction is a human player or an AI.
enum class PlayerType_t
{
    Player,
    AI,
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
    Tech,
    Production,
    Stockpile,
    Difficulty,
    BaseConquest,
    PoliceRules,
    MoraleLevel,
    // pop_composition.json's faction-wide `effects` array: enters the pool with no origin base.
    PopComposition,
    // pop_composition.json's per-base mood arrays (riot_tiers, golden_age_effects): collected
    // against a specific base, so ThisBase is legal here and rejected for PopComposition.
    PopCompositionBaseLocal,
};

} // namespace ac
