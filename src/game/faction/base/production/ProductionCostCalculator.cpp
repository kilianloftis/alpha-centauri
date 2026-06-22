#include "game/faction/base/production/ProductionCostCalculator.h"
#include "lib/LuaRuntime.h"
#include <iostream>
#include <unordered_map>

namespace ac
{

ProductionCostCalculator::ProductionCostCalculator(const ProductionCostConfig& rConfig, LuaRuntime& rLua)
    : m_pConfig(&rConfig)
    , m_pLua(&rLua)
{
}

int ProductionCostCalculator::ComputeCost(int baseCost, int industryRating) const
{
    const std::unordered_map<std::string, int> vars = {
        {"base_cost",       baseCost},
        {"industry_rating", industryRating},
    };
    return m_pLua->EvalInt(m_pConfig->costFormula, vars);
}

ProductionCostConfig ProductionCostConfigParser::ParseConfig(const std::string& scriptPath, LuaRuntime& rLua)
{
    ProductionCostConfig config;
    config.costFormula = "base_cost * (10 * industry_rating)";

    sol::state& lua = rLua.GetState();

    sol::protected_function_result result =
        lua.safe_script_file(scriptPath, sol::script_pass_on_error);

    if (!result.valid())
    {
        sol::error err = result;
        std::cout << "Warning: Failed to load production cost script '" << scriptPath
                  << "': " << err.what() << "\n";
        return config;
    }

    sol::table tbl = result;
    config.costFormula = tbl.get_or("cost_formula", std::string("base_cost * (10 * industry_rating)"));

    return config;
}

} // namespace ac
