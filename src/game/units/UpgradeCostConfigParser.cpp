#include "game/units/UpgradeCostConfigParser.h"

#include "lib/config/LuaConfigLoader.h"

namespace ac
{

UpgradeCostConfig_t UpgradeCostConfigParser::ParseConfig(const std::string& rScriptPath,
                                                         LuaRuntime& rLua)
{
    const sol::table table = LuaConfigLoader::RunScript(rLua, rScriptPath, "unit upgrade");
    UpgradeCostConfig_t config;
    config.energyCostFormula = LuaConfigLoader::RequireFormula(
        rLua, table, "energy_cost_formula",
        {"new_base_cost", "old_weapon_mineral_cost", "new_weapon_mineral_cost",
         "old_armor_mineral_cost", "new_armor_mineral_cost", "target_unprototyped"},
        rScriptPath, "unit upgrade");
    return config;
}

} // namespace ac
