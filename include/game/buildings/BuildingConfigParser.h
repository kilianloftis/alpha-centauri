#pragma once

#include "game/buildings/BuildingConfig.h"
#include <string>
#include <vector>

namespace ac
{

class BuildingConfigParser
{
public:
    BuildingConfigParser() = default;
    ~BuildingConfigParser() = default;

    std::vector<BuildingConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
