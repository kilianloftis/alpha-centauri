#pragma once

#include <string>

namespace ac
{

class LuaRuntime;

struct ProductionCostConfig_t
{
    std::string costFormula;
};

class ProductionCostConfig_tParser
{
public:
    ProductionCostConfig_tParser() = default;
    ~ProductionCostConfig_tParser() = default;

    // Load production_cost.lua via the shared Lua runtime.
    // Returns a default config on failure.
    ProductionCostConfig_t ParseConfig(const std::string& scriptPath, LuaRuntime& rLua);
};

} // namespace ac
