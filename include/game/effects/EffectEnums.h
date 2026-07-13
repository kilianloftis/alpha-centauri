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
    DisengageChance,
    Fuel,
    DamageFromOutOfFuel,
    CargoCapacity,
    DifficultTerrainCost,
    CostMultiplier,

    // Population growth rate modifier (AddPercent, base = 100%)
    GrowthRate,

    // Research tech cost percentage modifier (Add, base = 0; negative = cheaper)
    TechCost,

    // Tile terrain mutation (resolved back into Tile::SetMoisture, not a runtime-queried stat)
    MoistureTier
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
        case StatId_t::DisengageChance:
        case StatId_t::Fuel:
        case StatId_t::DamageFromOutOfFuel:
        case StatId_t::CargoCapacity:
        case StatId_t::DifficultTerrainCost:
        case StatId_t::TechCost:             return StatKind_t::Additive;
        case StatId_t::CostMultiplier:       return StatKind_t::PureMultiplier;
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
    SingleUse,
    IgnoreZoneOfControl,
    IgnoreDifficultTerrain,

    // Faction/global flags
    PopulationBoom,
    NearZeroGrowth
    // TODO: add more flags as they are defined
};

} // namespace ac
