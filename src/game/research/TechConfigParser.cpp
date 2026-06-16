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
            throw std::runtime_error("Could not open " + configPath);
        }

        json configJson;
        configFile >> configJson;

        if (!configJson.is_array())
        {
            throw std::runtime_error("Expected array of techs in config");
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
    config.category = techJson.value("category", std::string{});
    config.cost = techJson.value("cost", 0);

    if (techJson.contains("prerequisites") && techJson["prerequisites"].is_array())
    {
        for (const auto& prereq : techJson["prerequisites"])
        {
            config.prerequisites.push_back(prereq.get<std::string>());
        }
    }

    return config;
}

} // namespace ac
