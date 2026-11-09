#include "game/units/UpgradeCostConfigParser.h"

#include "lib/LuaRuntime.h"

#include <stdexcept>

namespace ac
{

UpgradeCostConfig_t UpgradeCostConfigParser::ParseConfig(const std::string& rScriptPath,
                                                         LuaRuntime& rLua) const
{
    sol::protected_function_result result =
        rLua.GetState().safe_script_file(rScriptPath, sol::script_pass_on_error);
    if (!result.valid())
    {
        const sol::error err = result;
        throw std::runtime_error("Failed to load unit upgrade script '" + rScriptPath + "': "
                                 + err.what());
    }

    sol::table table = result;
    UpgradeCostConfig_t config;
    config.energyCostFormula = table.get_or("energy_cost_formula", std::string(""));
    if (config.energyCostFormula.empty())
    {
        throw std::runtime_error("unit upgrade script '" + rScriptPath
                                 + "' must set energy_cost_formula to a non-empty Lua expression");
    }
    return config;
}

} // namespace ac
