#include "game/research/TechCostConfig.h"
#include "lib/config/LuaConfigLoader.h"

namespace ac
{

TechCostConfig_t TechCostConfigParser::ParseConfig(const std::string& scriptPath, LuaRuntime& rLua)
{
    const sol::table table = LuaConfigLoader::RunScript(rLua, scriptPath, "tech cost");
    TechCostConfig_t config;
    config.costFormula =
        LuaConfigLoader::RequireFormula(table, "cost_formula", scriptPath, "tech cost");
    return config;
}

} // namespace ac
