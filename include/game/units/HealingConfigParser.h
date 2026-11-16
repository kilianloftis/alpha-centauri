#pragma once

#include "game/units/HealingConfig.h"
#include <string>

namespace ac
{

class HealingConfigParser
{
public:
    static HealingConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
