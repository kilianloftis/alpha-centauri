#pragma once

#include "game/units/BaseConquestConfig.h"
#include <string>

namespace ac
{

class BaseConquestConfigParser
{
public:
    BaseConquestConfig_t ParseConfig(const std::string& configPath) const;
};

} // namespace ac
