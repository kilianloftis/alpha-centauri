#include "game/units/armour/ArmourConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

std::vector<ArmourConfig_t> ArmourConfigParser::ParseConfig(const std::string& rConfigPath)
{
    std::cout << "Loading armour configuration from: " << rConfigPath << "\n";

    std::ifstream configFile(rConfigPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + rConfigPath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of armour in '" + rConfigPath + "'");
    }

    std::vector<ArmourConfig_t> configs;
    for (const auto& rArmourJson : configJson)
    {
        configs.push_back(ParseArmourConfig(rArmourJson));
    }

    std::cout << "Loaded " << configs.size() << " armour configurations\n";
    return configs;
}

ArmourConfig_t ArmourConfigParser::ParseArmourConfig(const nlohmann::json& rArmourJson)
{
    ArmourConfig_t config;
    config.id = rArmourJson["id"];
    config.name = rArmourJson.value("name", config.id);
    config.requiredTech = rArmourJson.value("required_tech", std::string(""));
    config.mineralCost = rArmourJson.value("mineral_cost", 0);
    config.defense = rArmourJson.value("defense", 0);
    return config;
}

} // namespace ac
