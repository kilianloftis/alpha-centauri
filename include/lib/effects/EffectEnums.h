#pragma once

namespace ac
{

enum class StatId
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
    HitPoints,
    DisengageChance,
    Fuel,
    DamageFromOutOfFuel,
    CargoCapacity,
    DifficultTerrainCost,
    CostMultiplier,

    // Population growth rate modifier (AddPercent, base = 100%)
    GrowthRate,

    // Tile terrain mutation (resolved back into Tile::SetMoisture, not a runtime-queried stat)
    MoistureTier
    // TODO: add more stats as they are defined
};

enum class SocialRatingId
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

enum class RuleFlagId
{
    // Unit flags
    Flight,
    SingleUse,

    // Faction/global flags
    PopulationBoom,
    NearZeroGrowth
    // TODO: add more flags as they are defined
};

} // namespace ac
