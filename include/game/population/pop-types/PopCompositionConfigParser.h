#pragma once

#include <string>

namespace ac
{

class LuaRuntime;

struct PopCompositionConfig_t
{
    std::string droneFormula;  // Lua expression: number of drones
    std::string talentFormula; // Lua expression: number of talents
    std::string droneTypeId;   // Pop type id to convert into for drones
    std::string talentTypeId;  // Pop type id to convert into for talents
};

class PopCompositionConfigParser
{
public:
    PopCompositionConfigParser() = default;
    ~PopCompositionConfigParser() = default;

    // Load pop_composition.lua via the shared Lua runtime.
    // Throws if the script cannot be loaded.
    PopCompositionConfig_t ParseConfig(const std::string& scriptPath, LuaRuntime& rLua);
};

} // namespace ac
