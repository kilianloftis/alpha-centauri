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

int TechCostCalculator::CalculateCost(const TechConfig_t& rTech, const TechCostInputs_t& rInputs) const
{
    std::unordered_map<std::string, double> vars = {
        {"techs",               static_cast<double>(rInputs.techs)},
        {"most_techs",          static_cast<double>(rInputs.mostTechs)},
        {"diff",                static_cast<double>(rInputs.diff)},
        {"turns",               static_cast<double>(rInputs.turns)},
        {"is_ai",               rInputs.bIsAI ? 1.0 : 0.0},
        {"tech_stagnation",     rInputs.bTechStagnation ? 1.0 : 0.0},
        {"research_modifier",   static_cast<double>(rInputs.researchModifier)},
        {"world_size_modifier", static_cast<double>(rInputs.worldSizeModifier)},
        {"faction_modifier",    static_cast<double>(rInputs.factionTechCostModifier)},
        {"alphax_modifier",     static_cast<double>(rInputs.alphaxTechCostModifier)},
        {"base_cost",           static_cast<double>(rTech.cost)},
    };

    const int cost = m_pLua->EvalInt(m_pConfig->costFormula, vars);
    if (cost <= 0)
    {
        throw std::runtime_error("Tech cost formula produced " + std::to_string(cost)
                                 + " for tech '" + rTech.id + "'; costs must be positive");
    }
    return cost;
}

} // namespace ac
