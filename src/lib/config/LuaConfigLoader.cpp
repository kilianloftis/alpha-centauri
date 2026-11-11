#include "lib/config/LuaConfigLoader.h"

#include "lib/LuaRuntime.h"

#include <stdexcept>

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

std::string RequireFormula(const sol::table& rTable, const char* pKey,
                           const std::string& rScriptPath, const char* noun)
{
    std::string formula = rTable.get_or(pKey, std::string(""));
    if (formula.empty())
    {
        throw std::runtime_error(std::string(noun) + " script '" + rScriptPath + "' must set "
                                 + pKey + " to a non-empty Lua expression");
    }
    return formula;
}

} // namespace LuaConfigLoader
} // namespace ac
