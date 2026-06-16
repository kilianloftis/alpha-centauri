#include "game/research/TechCostConfig.h"
#include "lib/LuaRuntime.h"
#include <iostream>

namespace ac
{

TechCostConfig TechCostConfigParser::ParseConfig(const std::string& scriptPath, LuaRuntime& rLua)
{
    TechCostConfig config;

    sol::state& lua = rLua.GetState();

    sol::protected_function_result result =
        lua.safe_script_file(scriptPath, sol::script_pass_on_error);

    if (!result.valid())
    {
        throw std::runtime_error("Failed to load tech cost script '" + scriptPath + "'");
    }

    sol::table tbl = result;

    config.costFormula = tbl.get_or("cost_formula", std::string(""));

    return config;
}

} // namespace ac
