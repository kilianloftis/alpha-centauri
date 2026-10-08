#pragma once

#include "game/buildings/BaseSpriteSizesConfig.h"
#include <string>

namespace ac
{

class BaseSpriteSizesConfigParser
{
public:
    BaseSpriteSizesConfig_t ParseConfig(const std::string& rConfigPath) const;
};

} // namespace ac
