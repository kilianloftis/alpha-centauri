#pragma once

#include "lib/Formula.h"

#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

namespace ac
{

// The sandboxed Lua state (math only; no io, os or debug) that runs mod scripts and config
// formulas. Reading a global that does not exist is an error, so a misspelt variable fails.
class LuaRuntime
{
public:
    LuaRuntime();
    ~LuaRuntime() = default;

    // Access the underlying Lua state (e.g. for registering C++ bindings)
    sol::state& GetState();
    const sol::state& GetState() const;

    // Evaluates `return <expression>` with rVars as globals, cleared afterwards. rVars names
    // exactly rFormula's variables. Throws unless the result is a whole number in int range: a
    // broken formula is a config error.
    int EvalInt(const Formula_t& rFormula, const FormulaVars_t& rVars);

    // Same as EvalInt but allows fractional results (e.g. promotion chance 0.0–1.0).
    // Throws if the formula fails to evaluate or returns a non-finite number.
    double EvalDouble(const Formula_t& rFormula, const FormulaVars_t& rVars);

    // Throws unless rFormula compiles and returns a number with every variable at 1. Its value
    // is not checked, since what is valid depends on the real inputs.
    void CheckFormula(const Formula_t& rFormula);

private:
    // Compiled "return <expression>", compiling it on first use. Throws if it does not compile.
    sol::protected_function& LoadChunk_(const std::string& expression);
    lua_Number EvalNumber_(const std::string& expression, const FormulaVars_t& rVars);

    sol::state m_lua;
    // One compiled chunk per formula, since hot formulas run many times a turn. After m_lua, so
    // the chunks die before the state they reference.
    std::unordered_map<std::string, sol::protected_function> m_chunks;
};

} // namespace ac
