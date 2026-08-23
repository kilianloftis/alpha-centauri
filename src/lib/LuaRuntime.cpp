#include "lib/LuaRuntime.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

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
        "sqrt  = math.sqrt\n"
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

sol::protected_function& LuaRuntime::LoadChunk_(const std::string& formula)
{
    const auto it = m_chunks.find(formula);
    if (it != m_chunks.end())
    {
        return it->second;
    }

    sol::load_result chunk = m_lua.load("return " + formula);
    if (!chunk.valid())
    {
        const sol::error err = chunk;
        throw std::runtime_error("Lua formula error (\"" + formula + "\"): " + err.what());
    }
    return m_chunks.emplace(formula, chunk.get<sol::protected_function>()).first->second;
}

int LuaRuntime::EvalInt(const std::string& formula,
                        const std::unordered_map<std::string, double>& vars)
{
    if (formula.empty())
    {
        throw std::runtime_error("Lua formula is empty");
    }

    // Before the globals are set: a formula that does not compile cannot leave any behind.
    sol::protected_function& rChunk = LoadChunk_(formula);

    // Globals, not a per-call environment: config scripts define helper functions whose _ENV is
    // the globals table, so a formula calling one must see these there. Cleared on the way out
    // so an input a later formula forgets to set cannot read this call's value.
    for (const auto& [name, value] : vars)
    {
        m_lua[name] = value;
    }
    const auto clearVars = [this, &vars]() {
        for (const auto& [name, value] : vars)
        {
            m_lua[name] = sol::lua_nil;
        }
    };

    lua_Number number = 0.0;
    try
    {
        sol::protected_function_result result = rChunk();

        if (!result.valid())
        {
            const sol::error err = result;
            throw std::runtime_error("Lua formula error (\"" + formula + "\"): " + err.what());
        }

        const sol::optional<lua_Number> value = result.get<sol::optional<lua_Number>>();
        if (!value)
        {
            throw std::runtime_error("Lua formula (\"" + formula + "\") did not return a number");
        }
        number = *value;
    }
    catch (...)
    {
        clearVars();
        throw;
    }
    clearVars();

    lua_Number integral = 0.0;
    if (std::modf(number, &integral) != 0.0)
    {
        throw std::runtime_error("Lua formula (\"" + formula + "\") returned "
                                 + std::to_string(number) + ", which is not a whole number");
    }
    if (integral < static_cast<lua_Number>(std::numeric_limits<int>::min())
        || integral > static_cast<lua_Number>(std::numeric_limits<int>::max()))
    {
        throw std::runtime_error("Lua formula (\"" + formula + "\") returned "
                                 + std::to_string(number) + ", which does not fit in an int");
    }
    return static_cast<int>(integral);
}

} // namespace ac
