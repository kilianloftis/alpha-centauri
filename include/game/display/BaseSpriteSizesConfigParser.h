#pragma once

#include "game/display/BaseSpriteSizesConfig.h"
#include <string>

namespace ac
{

class BaseSpriteSizesConfigParser
{
public:
    BaseSpriteSizesConfig_t ParseConfig(const std::string& rConfigPath) const;
};

} // namespace ac
