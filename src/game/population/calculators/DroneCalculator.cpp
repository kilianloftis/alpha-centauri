#include "game/population/calculators/DroneCalculator.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

uint64_t StableBaseHash(int baseId)
{
    uint64_t x = static_cast<uint64_t>(static_cast<uint32_t>(baseId));
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

DroneCalculator::DroneCalculator(const PopCompositionConfig_t& rConfig, LuaRuntime& rLua)
    : m_rConfig(rConfig)
    , m_rLua(rLua)
{
}

int DroneCalculator::CalculateLimit(const DroneInputs_t& rInputs) const
{
    const std::unordered_map<std::string, int> vars = {
        {"difficulty",  rInputs.difficulty},
        {"efficiency",  rInputs.efficiency},
        {"map_width",   rInputs.mapWidth},
        {"map_height",  rInputs.mapHeight},
        {"bureaucracy_multiplier", rInputs.bureaucracyMultiplierPercent},
    };

    const int limit = m_rLua.EvalInt(m_rConfig.bureaucracyLimitFormula, vars);
    if (limit <= 0)
    {
        throw std::runtime_error("Bureaucracy limit formula ('"
                                 + m_rConfig.bureaucracyLimitFormula + "') produced "
                                 + std::to_string(limit) + "; limit must be positive");
    }
    return limit;
}

int DroneCalculator::Calculate(const DroneInputs_t& rInputs) const
{
    const int limit = CalculateLimit(rInputs);
    const int residue =
        static_cast<int>(StableBaseHash(rInputs.baseId) % static_cast<uint64_t>(limit));

    const std::unordered_map<std::string, int> vars = {
        {"difficulty",              rInputs.difficulty},
        {"efficiency",              rInputs.efficiency},
        {"map_width",               rInputs.mapWidth},
        {"map_height",              rInputs.mapHeight},
        {"base_id",                 rInputs.baseId},
        {"base_size",               rInputs.baseSize},
        {"faction_base_count",      rInputs.factionBaseCount},
        {"garrison_count",          rInputs.garrisonCount},
        {"social_drone_modifier",   rInputs.socialDroneModifier},
        {"turns_since_conquered",   rInputs.turnsSinceConquered},
        {"bureaucracy_limit",       limit},
        {"bureaucracy_multiplier",  rInputs.bureaucracyMultiplierPercent},
        {"residue",                 residue},
    };

    const int value = m_rLua.EvalInt(m_rConfig.droneFormula, vars);
    if (value < 0)
    {
        throw std::runtime_error("Drone formula ('" + m_rConfig.droneFormula + "') produced "
                                 + std::to_string(value) + "; target must not be negative");
    }
    return value;
}

} // namespace ac
