#include "game/units/chassis/ChassisConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

std::vector<ChassisConfig_t> ChassisConfigParser::ParseConfig(const std::string& rConfigPath)
{
    std::cout << "Loading chassis configuration from: " << rConfigPath << "\n";

    std::ifstream configFile(rConfigPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + rConfigPath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of chassis in '" + rConfigPath + "'");
    }

    std::vector<ChassisConfig_t> configs;
    for (const auto& rChassisJson : configJson)
    {
        configs.push_back(ParseChassisConfig(rChassisJson));
    }

    std::cout << "Loaded " << configs.size() << " chassis configurations\n";
    return configs;
}

ChassisConfig_t ChassisConfigParser::ParseChassisConfig(const nlohmann::json& rChassisJson)
{
    ChassisConfig_t config;
    config.id = rChassisJson["id"];
    config.name = rChassisJson.value("name", config.id);
    config.requiredTech = rChassisJson.value("required_tech", std::string(""));
    config.mineralCost = rChassisJson.value("mineral_cost", 0);
    return config;
}

} // namespace ac
