#include "game/social-engineering/UpheavalCostCalculator.h"

#include "game/social-engineering/UpheavalCostConfig.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

UpheavalCostCalculator::UpheavalCostCalculator(const UpheavalCostConfig_t& rConfig,
                                               LuaRuntime& rLua)
    : m_rConfig(rConfig)
    , m_rLua(rLua)
{
}

int UpheavalCostCalculator::Quote(const UpheavalCostInputs_t& rInputs) const
{
    if (rInputs.changedCategories == 0)
    {
        return 0;
    }

    const std::unordered_map<std::string, double> vars = {
        {"changed_categories", static_cast<double>(rInputs.changedCategories)},
    };
    const int cost = m_rLua.EvalInt(m_rConfig.costFormula, vars);
    if (cost < 0)
    {
        throw std::runtime_error("Upheaval cost formula produced " + std::to_string(cost)
                                 + "; cost must not be negative");
    }
    return cost;
}

} // namespace ac
