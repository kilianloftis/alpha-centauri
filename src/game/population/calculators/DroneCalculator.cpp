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

int DroneCalculator::EvalNonNegative_(const std::string& rFormula, const char* pTermName,
                                      const std::unordered_map<std::string, double>& rVars) const
{
    const int value = m_rLua.EvalInt(rFormula, rVars);
    if (value < 0)
    {
        throw std::runtime_error(std::string(pTermName) + " formula ('" + rFormula
                                 + "') produced " + std::to_string(value)
                                 + "; a drone contribution must not be negative");
    }
    return value;
}

int DroneCalculator::CalculateBureaucracyLimit(const BureaucracyDroneInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"bureaucracy", rInputs.bureaucracy},
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

int DroneCalculator::CalculateBureaucracyDrones(const BureaucracyDroneInputs_t& rInputs) const
{
    const int limit = CalculateBureaucracyLimit(rInputs);
    const int residue =
        static_cast<int>(StableBaseHash(rInputs.baseId) % static_cast<uint64_t>(limit));

    return EvalNonNegative_(m_rConfig.bureaucracyDroneFormula, "Bureaucracy drone", {
        {"bureaucracy",        rInputs.bureaucracy},
        {"map_width",          static_cast<double>(rInputs.mapWidth)},
        {"map_height",         static_cast<double>(rInputs.mapHeight)},
        {"base_id",            static_cast<double>(rInputs.baseId)},
        {"faction_base_count", static_cast<double>(rInputs.factionBaseCount)},
        {"bureaucracy_limit",  static_cast<double>(limit)},
        {"residue",            static_cast<double>(residue)},
    });
}

int DroneCalculator::CalculateSizeDrones(const SizeDroneInputs_t& rInputs) const
{
    return EvalNonNegative_(m_rConfig.sizeDroneFormula, "Size drone", {
        {"base_size",        static_cast<double>(rInputs.baseSize)},
        {"size_free_drones", static_cast<double>(rInputs.sizeFreeDrones)},
    });
}

int DroneCalculator::CalculateOccupationDrones(const OccupationDroneInputs_t& rInputs) const
{
    return EvalNonNegative_(m_rConfig.occupationDroneFormula, "Occupation drone", {
        {"base_size",             static_cast<double>(rInputs.baseSize)},
        {"turns_since_conquered", static_cast<double>(rInputs.turnsSinceConquered)},
        {"assimilation_duration", static_cast<double>(rInputs.assimilationDuration)},
        {"assimilation_peak",     static_cast<double>(rInputs.assimilationPeak)},
        {"conquered_drone_cap",   rInputs.conqueredDroneCap},
    });
}

} // namespace ac
