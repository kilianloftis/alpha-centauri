#include "lib/LuaRuntime.h"
#include <iostream>

namespace ac
{

LuaRuntime::LuaRuntime()
{
    // Open safe standard libraries only
    m_lua.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table
    );

    // Convenience aliases so formula authors can write floor() instead of math.floor()
    m_lua.script(
        "floor = math.floor\n"
        "ceil  = math.ceil\n"
        "abs   = math.abs\n"
        "max   = math.max\n"
        "min   = math.min\n"
    );
}

sol::state& LuaRuntime::GetState()
{
    return m_lua;
}

const sol::state& LuaRuntime::GetState() const
{
    return m_lua;
}

int LuaRuntime::EvalInt(const std::string& formula,
                        const std::unordered_map<std::string, int>& vars)
{
    if (formula.empty())
    {
        return 0;
    }

    // Set variables as Lua globals scoped to this call
    for (const auto& [name, value] : vars)
    {
        m_lua[name] = value;
    }

    try
    {
        sol::protected_function_result result =
            m_lua.safe_script("return " + formula, sol::script_pass_on_error);

        if (!result.valid())
        {
            sol::error err = result;
            std::cout << "Warning: Lua formula error (\"" << formula << "\"): " << err.what() << "\n";
            return 0;
        }

        return static_cast<int>(result.get<lua_Number>());
    }
    catch (const sol::error& e)
    {
        std::cout << "Warning: Lua formula error (\"" << formula << "\"): " << e.what() << "\n";
        return 0;
    }
}

} // namespace ac
