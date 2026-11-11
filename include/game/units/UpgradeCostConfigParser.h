#pragma once

#include "game/units/UpgradeCostConfig.h"

#include <string>

namespace ac
{

class LuaRuntime;

class UpgradeCostConfigParser
{
public:
    static UpgradeCostConfig_t ParseConfig(const std::string& rScriptPath, LuaRuntime& rLua);
};

} // namespace ac
