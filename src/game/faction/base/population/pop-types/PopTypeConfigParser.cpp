#include "game/faction/base/population/pop-types/PopTypeConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

PopTypeConfigParser::PopTypeConfigParser()
{
}

std::vector<PopTypeConfig> PopTypeConfigParser::ParseConfig(const std::string& configPath)
{
    std::cout << "Loading pop type configuration from: " << configPath << "\n";

    std::vector<PopTypeConfig> configs;

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
            std::cout << "Error: Expected array of pop types in config\n";
            return configs;
        }

        for (const auto& popJson : configJson)
        {
            configs.push_back(ParsePopTypeConfig(popJson));
        }

        std::cout << "Loaded " << configs.size() << " pop type configurations\n";
        return configs;
    }
    catch (const std::exception& e)
    {
        std::cout << "Error loading pop type config: " << e.what() << "\n";
        return configs;
    }
}

PopTypeConfig PopTypeConfigParser::ParsePopTypeConfig(const nlohmann::json& popJson)
{
    PopTypeConfig config;
    config.id = popJson["id"];
    config.name = popJson.value("name", config.id);
    config.bCanWorkTile       = popJson.value("can_work_tile",       false);
    config.bPlayerAssignable  = popJson.value("player_assignable",   false);

    if (popJson.contains("tile_multipliers"))
    {
        const auto& mJson = popJson["tile_multipliers"];
        config.tileMultipliers.nutrients = mJson.value("nutrients", 0.0f);
        config.tileMultipliers.energy    = mJson.value("energy",    0.0f);
        config.tileMultipliers.minerals  = mJson.value("minerals",  0.0f);
    }
    else
    {
        config.tileMultipliers = {0.0f, 0.0f, 0.0f};
    }

    if (popJson.contains("generation"))
    {
        const auto& gJson = popJson["generation"];
        config.generation.nutrients = gJson.value("nutrients", 0);
        config.generation.energy    = gJson.value("energy",    0);
        config.generation.minerals  = gJson.value("minerals",  0);
        config.generation.econ      = gJson.value("econ",      0);
        config.generation.labs      = gJson.value("labs",      0);
        config.generation.psych     = gJson.value("psych",     0);
    }
    else
    {
        config.generation = {0, 0, 0, 0, 0, 0};
    }

    config.riotContribution      = popJson.value("riot_contribution",       0);
    config.goldenAgeContribution = popJson.value("golden_age_contribution", 0);
    config.requiredTech          = popJson.value("required_tech",           "");

    if (popJson.contains("obsoletes"))
    {
        for (const auto& entry : popJson["obsoletes"])
        {
            config.obsoletes.push_back(entry.get<std::string>());
        }
    }

    return config;
}

} // namespace ac
