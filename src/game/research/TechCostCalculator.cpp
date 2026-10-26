#include "game/research/TechCostCalculator.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

TechCostCalculator::TechCostCalculator(const TechCostConfig_t& rConfig, LuaRuntime& rLua)
    : m_pConfig(&rConfig)
    , m_pLua(&rLua)
{
}

int TechCostCalculator::CalculateCost(const TechCostInputs_t& rInputs) const
{
    std::unordered_map<std::string, double> vars = {
        {"techs",               static_cast<double>(rInputs.techs)},
        {"most_techs",          static_cast<double>(rInputs.mostTechs)},
        {"diff",                static_cast<double>(rInputs.diff)},
        {"catch_up_diff",       static_cast<double>(rInputs.catchUpDiff)},
        {"turns",               static_cast<double>(rInputs.turns)},
        {"is_ai",               rInputs.bIsAI ? 1.0 : 0.0},
        {"turn_lag",            rInputs.bTurnLag ? 1.0 : 0.0},
        {"research",            static_cast<double>(rInputs.research)},
        {"map_area_sq_root",    static_cast<double>(rInputs.mapAreaSqRoot)},
    };

    const int cost = m_pLua->EvalInt(m_pConfig->costFormula, vars);
    if (cost <= 0)
    {
        throw std::runtime_error("Tech cost formula produced " + std::to_string(cost)
                                 + "; costs must be positive");
    }
    return cost;
}

} // namespace ac
