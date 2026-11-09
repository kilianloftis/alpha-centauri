#include "game/economy/AllocationImbalanceConfigParser.h"

#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

std::string RequireFormula_(sol::table& rTable, const char* pKey, const std::string& rFormulaPath)
{
    std::string formula = rTable.get_or(pKey, std::string(""));
    if (formula.empty())
    {
        throw std::runtime_error("Energy allocation script '" + rFormulaPath + "' must set "
                                 + pKey + " to a non-empty Lua expression");
    }
    return formula;
}

} // namespace

AllocationImbalanceConfig_t AllocationImbalanceConfigParser::ParseConfig(
    const std::string& formulaPath, LuaRuntime& rLua)
{
    sol::protected_function_result result =
        rLua.GetState().safe_script_file(formulaPath, sol::script_pass_on_error);
    if (!result.valid())
    {
        const sol::error err = result;
        throw std::runtime_error("Failed to load energy allocation script '" + formulaPath
                                 + "': " + err.what());
    }

    sol::table table = result;
    AllocationImbalanceConfig_t config;
    config.labsLossFormula = RequireFormula_(table, "labs_loss_formula", formulaPath);
    config.econLossFormula = RequireFormula_(table, "econ_loss_formula", formulaPath);
    config.psychLossFormula = RequireFormula_(table, "psych_loss_formula", formulaPath);
    return config;
}

} // namespace ac
