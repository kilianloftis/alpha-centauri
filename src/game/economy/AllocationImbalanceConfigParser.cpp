#include "game/economy/AllocationImbalanceConfigParser.h"

#include "lib/config/LuaConfigLoader.h"

#include <string>

namespace ac
{

namespace
{

constexpr const char* k_Noun = "energy allocation";

} // namespace

AllocationImbalanceConfig_t AllocationImbalanceConfigParser::ParseConfig(
    const std::string& formulaPath, LuaRuntime& rLua)
{
    const sol::table table = LuaConfigLoader::RunScript(rLua, formulaPath, k_Noun);
    AllocationImbalanceConfig_t config;
    config.labsLossFormula =
        LuaConfigLoader::RequireFormula(table, "labs_loss_formula", formulaPath, k_Noun);
    config.econLossFormula =
        LuaConfigLoader::RequireFormula(table, "econ_loss_formula", formulaPath, k_Noun);
    config.psychLossFormula =
        LuaConfigLoader::RequireFormula(table, "psych_loss_formula", formulaPath, k_Noun);
    return config;
}

} // namespace ac
