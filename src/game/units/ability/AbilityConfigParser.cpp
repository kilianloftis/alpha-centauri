#include "game/units/ability/AbilityConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

std::vector<AbilityConfig_t> AbilityConfigParser::ParseConfig(const std::string& rConfigPath)
{
    std::cout << "Loading ability configuration from: " << rConfigPath << "\n";

    std::ifstream configFile(rConfigPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + rConfigPath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of abilities in '" + rConfigPath + "'");
    }

    std::vector<AbilityConfig_t> configs;
    for (const auto& rAbilityJson : configJson)
    {
        configs.push_back(ParseAbilityConfig(rAbilityJson));
    }

    std::cout << "Loaded " << configs.size() << " ability configurations\n";
    return configs;
}

AbilityConfig_t AbilityConfigParser::ParseAbilityConfig(const nlohmann::json& rAbilityJson)
{
    AbilityConfig_t config;
    config.id = rAbilityJson["id"];
    config.name = rAbilityJson.value("name", config.id);
    config.requiredTech = rAbilityJson.value("required_tech", std::string(""));
    config.mineralCost = rAbilityJson.value("mineral_cost", 0);
    return config;
}

} // namespace ac
