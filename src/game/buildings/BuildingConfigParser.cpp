#include "game/buildings/BuildingConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

BuildingConfigParser::BuildingConfigParser()
{
}

std::vector<BuildingConfig_t> BuildingConfigParser::ParseConfig(const std::string& configPath)
{
    std::cout << "Loading building configuration from: " << configPath << "\n";

    std::vector<BuildingConfig_t> configs;

    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + configPath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of buildings in '" + configPath + "'");
    }

    for (const auto& buildingJson : configJson)
    {
        configs.push_back(ParseBuildingConfig(buildingJson));
    }

    std::cout << "Loaded " << configs.size() << " building configurations\n";
    return configs;
}

BuildingConfig_t BuildingConfigParser::ParseBuildingConfig(const nlohmann::json& buildingJson)
{
    BuildingConfig_t config;
    config.id = buildingJson["id"];
    config.name = buildingJson.value("name", config.id);
    config.nutrientsBonus = buildingJson.value("nutrients_bonus", 0);
    config.mineralCost = buildingJson.value("mineral_cost", 0);
    config.allowMultiple = buildingJson.value("allow_multiple", false);
    config.bIsSecretProject = buildingJson.value("secret_project", false);
    
    if (buildingJson.contains("required_techs"))
    {
        for (const auto& tech : buildingJson["required_techs"])
        {
            config.requiredTechs.push_back(tech);
        }
    }

    if (buildingJson.contains("improvement_bonuses"))
    {
        for (const auto& [key, val] : buildingJson["improvement_bonuses"].items())
        {
            BuildingImprovementBonus_t bonus;
            bonus.nutrients = val.value("nutrients", 0);
            config.improvementBonuses[key] = bonus;
        }
    }

    return config;
}

} // namespace ac
