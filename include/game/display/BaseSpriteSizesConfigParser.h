#pragma once

#include "game/display/BaseSpriteSizesConfig.h"
#include <string>

namespace ac
{

class BaseSpriteSizesConfigParser
{
public:
    static BaseSpriteSizesConfig_t ParseConfig(const std::string& rConfigPath);
};

} // namespace ac
