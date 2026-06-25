#include "game/research/TechCostCalculator.h"
#include "lib/LuaRuntime.h"
#include <unordered_map>

namespace ac
{

TechCostCalculator::TechCostCalculator(const TechCostConfig& rConfig, LuaRuntime& rLua)
    : m_pConfig(&rConfig)
    , m_pLua(&rLua)
{
}

TechCostCalculator::~TechCostCalculator()
{
}

int TechCostCalculator::CalculateCost(const TechConfig_t& rTech, const TechCostInputs_t& rInputs) const
{
    std::unordered_map<std::string, int> vars = {
        {"techs",               rInputs.techs},
        {"most_techs",          rInputs.mostTechs},
        {"diff",                rInputs.diff},
        {"turns",               rInputs.turns},
        {"is_ai",               rInputs.bIsAI ? 1 : 0},
        {"tech_stagnation",     rInputs.bTechStagnation ? 1 : 0},
        {"research_modifier",   rInputs.researchModifier},
        {"world_size_modifier", rInputs.worldSizeModifier},
        {"faction_modifier",    rInputs.factionTechCostModifier},
        {"alphax_modifier",     rInputs.alphaxTechCostModifier},
        {"base_cost",           rTech.cost},
    };

    int cost = m_pLua->EvalInt(m_pConfig->costFormula, vars);

    return std::max(1, cost);
}

} // namespace ac
