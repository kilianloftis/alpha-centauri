#pragma once

#include <string>

namespace ac
{

class LuaRuntime;

struct GrowthConfig_t
{
    std::string thresholdFormula;  // Lua expression: nutrients required to grow
};

class GrowthConfig_tParser
{
public:
    GrowthConfig_tParser() = default;
    ~GrowthConfig_tParser() = default;

    // Load pop_growth.lua via the shared Lua runtime.
    // Returns a default config on failure.
    GrowthConfig_t ParseConfig(const std::string& scriptPath, LuaRuntime& rLua);
};

} // namespace ac
