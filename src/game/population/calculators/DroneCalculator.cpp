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
    const std::unordered_map<std::string, double> vars = {
        {"bureaucracy", static_cast<double>(rInputs.bureaucracy)},
        {"map_width",   static_cast<double>(rInputs.mapWidth)},
        {"map_height",  static_cast<double>(rInputs.mapHeight)},
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

    const std::unordered_map<std::string, double> vars = {
        {"bureaucracy",             rInputs.bureaucracy},
        {"map_width",               static_cast<double>(rInputs.mapWidth)},
        {"map_height",              static_cast<double>(rInputs.mapHeight)},
        {"base_id",                 static_cast<double>(rInputs.baseId)},
        {"base_size",               static_cast<double>(rInputs.baseSize)},
        {"faction_base_count",      static_cast<double>(rInputs.factionBaseCount)},
        {"garrison_count",          static_cast<double>(rInputs.garrisonCount)},
        {"resolved_drones",         static_cast<double>(rInputs.resolvedDrones)},
        {"turns_since_conquered",   static_cast<double>(rInputs.turnsSinceConquered)},
        {"size_free_drones",        static_cast<double>(rInputs.sizeFreeDrones)},
        {"bureaucracy_limit",       static_cast<double>(limit)},
        {"residue",                 static_cast<double>(residue)},
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
