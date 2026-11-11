#include "game/units/UpgradeCostConfigParser.h"

#include "lib/config/LuaConfigLoader.h"

namespace ac
{

UpgradeCostConfig_t UpgradeCostConfigParser::ParseConfig(const std::string& rScriptPath,
                                                         LuaRuntime& rLua)
{
    const sol::table table = LuaConfigLoader::RunScript(rLua, rScriptPath, "unit upgrade");
    UpgradeCostConfig_t config;
    config.energyCostFormula =
        LuaConfigLoader::RequireFormula(table, "energy_cost_formula", rScriptPath, "unit upgrade");
    return config;
}

} // namespace ac
