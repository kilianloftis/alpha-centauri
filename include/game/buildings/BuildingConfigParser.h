#pragma once

#include "game/buildings/BuildingConfig.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

class BuildingConfigParser
{
public:
    BuildingConfigParser();
    ~BuildingConfigParser() = default;

    std::vector<BuildingConfig_t> ParseConfig(const std::string& configPath);

private:
    BuildingConfig_t ParseBuildingConfig_(const nlohmann::json& buildingJson);
};

} // namespace ac
