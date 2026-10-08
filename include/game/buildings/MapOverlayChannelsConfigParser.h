#pragma once

#include "game/buildings/MapOverlayChannelsConfig.h"
#include <string>

namespace ac
{

class MapOverlayChannelsConfigParser
{
public:
    MapOverlayChannelsConfig_t ParseConfig(const std::string& rConfigPath) const;
};

} // namespace ac
