#pragma once

#include "game/buildings/BuildingConfig.h"
#include <string>
#include <vector>

namespace ac
{

class BuildingConfigParser
{
public:
    static std::vector<BuildingConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
