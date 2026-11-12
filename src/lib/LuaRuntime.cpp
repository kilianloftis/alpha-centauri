#include "lib/LuaRuntime.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

// Sets vars as globals for one evaluation and clears them however it ends.
class ScopedGlobals_
{
public:
    ScopedGlobals_(sol::state& rLua, const FormulaVars_t& rVars)
        : m_rLua(rLua)
        , m_rVars(rVars)
    {
        for (const auto& [name, value] : m_rVars)
        {
            m_rLua[name] = value;
        }
    }
    ScopedGlobals_(const ScopedGlobals_&) = delete;
    ScopedGlobals_& operator=(const ScopedGlobals_&) = delete;
    ~ScopedGlobals_()
    {
        for (const auto& [name, value] : m_rVars)
        {
            m_rLua[name] = sol::lua_nil;
        }
    }

private:
    sol::state& m_rLua;
    const FormulaVars_t& m_rVars;
};

lua_Number ResultNumber_(const sol::protected_function_result& rResult,
                         const std::string& expression)
{
    if (!rResult.valid())
    {
        const sol::error err = rResult;
        throw std::runtime_error("Lua formula error (\"" + expression + "\"): " + err.what());
    }
    const sol::optional<lua_Number> value = rResult.get<sol::optional<lua_Number>>();
    if (!value)
    {
        throw std::runtime_error("Lua formula (\"" + expression + "\") did not return a number");
    }
    return *value;
}

// The caller and the config disagreeing on a formula's variables is a code error.
void RequireDeclaredVariables_(const Formula_t& rFormula, const FormulaVars_t& rVars)
{
    const auto isGiven = [&rVars](const std::string& rName) { return rVars.contains(rName); };
    const bool bDeclared = rVars.size() == rFormula.variables.size()
        && std::ranges::all_of(rFormula.variables, isGiven);
    if (!bDeclared)
    {
        throw std::logic_error("Lua formula (\"" + rFormula.expression
                               + "\") was not given exactly the variables it declares");
    }
}

} // namespace

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
        "setmetatable(_G, { __index = function(_, name)\n"
        "    error(\"unknown variable '\" .. tostring(name) .. \"'\", 2)\n"
        "end })\n"
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

sol::protected_function& LuaRuntime::LoadChunk_(const std::string& expression)
{
    const auto it = m_chunks.find(expression);
    if (it != m_chunks.end())
    {
        return it->second;
    }

    sol::load_result chunk = m_lua.load("return " + expression);
    if (!chunk.valid())
    {
        const sol::error err = chunk;
        throw std::runtime_error("Lua formula error (\"" + expression + "\"): " + err.what());
    }
    return m_chunks.emplace(expression, chunk.get<sol::protected_function>()).first->second;
}

lua_Number LuaRuntime::EvalNumber_(const std::string& expression, const FormulaVars_t& rVars)
{
    // Before the globals are set: a formula that does not compile cannot leave any behind.
    sol::protected_function& rChunk = LoadChunk_(expression);
    // Globals, so helper functions defined by config scripts can see them. Cleared afterwards
    // so a later formula cannot read an input it forgot to set.
    const ScopedGlobals_ globals(m_lua, rVars);
    return ResultNumber_(rChunk(), expression);
}

double LuaRuntime::EvalDouble(const Formula_t& rFormula, const FormulaVars_t& rVars)
{
    RequireDeclaredVariables_(rFormula, rVars);
    const lua_Number number = EvalNumber_(rFormula.expression, rVars);
    if (!std::isfinite(number))
    {
        throw std::runtime_error("Lua formula (\"" + rFormula.expression + "\") returned "
                                 + std::to_string(number) + ", which is not finite");
    }
    return static_cast<double>(number);
}

int LuaRuntime::EvalInt(const Formula_t& rFormula, const FormulaVars_t& rVars)
{
    const double number = EvalDouble(rFormula, rVars);

    double integral = 0.0;
    if (std::modf(number, &integral) != 0.0)
    {
        throw std::runtime_error("Lua formula (\"" + rFormula.expression + "\") returned "
                                 + std::to_string(number) + ", which is not a whole number");
    }
    if (integral < static_cast<double>(std::numeric_limits<int>::min())
        || integral > static_cast<double>(std::numeric_limits<int>::max()))
    {
        throw std::runtime_error("Lua formula (\"" + rFormula.expression + "\") returned "
                                 + std::to_string(number) + ", which does not fit in an int");
    }
    return static_cast<int>(integral);
}

void LuaRuntime::CheckFormula(const Formula_t& rFormula)
{
    // One: at zero, a formula that divides by a variable would run on infinities.
    FormulaVars_t vars;
    for (const std::string& rName : rFormula.variables)
    {
        vars.emplace(rName, 1.0);
    }
    (void)EvalNumber_(rFormula.expression, vars);
}

} // namespace ac
