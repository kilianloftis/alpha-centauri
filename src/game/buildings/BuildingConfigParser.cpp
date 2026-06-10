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

std::vector<BuildingConfig> BuildingConfigParser::ParseConfig(const std::string& configPath)
{
    std::cout << "Loading building configuration from: " << configPath << "\n";

    std::vector<BuildingConfig> configs;

    try
    {
        std::ifstream configFile(configPath);
        if (!configFile.is_open())
        {
            std::cout << "Warning: Could not open " << configPath << "\n";
            return configs;
        }

        json configJson;
        configFile >> configJson;

        if (!configJson.is_array())
        {
            std::cout << "Error: Expected array of buildings in config\n";
            return configs;
        }

        for (const auto& buildingJson : configJson)
        {
            configs.push_back(ParseBuildingConfig(buildingJson));
        }

        std::cout << "Loaded " << configs.size() << " building configurations\n";
        return configs;
    }
    catch (const std::exception& e)
    {
        std::cout << "Error loading building config: " << e.what() << "\n";
        return configs;
    }
}

BuildingConfig BuildingConfigParser::ParseBuildingConfig(const nlohmann::json& buildingJson)
{
    BuildingConfig config;
    config.id = buildingJson["id"];
    config.name = buildingJson.value("name", config.id);
    config.nutrientsBonus = buildingJson.value("nutrients_bonus", 0);

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
