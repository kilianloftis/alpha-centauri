#include "game/research/TechCostConfig.h"
#include "lib/config/LuaConfigLoader.h"

namespace ac
{

TechCostConfig_t TechCostConfigParser::ParseConfig(const std::string& scriptPath, LuaRuntime& rLua)
{
    const sol::table table = LuaConfigLoader::RunScript(rLua, scriptPath, "tech cost");
    TechCostConfig_t config;
    config.costFormula = LuaConfigLoader::RequireFormula(
        rLua, table, "cost_formula",
        {"techs", "most_techs", "diff", "catch_up_diff", "turns", "is_ai", "turn_lag", "research",
         "map_area_sq_root"},
        scriptPath, "tech cost");
    return config;
}

} // namespace ac
