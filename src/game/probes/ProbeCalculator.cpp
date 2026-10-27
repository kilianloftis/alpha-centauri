#include "game/probes/ProbeCalculator.h"

#include "game/probes/ProbeActionConfig.h"
#include "lib/LuaRuntime.h"

#include <string>
#include <unordered_map>

namespace ac
{

ProbeCalculator::ProbeCalculator(LuaRuntime& rLua)
    : m_rLua(rLua)
{}

int ProbeCalculator::MindControlCost(const ProbeCostConfig_t& rCost,
                                     const MindControlCostInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"garrison_weight", static_cast<double>(rInputs.garrisonWeight)},
        {"garrison_multiplier", static_cast<double>(rInputs.garrisonMultiplier)},
        {"mind_control_total", static_cast<double>(rInputs.mindControlTotal)},
        {"population", static_cast<double>(rInputs.population)},
        {"energy", static_cast<double>(rInputs.energy)},
        {"distance", static_cast<double>(rInputs.distance)},
    };
    return m_rLua.EvalInt(rCost.formula, vars);
}

int ProbeCalculator::SubvertCost(const ProbeCostConfig_t& rCost,
                                 const SubvertCostInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"mineral_cost", static_cast<double>(rInputs.mineralCost)},
        {"energy", static_cast<double>(rInputs.energy)},
        {"distance", static_cast<double>(rInputs.distance)},
    };
    return m_rLua.EvalInt(rCost.formula, vars);
}

} // namespace ac
