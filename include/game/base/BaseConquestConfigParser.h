#pragma once

#include "game/base/BaseConquestConfig.h"
#include <string>

namespace ac
{

class BaseConquestConfigParser
{
public:
    static BaseConquestConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
