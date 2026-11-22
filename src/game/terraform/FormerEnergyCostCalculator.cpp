#include "game/terraform/FormerEnergyCostCalculator.h"

#include "lib/LuaRuntime.h"

#include <unordered_map>

namespace ac
{

FormerEnergyCostCalculator::FormerEnergyCostCalculator(LuaRuntime& rLua) : m_rLua(rLua) {}

int FormerEnergyCostCalculator::Cost(const FormerEnergyCost_t& rCost, int elevBand,
                                     int nearestBaseDistance) const
{
    if (!rCost.formula.has_value())
    {
        return rCost.flat;
    }
    const std::unordered_map<std::string, double> vars = {
        {"elev_band", static_cast<double>(elevBand)},
        {"nearest_base_distance", static_cast<double>(nearestBaseDistance)},
    };
    return m_rLua.EvalInt(*rCost.formula, vars);
}

} // namespace ac
