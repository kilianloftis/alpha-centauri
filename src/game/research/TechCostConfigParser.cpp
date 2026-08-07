#include "game/research/TechCostConfig.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>

namespace ac
{

TechCostConfig_t TechCostConfigParser::ParseConfig(const std::string& scriptPath, LuaRuntime& rLua)
{
    TechCostConfig_t config;

    sol::state& lua = rLua.GetState();

    sol::protected_function_result result =
        lua.safe_script_file(scriptPath, sol::script_pass_on_error);

    if (!result.valid())
    {
        const sol::error err = result;
        throw std::runtime_error("Failed to load tech cost script '" + scriptPath + "': "
                                 + err.what());
    }

    sol::table tbl = result;

    config.costFormula = tbl.get_or("cost_formula", std::string(""));
    if (config.costFormula.empty())
    {
        throw std::runtime_error("tech cost script '" + scriptPath
                                 + "' must set cost_formula to a non-empty Lua expression");
    }

    return config;
}

} // namespace ac
