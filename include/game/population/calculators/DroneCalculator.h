#pragma once

#include "game/population/pop-types/PopCompositionConfigParser.h"

#include <cstdint>

namespace ac
{

class LuaRuntime;

struct DroneInputs_t
{
    // Resolved PureMultiplier Bureaucracy (Citizen + Efficiency 0 → 32). May be fractional.
    double bureaucracy = 1.0;
    // Resolved SizeFreeDrones: pops at or below this are free of size drones.
    int sizeFreeDrones = 0;
    int mapWidth = 0;
    int mapHeight = 0;
    int baseId = 0;
    int baseSize = 0;
    int factionBaseCount = 0;
    int garrisonCount = 0;
    // Effect-only Drones contribution (SeedFor=0): BaseSize×amount Adds, flat Adds, etc.
    // Size drones are max(0, base_size - size_free_drones) in drone_formula, not this field.
    int resolvedDrones = 0;
    int turnsSinceConquered = 0;
    // 0 duration / peak means the base is not assimilating (never captured, or the window ended).
    int assimilationDuration = 0;
    int assimilationPeak = 0;
    // Resolved ConqueredDroneCap (difficulty 0.25×level + base_conquest −0.5). Kept as a
    // double so the 0.25 steps survive until Lua takes floor(base_size/4 + this).
    double conqueredDroneCap = 0.0;
};

uint64_t StableBaseHash(int baseId);

class DroneCalculator
{
public:
    DroneCalculator(const PopCompositionConfig_t& rConfig, LuaRuntime& rLua);
    ~DroneCalculator() = default;

    int CalculateLimit(const DroneInputs_t& rInputs) const;
    int Calculate(const DroneInputs_t& rInputs) const;

private:
    const PopCompositionConfig_t& m_rConfig;
    LuaRuntime& m_rLua;
};

} // namespace ac
