#pragma once

#include "game/display/MapOverlayChannelsConfig.h"
#include <string>

namespace ac
{

class MapOverlayChannelsConfigParser
{
public:
    static MapOverlayChannelsConfig_t ParseConfig(const std::string& rConfigPath);
};

} // namespace ac
