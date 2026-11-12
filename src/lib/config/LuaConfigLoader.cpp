#include "lib/config/LuaConfigLoader.h"

#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <utility>

namespace ac
{
namespace LuaConfigLoader
{

sol::table RunScript(LuaRuntime& rLua, const std::string& rScriptPath, const char* noun)
{
    sol::protected_function_result result =
        rLua.GetState().safe_script_file(rScriptPath, sol::script_pass_on_error);
    if (!result.valid())
    {
        const sol::error err = result;
        throw std::runtime_error("Failed to load " + std::string(noun) + " script '"
                                 + rScriptPath + "': " + err.what());
    }
    return result;
}

Formula_t RequireFormula(LuaRuntime& rLua, const sol::table& rTable, const char* pKey,
                         std::vector<std::string> variables, const std::string& rScriptPath,
                         const char* noun)
{
    Formula_t formula{rTable.get_or(pKey, std::string("")), std::move(variables)};
    if (formula.expression.empty())
    {
        throw std::runtime_error(std::string(noun) + " script '" + rScriptPath + "' must set "
                                 + pKey + " to a non-empty Lua expression");
    }
    try
    {
        rLua.CheckFormula(formula);
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error(std::string(noun) + " script '" + rScriptPath + "': " + pKey
                                 + " is not a valid formula: " + e.what());
    }
    return formula;
}

} // namespace LuaConfigLoader
} // namespace ac
