#pragma once

#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

namespace ac
{

// Owns the shared Lua state used across the engine.
// Provides a sandboxed environment: math library available, io/os/debug removed.
// Modding scripts and config formula strings are evaluated through this runtime.
class LuaRuntime
{
public:
    LuaRuntime();
    ~LuaRuntime() = default;

    // Access the underlying Lua state (e.g. for registering C++ bindings)
    sol::state& GetState();
    const sol::state& GetState() const;

    // Evaluate a formula string as a Lua expression with named integer variables.
    // The expression is wrapped as "return <formula>" and executed.
    //
    // Throws if the formula is empty, fails to evaluate (the Lua message is included), returns
    // a non-number, returns a fractional value, or returns something outside int range. A
    // formula is game data: a broken one is a config error, not a zero.
    //
    // vars are visible to the formula as globals and are cleared afterwards, so a formula that
    // omits an input cannot read a previous call's value.
    int EvalInt(const std::string& formula,
                const std::unordered_map<std::string, int>& vars);

private:
    sol::state m_lua;
};

} // namespace ac
