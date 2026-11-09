#pragma once

#include "game/economy/AllocationImbalanceConfig.h"

#include <string>

namespace ac
{

class LuaRuntime;

class AllocationImbalanceConfigParser
{
public:
    AllocationImbalanceConfigParser() = default;
    ~AllocationImbalanceConfigParser() = default;

    // Load energy_allocation.lua loss formulas. Throws if the script does not name all three.
    AllocationImbalanceConfig_t ParseConfig(const std::string& formulaPath, LuaRuntime& rLua);
};

} // namespace ac
