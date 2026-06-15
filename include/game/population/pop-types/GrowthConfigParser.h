#pragma once

#include <string>

namespace ac
{

class LuaRuntime;

struct GrowthConfig
{
    std::string thresholdFormula;  // Lua expression: nutrients required to grow
};

class GrowthConfigParser
{
public:
    GrowthConfigParser() = default;
    ~GrowthConfigParser() = default;

    // Load pop_growth.lua via the shared Lua runtime.
    // Returns a default config on failure.
    GrowthConfig ParseConfig(const std::string& scriptPath, LuaRuntime& rLua);
};

} // namespace ac
