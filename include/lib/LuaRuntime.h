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
    // Returns 0 and logs a warning on error.
    int EvalInt(const std::string& formula,
                const std::unordered_map<std::string, int>& vars);

private:
    sol::state m_lua;
};

} // namespace ac
