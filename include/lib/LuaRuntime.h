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
    // Each distinct formula is compiled once and the chunk kept: callers evaluate the same
    // handful of config strings over and over (hurry pricing bisects over one), and compiling
    // per call cost an order of magnitude more than running the result. A formula that fails
    // to compile is not kept, so a later call reports the same error rather than a stale hit.
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
    // Compiled "return <formula>" for a formula string, compiling it on first use. Throws if
    // the formula does not compile.
    sol::protected_function& LoadChunk_(const std::string& formula);

    sol::state m_lua;
    // Declared after m_lua so the chunks — which hold references into that state — are
    // destroyed before it.
    std::unordered_map<std::string, sol::protected_function> m_chunks;
};

} // namespace ac
