#include "game/units/weapon/WeaponConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

std::vector<WeaponConfig_t> WeaponConfigParser::ParseConfig(const std::string& rConfigPath)
{
    std::cout << "Loading weapon configuration from: " << rConfigPath << "\n";

    std::ifstream configFile(rConfigPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + rConfigPath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of weapons in '" + rConfigPath + "'");
    }

    std::vector<WeaponConfig_t> configs;
    for (const auto& rWeaponJson : configJson)
    {
        configs.push_back(ParseWeaponConfig(rWeaponJson));
    }

    std::cout << "Loaded " << configs.size() << " weapon configurations\n";
    return configs;
}

WeaponConfig_t WeaponConfigParser::ParseWeaponConfig(const nlohmann::json& rWeaponJson)
{
    WeaponConfig_t config;
    config.id = rWeaponJson["id"];
    config.name = rWeaponJson.value("name", config.id);
    config.requiredTech = rWeaponJson.value("required_tech", std::string(""));
    config.mineralCost = rWeaponJson.value("mineral_cost", 0);
    config.attack = rWeaponJson.value("attack", 0);
    return config;
}

} // namespace ac
