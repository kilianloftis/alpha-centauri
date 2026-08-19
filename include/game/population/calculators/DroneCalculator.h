#pragma once

#include "game/population/pop-types/PopCompositionConfigParser.h"

#include <cstdint>

namespace ac
{

class LuaRuntime;

struct DroneInputs_t
{
    int difficulty = 0;
    int efficiency = 0;
    int mapWidth = 0;
    int mapHeight = 0;
    int baseId = 0;
    int baseSize = 0;
    int factionBaseCount = 0;
    int garrisonCount = 0;
    int socialDroneModifier = 0;
    int turnsSinceConquered = 0;
    // Percent representation of the bureaucracy multiplier (100 == 1.0)
    int bureaucracyMultiplierPercent = 100;
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
