#include "game/economy/AllocationImbalanceConfigParser.h"

#include "lib/config/LuaConfigLoader.h"

#include <string>

namespace ac
{

namespace
{

constexpr const char* k_Noun = "energy allocation";

Formula_t RequireLossFormula_(LuaRuntime& rLua, const sol::table& rTable, const char* pKey,
                              const std::string& rFormulaPath)
{
    return LuaConfigLoader::RequireFormula(
        rLua, rTable, pKey, {"econ_percent", "labs_percent", "psych_percent", "imbalance_factor"},
        rFormulaPath, k_Noun);
}

} // namespace

AllocationImbalanceConfig_t AllocationImbalanceConfigParser::ParseConfig(
    const std::string& formulaPath, LuaRuntime& rLua)
{
    const sol::table table = LuaConfigLoader::RunScript(rLua, formulaPath, k_Noun);
    AllocationImbalanceConfig_t config;
    config.labsLossFormula = RequireLossFormula_(rLua, table, "labs_loss_formula", formulaPath);
    config.econLossFormula = RequireLossFormula_(rLua, table, "econ_loss_formula", formulaPath);
    config.psychLossFormula = RequireLossFormula_(rLua, table, "psych_loss_formula", formulaPath);
    return config;
}

} // namespace ac
