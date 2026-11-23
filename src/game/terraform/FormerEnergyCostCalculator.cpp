#include "game/terraform/FormerEnergyCostCalculator.h"

#include "lib/LuaRuntime.h"

#include <unordered_map>

namespace ac
{

FormerEnergyCostCalculator::FormerEnergyCostCalculator(LuaRuntime& rLua) : m_rLua(rLua) {}

int FormerEnergyCostCalculator::Cost(const FormerEnergyCost_t& rCost,
                                     const FormerEnergyCostInputs_t& rInputs) const
{
    if (!rCost.formula.has_value())
    {
        return rCost.flat;
    }
    const std::unordered_map<std::string, double> vars = {
        {"elev_band", static_cast<double>(rInputs.elevBand)},
        {"fungus", rInputs.bFungus ? 1.0 : 0.0},
        {"at_sea", rInputs.bAtSea ? 1.0 : 0.0},
        {"sea_cost_scale", rInputs.seaCostScale},
        {"nearest_base_distance", static_cast<double>(rInputs.nearestBaseDistance)},
        {"own_pop", static_cast<double>(rInputs.ownPop)},
        {"enemy_pop", static_cast<double>(rInputs.enemyPop)},
        {"own_range", static_cast<double>(rInputs.ownRange)},
        {"enemy_range", static_cast<double>(rInputs.enemyRange)},
    };
    return m_rLua.EvalInt(*rCost.formula, vars);
}

} // namespace ac
