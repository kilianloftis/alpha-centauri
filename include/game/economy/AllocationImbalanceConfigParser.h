#pragma once

#include "game/economy/AllocationImbalanceConfig.h"

#include <string>

namespace ac
{

class LuaRuntime;

class AllocationImbalanceConfigParser
{
public:
    // Load energy_allocation.lua loss formulas. Throws if the script does not name all three.
    static AllocationImbalanceConfig_t ParseConfig(const std::string& formulaPath,
                                                   LuaRuntime& rLua);
};

} // namespace ac
