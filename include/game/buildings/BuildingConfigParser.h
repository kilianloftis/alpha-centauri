#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

struct BuildingImprovementBonus_t
{
    int nutrients;
};

struct BuildingConfig
{
    std::string id;
    std::string name;
    int nutrientsBonus;
    std::unordered_map<std::string, BuildingImprovementBonus_t> improvementBonuses;
};

class BuildingConfigParser
{
public:
    BuildingConfigParser();
    ~BuildingConfigParser() = default;

    std::vector<BuildingConfig> ParseConfig(const std::string& configPath);

private:
    BuildingConfig ParseBuildingConfig(const nlohmann::json& buildingJson);
};

} // namespace ac
