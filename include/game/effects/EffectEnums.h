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
    // HP removed from each other occupant when this unit kills a defender. Reactors set
    // this to their tier; native life Adds 1. Combat reads the attacker's effects only.
    CollateralDamage,
    // Levels an earthquake raises. Reactors set this to their tier; the Earthquake triggered
    // effect reads it off the detonating unit via `levels_stat`.
    EarthquakeLevels,
    // Tiles a fungal bloom converts. Reactors set 3, 5, 7, 9; the FungalBloom triggered
    // effect reads it off the detonating unit via `tiles_stat`.
    FungalBloomTiles,
    // Chebyshev radius of an explosion. Reactors set 1, 2, 3, 4; the Explosion triggered
    // effect reads it off the detonating unit via `radius_stat`. Radius 1 is 9 tiles.
    ExplosionRadius,
    // Pure multiplier on incoming collateral. Seed 1. A tile MaxClamp that leaves 0 skips
    // that occupant. An air unit's own geometric 0 does not.
    CollateralSusceptibility,
    // Energy credits paid to the killer for a destroyed wild native. The design Adds the
    // base; the intrinsic lifecycle level MultiplyGeometrics it (10, 20, 30, …).
    PlanetPearls,
    DisengageChance,
    // Percent chance per qualifying round that a Marine Detachment boarding succeeds; 0 cannot
    // board.
    BoardChance,
    // Share of the defender's starting HP it must lose before boarding rolls start (a fraction,
    // read unrounded).
    BoardHpLimit,
    // Turns of fuel capacity; max fuel pool = TurnsOfFuel × Movement. 0 = unlimited / no tracking.
    TurnsOfFuel,
    // Percent of max HP applied when a fueled unit ends a turn at 0 fuel away from a refuel site.
    DamageFromOutOfFuel,
    // Chebyshev airdrop range from the launch tile (Drop Pods). 0 = cannot airdrop by range
    // alone; OrbitalInsertion ignores this cap.
    AirdropRange,
    // Percent of max HP applied as flat landing damage after an airdrop (skipped on
    // airdrop_launch pads). Drop Pods baseline; reactors may Add further.
    AirdropLandingDamage,
    // Chebyshev bombard range from the firing tile. 0 = cannot fire.
    BombardRange,
    // Percent chance each Destroy Enhancement attempt succeeds (Additive; seed 100 in Rules).
    // Air chassis Add -25 for SMAC's 25% fail; tile auras can cut it further.
    DestroyEnhancementSuccess,
    // How many enhancements one Destroy Enhancement order may attempt (Additive; Rules seed 1).
    DestroyEnhancementCount,
    CargoCapacity,
    DifficultTerrainCost,
    // Ceiling on a tile's entry price, in move points ("1/3", 1, 0); MaxClamp only. The tightest
    // matching clamp wins, never raises a lower cost, and cancels fungus entry rules.
    MoveCost,
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
    // Weight toward away-from-home drones while off owner territory (Additive). Police SE scales
    // the per-base sum.
    AwayFromHomeDrones,
    CostMultiplier,
    // Scales only the prototype surcharge (PureMultiplier). Skunkworks multiplies it by 0 without
    // making the unit free.
    PrototypeSurchargeScale,
    // Scales the retool mineral forfeit (PureMultiplier, seed 1.0). ApplyRetoolPenalty_ uses
    // stockpile * retoolPercent/100 * this. Skunkworks emits MultiplyGeometric 0 on ThisBase.
    RetoolPenaltyScale,
    // Facility energy upkeep (RawScaled from BuildingConfig_t::upkeep), per building type with an
    // optional buildingFilter.
    FacilityEnergyUpkeep,
    // The target side of a mind-control or subversion cost (PureMultiplier), resolved on the base
    // or unit being probed with the actor as partner, so scope picks the quote: SE Probe, riot,
    // golden age and former owner reach bases; the plan baseline, Poly and SE Probe's
    // FactionUnits copies reach units.
    ProbeActionCost,
    // The actor side (PureMultiplier, Faction), resolved on the faction paying with the target
    // faction as partner: Nethack, Pact, the AI-vs-player discount, atrocity victim.
    ProbeActorCost,
    // The HQ distance in a mind-control or subversion cost (PureMultiplier), resolved like
    // ProbeActionCost and truncated: Genejack x2 and Children's Creche x0.5 on bases, Punishment
    // Sphere x0.5 on its base and the units homed there.
    ProbeDistanceScale,
    // Additive local probe defense (Covert Ops Center +2, etc.). Added to SE Probe before
    // the success/escape clamp.
    ProbeDefense,
    // Scales mission/escape failure rates for the acting probe (PureMultiplier). Algorithmic
    // Enhancement emits AddPercent -50. Void while the target's hunter neutralises the probe.
    ProbeFailureScale,
    // Scales mission/escape success rates against this target (PureMultiplier). Hunter-Seeker
    // Algorithm emits AddPercent -50.
    ProbeSuccessScale,
    // The target's side of the probe contest (Additive). A probe whose intrusion it exceeds dies
    // on contact; one it matches loses its intrusion bonuses. Hunter-Seeker Algorithm Adds 1.
    ProbeHunter,
    // The probe's side of the contest (Additive). Algorithmic Enhancement Adds 1.
    ProbeIntrusion,
    // Percent chance a successful mission or escape draw against this target is busted anyway
    // (Additive). Hunter-Seeker Algorithm Adds 50.
    ProbeSuccessBust,
    // Percent chance the probe's failed mission or escape draw is saved (Additive). Void while the
    // target's hunter neutralises the probe. Algorithmic Enhancement Adds 50.
    ProbeFailureSave,
    // A unit's count in the base mind-control garrison (Additive). probe_actions.json Adds 1
    // for offense and combat plans.
    GarrisonProbeWeight,
    // A unit's addition to the garrison multiplier, which starts at the cost's
    // garrison_multiplier_base (Additive). Polymorphic Encryption Adds 1.
    GarrisonProbeMultiplier,
    // PureMultiplier on genetic-plague pop and garrison damage. Hospitals emit ×0.5.
    PlagueDamageScale,
    // Minerals credited to a newly founded base's production stockpile (resolved once at
    // founding from the new base's effect list plus the founding unit). Not a live yield.
    StartingMinerals,
    // Shifts the displayed rank word (morale_veh): SE table, home Crèche, riot in combat.
    // Added to Unit::m_xp and clamped; not seeded into m_xp.
    MoraleRank,
    // Extra morale levels after the rank word; the name does not change. Attack and defence rows
    // split on IsDefending.
    MoraleExtra,
    // Post-combat promotion probability (RawScaled from D/(A+D)). Not resolved during combat.
    PromotionChance,

    // Population growth rate modifier (AddPercent, base = 100%)
    GrowthRate,

    // Population when a base is founded if the caller does not override size (Additive).
    // pop_growth.json baselines at Add 1 (AllOwnerBases).
    StartingSize,

    // Soft population cap for CanGrow (Additive). pop_growth.json baselines at Add 7;
    // Hab Complex Adds further; Hab Dome Adds a large amount (classic hard cap ~127).
    MaxBaseSize,

    // Research cost scale (PureMultiplier, seed 1). Faction / ThisTech / custom-game
    // stagnation MultiplyGeometric; applied after tech_cost.lua returns.
    TechCost,

    // Difficulty ordinal fed to tech_cost.lua as `diff` (Additive; shipping 1=Citizen … 5=
    // Transcend). Difficulty-only; not a percentage and not stacked with TechCost.
    TechCostDiff,
    // Catch-up ordinal fed to tech_cost.lua as `catch_up_diff` (Additive; shipping 0=Citizen
    // … 5=Transcend). Talent and Librarian share TechCostDiff but not this.
    TechCostCatchUp,
    // Slower tech_rate turn lag (Additive; nonzero is on).
    TechCostTurnLag,
    // Inherent SOCIAL RESEARCH index fed to tech_cost.lua as `research` (Additive).
    // Subtracted from the local tech factor. Not the live Research SE rating.
    TechCostResearch,

    // Difficulty band for social-engineering upheaval (Additive). Difficulty emits player
    // Citizen…Transcend as 0…5 and AI as 3 (Librarian). Multiplies the upheaval cost_formula.
    UpheavalCost,
    // Scales the formula×UpheavalCost product (RawScaled). Progenitor factions emit +50%.
    UpheavalCostScale,

    // Tile terrain mutation (resolved back into Tile::SetMoisture, not a runtime-queried stat)
    MoistureTier,

    // Planetary commerce income multiplier (PureMultiplier; Global Trade Pact uses AddPercent).
    CommerceRate,
    // Faction & Economy SE bonuses folded into the commerce tech ratio (Additive; Base domain
    // so social-rating level expansions in BaseEffectsCache resolve via ResolveBaseStat).
    CommerceRating,
    // Bureaucracy base-limit product factor (PureMultiplier; seed 1.0). Difficulty and
    // Efficiency SE emit MultiplyGeometric; pop_composition.json multiplies by map root.
    Bureaucracy,
    // Extra council votes (Additive). Population elections seed with total population;
    // representative elections seed with 1. Buildings / projects / faction bonuses modify this.
    CouncilVotes,
    // Bonus energy credited per commerce transaction at each base (Additive; Planetary Governor).
    CommerceEnergyBonus,

    // Energy inefficiency denominator: loss = Energy × Distance / denom, and denom ≤ 0 loses all.
    // Set from the efficiency rating table.
    InefficiencyDenominator,

    // Labs↔econ slider-imbalance scale from the efficiency rating table (Additive; 8…0 for −4…+4).
    AllocationImbalanceFactor,

    // Scrap refund after the kind formula (RawScaled), capped by refund_ceiling_percent.
    ScrapRefund,

    // Population lost when a base's last defender falls (Additive), floored at 0.
    LastDefenderPopLoss,

    // Fewest facilities destroyed on capture (Additive), clamped to the eligible count.
    CaptureFacilitiesDestroyedMin,

    // Most facilities destroyed on capture, as a percent of the eligible count (Additive; use Add,
    // since AddPercent of a percent misleads).
    CaptureFacilitiesDestroyedMaxPercent,

    // Artillery strike floor as a percent of the defender's hit_points (Additive; AddPercent is
    // rejected). The highest MinClamp wins, and the strike ceils the result.
    BombardMinHpPercent,

    // Population lost when a same-species faction captures a base (Additive), floored at 0.
    CapturePopLoss,

    // Offset on the recently-conquered drone cap floor(base_size/4 + this) (Additive). A double,
    // so difficulty's 0.25 steps survive until the cap is taken.
    ConqueredDroneCap,

    // The eco-damage multiplier (PureMultiplier); the eco score multiplies by it once.
    EcologicalDamage,

    // Scales Planet fauna boat cargo (PureMultiplier, seed 1). Perihelion MultiplyGeometric 2.
    PlanetFaunaCargoScale,

    // An improvement's weight toward its base's terraform sum (Additive, Tile), summed per tile.
    EcoDamageContribution,
    // The extra weight counted only while the base's own pops work the tile (Additive, Tile).
    // A supply-crawled tile is not worked by the base, so it counts the unworked weight only.
    EcoDamageWorkedContribution,
    // Scales the terraform term (PureMultiplier, Base). Tree Farm halves it, Hybrid Forest
    // zeroes it.
    EcoTerraformScale,
    // Baseline clean-minerals cap (Additive, Faction). Blooms and grants add on top from the
    // EcologyLedger.
    EcoCleanMinerals,
    // Goodfacs: the mineral term is divided by (1 + this) (Additive, Base).
    EcoDamageReduction,
    // Signed offset on the minerals the eco formula charges (Additive, Base). Negative for
    // minerals that do not answer to ecology (orbital), positive for a dirty facility.
    EcoMineralOffset,

    // Weight toward receiving a rebelling base (Additive, Faction domain). RebelCandidateWeights
    // seeds it with rebel_selection's base join weight, so factions without modifiers take part.
    RebelJoinWeight,

    // A gift's value as its giver weighs it (RawScaled, Faction), resolved on the giver with the
    // recipient as partner and GiftKind naming the item's kind.
    GiftGivenValue,
    // The same value as its recipient weighs it (RawScaled, Faction), seeded with the giver's.
    GiftReceivedValue,
    // Scales the thresholds a gift's size is tested against (PureMultiplier, Faction), resolved on
    // the giver. Friction never reads it.
    GiftSizeThreshold
};

// How a stat's modifier stack is seeded. KindFor's switch is exhaustive, so a new stat must
// choose, and no pure-multiplier stat can default to a 0.0 seed.
enum class StatKind_t
{
    // Contributions add onto an empty base; the context-free seed is 0.0.
    Additive,
    // The stat IS a multiplier, resolved purely through AddPercent/MultiplyGeometric
    // contributions; the seed is the identity 1.0 (a 0.0 seed collapses the result to 0).
    PureMultiplier,
    // Modifiers scale a raw value only the resolve site knows, so SeedFor throws.
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
        case StatId_t::CollateralDamage:
        case StatId_t::EarthquakeLevels:
        case StatId_t::FungalBloomTiles:
        case StatId_t::ExplosionRadius:
        case StatId_t::PlanetPearls:
        case StatId_t::DisengageChance:
        case StatId_t::BoardChance:
        case StatId_t::BoardHpLimit:
        case StatId_t::TurnsOfFuel:
        case StatId_t::DamageFromOutOfFuel:
        case StatId_t::AirdropRange:
        case StatId_t::AirdropLandingDamage:
        case StatId_t::BombardRange:
        case StatId_t::DestroyEnhancementSuccess:
        case StatId_t::DestroyEnhancementCount:
        case StatId_t::CargoCapacity:
        case StatId_t::DifficultTerrainCost:
        case StatId_t::MineralUpkeep:
        case StatId_t::FreeUnitSupport:
        case StatId_t::MaxPolice:
        case StatId_t::PoliceEffectiveness:
        case StatId_t::AwayFromHomeDrones:
        case StatId_t::StartingMinerals:
        case StatId_t::StartingSize:
        case StatId_t::MaxBaseSize:
        case StatId_t::MoraleRank:
        case StatId_t::MoraleExtra:
        case StatId_t::ProbeDefense:
        case StatId_t::ProbeHunter:
        case StatId_t::ProbeIntrusion:
        case StatId_t::ProbeSuccessBust:
        case StatId_t::ProbeFailureSave:
        case StatId_t::GarrisonProbeWeight:
        case StatId_t::GarrisonProbeMultiplier:
        case StatId_t::LastDefenderPopLoss:
        case StatId_t::CapturePopLoss:
        case StatId_t::ConqueredDroneCap:
        case StatId_t::CaptureFacilitiesDestroyedMin:
        case StatId_t::CaptureFacilitiesDestroyedMaxPercent:
        case StatId_t::BombardMinHpPercent:
        case StatId_t::TechCostDiff:
        case StatId_t::TechCostCatchUp:
        case StatId_t::TechCostTurnLag:
        case StatId_t::TechCostResearch:
        case StatId_t::UpheavalCost:
        case StatId_t::SizeFreeDrones:
        case StatId_t::CouncilVotes:
        case StatId_t::CommerceEnergyBonus:
        case StatId_t::CommerceRating:
        case StatId_t::InefficiencyDenominator:
        case StatId_t::AllocationImbalanceFactor:
        case StatId_t::EcoDamageContribution:
        case StatId_t::EcoDamageWorkedContribution:
        case StatId_t::EcoCleanMinerals:
        case StatId_t::EcoDamageReduction:
        case StatId_t::EcoMineralOffset:
        case StatId_t::RebelJoinWeight: return StatKind_t::Additive;
        case StatId_t::CostMultiplier:
        case StatId_t::PrototypeSurchargeScale:
        case StatId_t::RetoolPenaltyScale:
        case StatId_t::ProbeActionCost:
        case StatId_t::ProbeActorCost:
        case StatId_t::ProbeFailureScale:
        case StatId_t::ProbeSuccessScale:
        case StatId_t::ProbeDistanceScale:
        case StatId_t::PlagueDamageScale:
        case StatId_t::TechCost:
        case StatId_t::CommerceRate:
        case StatId_t::Bureaucracy:
        case StatId_t::TileDefense:
        case StatId_t::EcologicalDamage:
        case StatId_t::PlanetFaunaCargoScale:
        case StatId_t::EcoTerraformScale:
        case StatId_t::GiftSizeThreshold:
        case StatId_t::CollateralSusceptibility: return StatKind_t::PureMultiplier;
        case StatId_t::PromotionChance:
        case StatId_t::GrowthRate:
        case StatId_t::MoistureTier:
        case StatId_t::FacilityEnergyUpkeep:
        case StatId_t::ScrapRefund:
        case StatId_t::UpheavalCostScale:
        case StatId_t::GiftGivenValue:
        case StatId_t::GiftReceivedValue:
        case StatId_t::MoveCost:             return StatKind_t::RawScaled;
    }
    return StatKind_t::Additive; // unreachable; all enumerators handled above
}

// The context-free seed for `stat`: 0.0 for Additive, 1.0 for PureMultiplier; RawScaled
// throws. Sites with their own raw base pass it explicitly.
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

// Which subject resolves a stat. Effects may come from any pool; the consumer resolves them
// under one subject.
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
        case StatId_t::ProbeHunter:
        case StatId_t::ProbeSuccessBust:
        case StatId_t::ProbeDistanceScale:
        case StatId_t::PlagueDamageScale:
        case StatId_t::StartingMinerals:
        case StatId_t::GrowthRate:
        case StatId_t::StartingSize:
        case StatId_t::MaxBaseSize:
        case StatId_t::Bureaucracy:
        case StatId_t::CommerceEnergyBonus:
        case StatId_t::CommerceRating:
        case StatId_t::InefficiencyDenominator:
        case StatId_t::AllocationImbalanceFactor:
        case StatId_t::ScrapRefund:
        case StatId_t::LastDefenderPopLoss:
        case StatId_t::CaptureFacilitiesDestroyedMin:
        case StatId_t::CaptureFacilitiesDestroyedMaxPercent:
        case StatId_t::CapturePopLoss:
        case StatId_t::ConqueredDroneCap:
        case StatId_t::EcologicalDamage:
        case StatId_t::EcoTerraformScale:
        case StatId_t::EcoDamageReduction:
        case StatId_t::EcoMineralOffset: return ResolveDomain_t::Base;

        case StatId_t::PlanetFaunaCargoScale:
        case StatId_t::TechCost:
        case StatId_t::TechCostDiff:
        case StatId_t::TechCostCatchUp:
        case StatId_t::TechCostTurnLag:
        case StatId_t::TechCostResearch:
        case StatId_t::UpheavalCost:
        case StatId_t::UpheavalCostScale:
        case StatId_t::CommerceRate:
        case StatId_t::ProbeActorCost:
        case StatId_t::CouncilVotes:
        case StatId_t::EcoCleanMinerals:
        case StatId_t::RebelJoinWeight:
        case StatId_t::GiftGivenValue:
        case StatId_t::GiftReceivedValue:
        case StatId_t::GiftSizeThreshold: return ResolveDomain_t::Faction;

        case StatId_t::Attack:
        case StatId_t::Defense:
        case StatId_t::Movement:
        case StatId_t::Vision:
        case StatId_t::HitPoints:
        case StatId_t::PsiDamage:
        case StatId_t::CollateralDamage:
        case StatId_t::EarthquakeLevels:
        case StatId_t::FungalBloomTiles:
        case StatId_t::ExplosionRadius:
        case StatId_t::CollateralSusceptibility:
        case StatId_t::PlanetPearls:
        case StatId_t::DisengageChance:
        case StatId_t::BoardChance:
        case StatId_t::BoardHpLimit:
        case StatId_t::TurnsOfFuel:
        case StatId_t::DamageFromOutOfFuel:
        case StatId_t::AirdropRange:
        case StatId_t::AirdropLandingDamage:
        case StatId_t::BombardRange:
        case StatId_t::DestroyEnhancementSuccess:
        case StatId_t::DestroyEnhancementCount:
        case StatId_t::CargoCapacity:
        case StatId_t::DifficultTerrainCost:
        case StatId_t::MineralUpkeep:
        case StatId_t::PoliceEffectiveness:
        case StatId_t::AwayFromHomeDrones:
        case StatId_t::ProbeFailureScale:
        case StatId_t::ProbeIntrusion:
        case StatId_t::ProbeFailureSave:
        case StatId_t::GarrisonProbeWeight:
        case StatId_t::GarrisonProbeMultiplier:
        case StatId_t::MoraleRank:
        case StatId_t::MoraleExtra:
        case StatId_t::PromotionChance: return ResolveDomain_t::Unit;

        case StatId_t::MoistureTier:
        case StatId_t::TileDefense:
        case StatId_t::MoveCost:
        case StatId_t::EcoDamageContribution:
        case StatId_t::EcoDamageWorkedContribution:
        case StatId_t::BombardMinHpPercent: return ResolveDomain_t::Tile;
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
    if (rStat == "collateral_damage")       return StatId_t::CollateralDamage;
    if (rStat == "earthquake_levels")       return StatId_t::EarthquakeLevels;
    if (rStat == "fungal_bloom_tiles")      return StatId_t::FungalBloomTiles;
    if (rStat == "explosion_radius")        return StatId_t::ExplosionRadius;
    if (rStat == "collateral_susceptibility") return StatId_t::CollateralSusceptibility;
    if (rStat == "planet_pearls")           return StatId_t::PlanetPearls;
    if (rStat == "disengage_chance")        return StatId_t::DisengageChance;
    if (rStat == "board_chance")            return StatId_t::BoardChance;
    if (rStat == "board_hp_limit")          return StatId_t::BoardHpLimit;
    if (rStat == "turns_of_fuel")           return StatId_t::TurnsOfFuel;
    if (rStat == "damage_from_out_of_fuel") return StatId_t::DamageFromOutOfFuel;
    if (rStat == "airdrop_range")           return StatId_t::AirdropRange;
    if (rStat == "airdrop_landing_damage")  return StatId_t::AirdropLandingDamage;
    if (rStat == "bombard_range")           return StatId_t::BombardRange;
    if (rStat == "destroy_enhancement_success") return StatId_t::DestroyEnhancementSuccess;
    if (rStat == "destroy_enhancement_count")   return StatId_t::DestroyEnhancementCount;
    if (rStat == "cargo_capacity")          return StatId_t::CargoCapacity;
    if (rStat == "difficult_terrain_cost")  return StatId_t::DifficultTerrainCost;
    if (rStat == "move_cost")               return StatId_t::MoveCost;
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
    if (rStat == "probe_actor_cost")        return StatId_t::ProbeActorCost;
    if (rStat == "probe_defense")           return StatId_t::ProbeDefense;
    if (rStat == "probe_failure_scale")     return StatId_t::ProbeFailureScale;
    if (rStat == "probe_success_scale")     return StatId_t::ProbeSuccessScale;
    if (rStat == "probe_hunter")            return StatId_t::ProbeHunter;
    if (rStat == "probe_intrusion")         return StatId_t::ProbeIntrusion;
    if (rStat == "probe_success_bust")      return StatId_t::ProbeSuccessBust;
    if (rStat == "probe_failure_save")      return StatId_t::ProbeFailureSave;
    if (rStat == "probe_distance_scale")    return StatId_t::ProbeDistanceScale;
    if (rStat == "garrison_probe_weight")   return StatId_t::GarrisonProbeWeight;
    if (rStat == "garrison_probe_multiplier") return StatId_t::GarrisonProbeMultiplier;
    if (rStat == "plague_damage_scale")     return StatId_t::PlagueDamageScale;
    if (rStat == "starting_minerals")       return StatId_t::StartingMinerals;
    if (rStat == "morale_rank")             return StatId_t::MoraleRank;
    if (rStat == "morale_extra")            return StatId_t::MoraleExtra;
    if (rStat == "promotion_chance")        return StatId_t::PromotionChance;
    if (rStat == "growth_rate")             return StatId_t::GrowthRate;
    if (rStat == "starting_size")           return StatId_t::StartingSize;
    if (rStat == "max_base_size")           return StatId_t::MaxBaseSize;
    if (rStat == "tech_cost")               return StatId_t::TechCost;
    if (rStat == "tech_cost_diff")          return StatId_t::TechCostDiff;
    if (rStat == "tech_cost_catch_up")      return StatId_t::TechCostCatchUp;
    if (rStat == "tech_cost_turn_lag")      return StatId_t::TechCostTurnLag;
    if (rStat == "tech_cost_research")      return StatId_t::TechCostResearch;
    if (rStat == "upheaval_cost")           return StatId_t::UpheavalCost;
    if (rStat == "upheaval_cost_scale")     return StatId_t::UpheavalCostScale;
    if (rStat == "moisture_tier")           return StatId_t::MoistureTier;
    if (rStat == "commerce_rate")           return StatId_t::CommerceRate;
    if (rStat == "commerce_rating")         return StatId_t::CommerceRating;
    if (rStat == "bureaucracy")             return StatId_t::Bureaucracy;
    if (rStat == "council_votes")           return StatId_t::CouncilVotes;
    if (rStat == "commerce_energy_bonus")   return StatId_t::CommerceEnergyBonus;
    if (rStat == "inefficiency_denominator") return StatId_t::InefficiencyDenominator;
    if (rStat == "allocation_imbalance_factor") return StatId_t::AllocationImbalanceFactor;
    if (rStat == "scrap_refund")             return StatId_t::ScrapRefund;
    if (rStat == "last_defender_pop_loss")   return StatId_t::LastDefenderPopLoss;
    if (rStat == "capture_pop_loss")         return StatId_t::CapturePopLoss;
    if (rStat == "conquered_drone_cap")      return StatId_t::ConqueredDroneCap;
    if (rStat == "capture_facilities_destroyed_min")
        return StatId_t::CaptureFacilitiesDestroyedMin;
    if (rStat == "capture_facilities_destroyed_max_percent")
        return StatId_t::CaptureFacilitiesDestroyedMaxPercent;
    if (rStat == "bombard_min_hp_percent")   return StatId_t::BombardMinHpPercent;
    if (rStat == "ecological_damage")        return StatId_t::EcologicalDamage;
    if (rStat == "planet_fauna_cargo_scale")  return StatId_t::PlanetFaunaCargoScale;
    if (rStat == "eco_damage_contribution")  return StatId_t::EcoDamageContribution;
    if (rStat == "eco_damage_worked_contribution")
        return StatId_t::EcoDamageWorkedContribution;
    if (rStat == "eco_terraform_scale")      return StatId_t::EcoTerraformScale;
    if (rStat == "eco_clean_minerals")       return StatId_t::EcoCleanMinerals;
    if (rStat == "eco_damage_reduction")     return StatId_t::EcoDamageReduction;
    if (rStat == "eco_mineral_offset")       return StatId_t::EcoMineralOffset;
    if (rStat == "rebel_join_weight")         return StatId_t::RebelJoinWeight;
    if (rStat == "gift_given_value")          return StatId_t::GiftGivenValue;
    if (rStat == "gift_received_value")       return StatId_t::GiftReceivedValue;
    if (rStat == "gift_size_threshold")       return StatId_t::GiftSizeThreshold;
    throw std::runtime_error("Unknown stat id: '" + rStat + "'");
}

using SocialPolicyId_t = std::string;

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
    // A use-action spends the unit; true if any component carries it.
    SingleUse,
    IgnoreDifficultTerrain,
    // Any combat involving a unit with this flag uses psi strengths and damage.
    ForcesPsiCombat,
    // This blueprint is native life. Train bonuses (Centauri Preserve, Command Center)
    // read it through IsNativeLife. A composed design carries it on a component.
    NativeLife,

    // Non-combat special equipment (weapon-slot) capability gates.
    FoundBase,
    Terraform,
    SupplyCrawl,
    ProbeTeam,
    // Not a combatant for the non-combatant stack census. Probe Team declares it.
    NonCombatant,
    // Unit may attempt an airdrop when it began the turn on an airdrop_launch pad.
    Airdrop,
    // Long-range fire inside bombard_range. No separate melee attack.
    Bombard,

    // Sole capture veto: chassis (Needlejet / Missile) or noncombat weapon modules.
    CannotCaptureBases,
    // Attack spends all remaining moves (Needlejet: one strike per turn).
    AttackingEndsTurn,
    // Capturing this unit does not fully repair it (e.g. Battle Ogre).
    NoConquestRepair,
    // A combat kill of this unit may capture it instead (Alien Artifact), and earns no promotion.
    CapturedOnDefeat,
    // Attacking this unit is not a hostile act. Resolved with its tile, so conditions can
    // restrict it (an artifact off a base).
    AttackNotHostile,
    // Owner is unknown to other factions (IsOwnerKnownTo): its hostile acts and those against
    // it are unattributed, it is not met on sight, and territory bans do not hold it (Probe Team).
    Covert,

    // Unit / tile flags
    // Blocks the opponent from disengaging (ThisUnit, Comm Jammer), or a unit on this tile
    // (ThisTile: Base, Bunker, Airbase).
    PreventsDisengage,

    // Tile declares it harbors a domain (ThisTile). Queried via TileHarbors with territory
    // ownership.
    Harbors,
    // Tile may serve as an airdrop launch / safe-landing pad (Base, Airbase). Queried via
    // TileProvidesFlag — distinct from Harbors so carrier decks do not qualify.
    AirdropLaunch,
    // Hostile ThisTile aura that blocks enemy airdrops onto the tiles it covers.
    AirdropInterdiction,

    // Faction/global flags
    PopulationBoom,
    NearZeroGrowth,
    // Base is the faction headquarters (assassinate / MC eligibility).
    Headquarters,
    // SE Probe +3 / Thought Control: bases and units cannot be subverted by standard probes.
    ProbeSubversionImmune,
    // Targeted sabotage of this facility is at least risk 2 (Perimeter Defense, Tachyon Field).
    HighRiskSabotage,
    // Tech that unlocks genetic plague (TFLAG_ALLOW_GENE_WARFARE).
    AllowGeneWarfare,
    // Tech that improves gene-warfare defense (TFLAG_INC_GENE_WARFARE_DEFENSE).
    IncGeneWarfareDefense,
    // Faction may airdrop anywhere on the map (ignores AirdropRange). Graviton Theory /
    // Space Elevator.
    OrbitalInsertion,

    // Map visibility. RemoveShroud permanently explores the map (satellite); RemoveFog
    // clears current fog while active (secret project). See VisibilityMutations helpers.
    RemoveShroud,
    RemoveFog,
    // This unit stays visible on tiles the observer has explored, including while those
    // tiles are not currently lit. Shroud still hides it. Concealment still applies.
    VisibleInFog,

    // U.N. Charter: atrocities are illegal while this flag is in force planet-wide.
    AtrocitiesForbidden,
    // While present, a stack left with only non-combatants loses those occupants after
    // collateral. Standing world rule; the non_combatant tag stays either way.
    NonCombatantsDestroyedWithoutCombatant,

    // The base cannot bank minerals, complete construction or hurry; stockpiles keep progress.
    DisableProduction,

    // Voice of Planet (and peers): wild Planet fauna unit spawning is off for the rest of the game.
    SuppressPlanetFauna,
};

// Snake_case JSON wire form differs from enumerator names — one explicit map next to the enum.
inline RuleFlagId_t ParseRuleFlagId(const std::string& rFlag)
{
    if (rFlag == "flight")                      return RuleFlagId_t::Flight;
    if (rFlag == "population_boom")             return RuleFlagId_t::PopulationBoom;
    if (rFlag == "near_zero_growth")            return RuleFlagId_t::NearZeroGrowth;
    if (rFlag == "remove_shroud")               return RuleFlagId_t::RemoveShroud;
    if (rFlag == "remove_fog")                  return RuleFlagId_t::RemoveFog;
    if (rFlag == "visible_in_fog")              return RuleFlagId_t::VisibleInFog;
    if (rFlag == "single_use")                  return RuleFlagId_t::SingleUse;
    if (rFlag == "ignores_difficult_terrain")   return RuleFlagId_t::IgnoreDifficultTerrain;
    if (rFlag == "forces_psi_combat")            return RuleFlagId_t::ForcesPsiCombat;
    if (rFlag == "native_life")                  return RuleFlagId_t::NativeLife;
    if (rFlag == "found_base")                   return RuleFlagId_t::FoundBase;
    if (rFlag == "terraform")                   return RuleFlagId_t::Terraform;
    if (rFlag == "supply_crawl")                return RuleFlagId_t::SupplyCrawl;
    if (rFlag == "probe_team")                  return RuleFlagId_t::ProbeTeam;
    if (rFlag == "non_combatant")               return RuleFlagId_t::NonCombatant;
    if (rFlag == "airdrop")                     return RuleFlagId_t::Airdrop;
    if (rFlag == "bombard")                     return RuleFlagId_t::Bombard;
    if (rFlag == "cannot_capture_bases")        return RuleFlagId_t::CannotCaptureBases;
    if (rFlag == "attacking_ends_turn")         return RuleFlagId_t::AttackingEndsTurn;
    if (rFlag == "no_conquest_repair")          return RuleFlagId_t::NoConquestRepair;
    if (rFlag == "covert")                      return RuleFlagId_t::Covert;
    if (rFlag == "prevents_disengage")          return RuleFlagId_t::PreventsDisengage;
    if (rFlag == "captured_on_defeat")          return RuleFlagId_t::CapturedOnDefeat;
    if (rFlag == "attack_not_hostile")          return RuleFlagId_t::AttackNotHostile;
    if (rFlag == "harbors")                     return RuleFlagId_t::Harbors;
    if (rFlag == "airdrop_launch")              return RuleFlagId_t::AirdropLaunch;
    if (rFlag == "airdrop_interdiction")        return RuleFlagId_t::AirdropInterdiction;
    if (rFlag == "headquarters")                return RuleFlagId_t::Headquarters;
    if (rFlag == "probe_subversion_immune")     return RuleFlagId_t::ProbeSubversionImmune;
    if (rFlag == "high_risk_sabotage")          return RuleFlagId_t::HighRiskSabotage;
    if (rFlag == "allow_gene_warfare")          return RuleFlagId_t::AllowGeneWarfare;
    if (rFlag == "inc_gene_warfare_defense")    return RuleFlagId_t::IncGeneWarfareDefense;
    if (rFlag == "orbital_insertion")           return RuleFlagId_t::OrbitalInsertion;
    if (rFlag == "atrocities_forbidden")        return RuleFlagId_t::AtrocitiesForbidden;
    if (rFlag == "non_combatants_destroyed_without_combatant")
        return RuleFlagId_t::NonCombatantsDestroyedWithoutCombatant;
    if (rFlag == "disable_production")          return RuleFlagId_t::DisableProduction;
    if (rFlag == "suppress_planet_fauna")       return RuleFlagId_t::SuppressPlanetFauna;
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
    // Only the tile the effect belongs to; resolved locally and never in the base-wide pool.
    ThisTile,
    // Stamped onto units built at the originating base, permanently (Command Center, Aerospace).
    ProducedAtThisBase,
    // Only the tech it is declared on, while that tech is the research target.
    ThisTech,
    // Only the faction's dealings with one partner; resolves where the context names that
    // partner.
    FactionPair,
};

// Where an effect resolves. The one source of scope routing: collectors and filters derive
// from LaneFor, whose switch is exhaustive.
enum class EffectLane_t
{
    // In the faction pool tagged with originBase; FilterForBase includes it per base.
    Base,
    // At every base of the faction and in faction resolves. FactionGlobal and WorldGlobal
    // also reach live units and unit move cost (AppendGlobalScopeEffects); WorldGlobal also
    // crosses factions and reaches session tile resolvers.
    FactionWide,
    // Merged into every live unit's stat resolution. Lives in the faction pool; consumed by
    // Unit::Get* via FilterByScope(FactionUnits), never applies at base level.
    FactionUnits,
    // Stamped onto units whose production base matches originBase; never at base level.
    ProducedAtBase,
    // Resolved by the unit's own design (intrinsic component stats). Never enters the pool.
    UnitLocal,
    // Resolved by the pop itself (tile multipliers). Never enters the pool.
    PopLocal,
    // Resolved by the tile resolvers (CollectTileEffects/CollectAreaEffects). Never enters
    // the pool.
    TileLocal,
    // Resolved when costing the tech that declares the effect. Never enters the pool.
    TechLocal,
    // Lives in the faction pool tagged with partnerFaction. Resolved only in a context naming
    // that partner; never applies at base level, to units, or in a context-free resolve.
    FactionPair,
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
        case EffectScope_t::ThisTech:      return EffectLane_t::TechLocal;
        case EffectScope_t::FactionPair:   return EffectLane_t::FactionPair;
    }
    return EffectLane_t::FactionWide; // unreachable; all enumerators handled above
}

// Scopes resolved faction-wide through the pool, not by one base, pop, unit or tile.
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
    // Clamps apply after the arithmetic; the tightest of each kind wins, and MinClamp beats a
    // crossing MaxClamp. Clamps the value to <= amount.
    MaxClamp,
    // Clamp the resolved stat value to be >= amount.
    MinClamp,
};

// Live integer vs a bound. Numeric conditions share this (`SocialRating_t` today).
enum class CompareOp_t
{
    Eq,
    Le,
    Ge,
};

inline bool CompareSatisfied(int live, CompareOp_t op, int bound)
{
    switch (op)
    {
        case CompareOp_t::Eq: return live == bound;
        case CompareOp_t::Le: return live <= bound;
        case CompareOp_t::Ge: return live >= bound;
    }
    return false;
}

// World-state change id (sea level / climate) for the triggered WorldParameterEffect_t.
enum class WorldParameterId_t
{
    SeaLevel,
};

// Snake_case JSON wire form differs from enumerator names — one explicit map next to the enum.
inline WorldParameterId_t ParseWorldParameterId(const std::string& rParameter)
{
    if (rParameter == "sea_level") return WorldParameterId_t::SeaLevel;
    throw std::runtime_error("Unknown world parameter: '" + rParameter + "'");
}

// Narrows which other factions a cross-faction effect targets; scope stays the lane.
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

// The kind of config that declared an effect, so a scope only one kind can resolve fails on
// any other at load.
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
    // Flat native-life designs (config/native_units.json): ThisUnit continuous effects only.
    NativeUnit,
    // pop_composition.json's faction-wide `effects` array: enters the pool with no origin base.
    PopComposition,
    // pop_composition.json's per-base mood arrays (riot_tiers, golden_age_effects): collected
    // against a specific base, so ThisBase is legal here and rejected for PopComposition.
    PopCompositionBaseLocal,
    // pop_growth.json continuous baselines (starting_size / max_base_size). No origin base —
    // AllOwnerBases only, like Difficulty.
    Growth,
    // config/world_rules.json: standing WorldGlobal effects appended once per session
    // (collateral, bombard floor, tech stagnation cost and turn lag).
    WorldRules,
    // config/eco_damage.json's continuous `effects` (the clean-minerals baseline). No origin
    // base — FactionGlobal / AllOwnerBases only, like Difficulty.
    EcoDamage,
    // config/native_life.json: the session abundance level's continuous effects.
    NativeLifeLevel,
    // config/world_events.json: an event's `effects`, served to every faction while it is
    // active.
    WorldEvent,
    // config/diplomacy.json: a status's `effects`, applied to each side of a pair holding it.
    // FactionPair scope only.
    DiplomaticStatus,
};

} // namespace ac
