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
    CostMultiplier
    // TODO: add more stats as they are defined
};

enum class RuleFlagId
{
    // Unit flags
    Flight,
    SingleUse,

    // Faction/global flags
    PopulationBoom
    // TODO: add more flags as they are defined
};

} // namespace ac
