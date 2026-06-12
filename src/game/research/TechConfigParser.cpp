#include "game/research/TechConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

TechConfigParser::TechConfigParser()
{
}

std::vector<TechConfig> TechConfigParser::ParseConfig(const std::string& configPath)
{
    std::cout << "Loading tech configuration from: " << configPath << "\n";

    std::vector<TechConfig> configs;

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
            std::cout << "Error: Expected array of techs in config\n";
            return configs;
        }

        for (const auto& techJson : configJson)
        {
            configs.push_back(ParseTechConfig(techJson));
        }

        std::cout << "Loaded " << configs.size() << " tech configurations\n";
        return configs;
    }
    catch (const std::exception& e)
    {
        std::cout << "Error loading tech config: " << e.what() << "\n";
        return configs;
    }
}

TechConfig TechConfigParser::ParseTechConfig(const nlohmann::json& techJson)
{
    TechConfig config;
    config.id = techJson["id"];
    config.name = techJson.value("name", config.id);
    config.cost = techJson.value("cost", 0);
    return config;
}

} // namespace ac
