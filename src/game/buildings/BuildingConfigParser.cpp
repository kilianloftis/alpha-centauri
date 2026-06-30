#include "game/buildings/BuildingConfigParser.h"
#include "lib/effects/BonusEffectParser.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

using json = nlohmann::json;

namespace ac
{

BuildingConfigParser::BuildingConfigParser()
{
}

std::vector<BuildingConfig_t> BuildingConfigParser::ParseConfig(const std::string& configPath)
{
    if (fs::is_directory(configPath))
    {
        std::vector<fs::path> files;
        for (const auto& rEntry : fs::directory_iterator(configPath))
        {
            if (rEntry.path().extension() == ".json")
                files.push_back(rEntry.path());
        }
        std::sort(files.begin(), files.end());

        std::vector<BuildingConfig_t> all;
        for (const auto& rFile : files)
        {
            auto configs = ParseFile_(rFile.string());
            all.insert(all.end(), configs.begin(), configs.end());
        }
        return all;
    }

    return ParseFile_(configPath);
}

std::vector<BuildingConfig_t> BuildingConfigParser::ParseFile_(const std::string& filePath)
{
    std::cout << "Loading building configuration from: " << filePath << "\n";

    std::ifstream configFile(filePath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + filePath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of buildings in '" + filePath + "'");
    }

    std::vector<BuildingConfig_t> configs;
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

    config.effects = BonusEffectParser::ParseEffects(buildingJson);

    return config;
}

} // namespace ac
