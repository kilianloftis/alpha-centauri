#pragma once

#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

namespace ac
{

// The sandboxed Lua state (math only; no io, os or debug) that runs mod scripts and config
// formulas.
class LuaRuntime
{
public:
    LuaRuntime();
    ~LuaRuntime() = default;

    // Access the underlying Lua state (e.g. for registering C++ bindings)
    sol::state& GetState();
    const sol::state& GetState() const;

    // Evaluates `return <formula>` with vars as globals, cleared afterwards. Throws unless the
    // result is a whole number in int range: a broken formula is a config error.
    int EvalInt(const std::string& formula,
                const std::unordered_map<std::string, double>& vars);

    // Same as EvalInt but allows fractional results (e.g. promotion chance 0.0–1.0).
    // Throws if the formula is empty, fails to evaluate, or returns a non-finite number.
    double EvalDouble(const std::string& formula,
                      const std::unordered_map<std::string, double>& vars);

private:
    // Compiled "return <formula>" for a formula string, compiling it on first use. Throws if
    // the formula does not compile.
    sol::protected_function& LoadChunk_(const std::string& formula);

    sol::state m_lua;
    // One compiled chunk per formula, since hot formulas run many times a turn. After m_lua, so
    // the chunks die before the state they reference.
    std::unordered_map<std::string, sol::protected_function> m_chunks;
};

} // namespace ac
