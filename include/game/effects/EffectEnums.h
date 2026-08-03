#pragma once

#include <stdexcept>

namespace ac
{

enum class StatId_t
{
    // Base resources
    Nutrients,
    Minerals,
    Energy,

    // Base output, allocated directly (specialists) rather than via energy allocation
    Econ,
    Labs,
    Psych,

    // Unit stats
    Attack,
    Defense,
    Movement,
    Vision,
    HitPoints,
    // Damage received per lost psi-combat round. Reactors set this to their tier.
    PsiDamage,
    DisengageChance,
    Fuel,
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
    CommerceEnergyBonus
    // TODO: add more stats as they are defined
};

// How a stat's modifier stack is seeded — the seed-semantics counterpart of LaneFor's scope
// routing (BonusEffect.h). Adding a StatId_t forces a kind decision in KindFor's exhaustive
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
        case StatId_t::Attack:
        case StatId_t::Defense:
        case StatId_t::Movement:
        case StatId_t::Vision:
        case StatId_t::HitPoints:
        case StatId_t::PsiDamage:
        case StatId_t::DisengageChance:
        case StatId_t::Fuel:
        case StatId_t::DamageFromOutOfFuel:
        case StatId_t::CargoCapacity:
        case StatId_t::DifficultTerrainCost:
        case StatId_t::StartingExperience:
        case StatId_t::MoraleBonus:
        case StatId_t::ProbeDefense:
        case StatId_t::TechCost:
        case StatId_t::CouncilVotes:
        case StatId_t::CommerceEnergyBonus:  return StatKind_t::Additive;
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

    // Conquest / amphibious assault.
    // Land units may attack and capture sea bases (not free ocean movement).
    Amphibious,
    // Sole capture veto: chassis (Needlejet / Missile) or noncombat weapon modules.
    CannotCaptureBases,
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

} // namespace ac
