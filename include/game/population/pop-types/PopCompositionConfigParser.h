#pragma once

#include <string>
#include <vector>

namespace ac
{

class LuaRuntime;

struct PopCompositionConfig
{
    std::string droneFormula;            // Lua expression: number of drones
    std::string talentFormula;           // Lua expression: number of talents
    std::vector<std::string> precedence; // Order in which types are assigned when recalculating
};

class PopCompositionConfigParser
{
public:
    PopCompositionConfigParser() = default;
    ~PopCompositionConfigParser() = default;

    // Load pop_composition.lua via the shared Lua runtime.
    // Returns a default config on failure.
    PopCompositionConfig ParseConfig(const std::string& scriptPath, LuaRuntime& rLua);
};

} // namespace ac
