#pragma once

#include <string>

namespace ac
{

class LuaRuntime;

struct ProductionCostConfig
{
    std::string costFormula;
};

class ProductionCostConfigParser
{
public:
    ProductionCostConfigParser() = default;
    ~ProductionCostConfigParser() = default;

    // Load production_cost.lua via the shared Lua runtime.
    // Returns a default config on failure.
    ProductionCostConfig ParseConfig(const std::string& scriptPath, LuaRuntime& rLua);
};

} // namespace ac
