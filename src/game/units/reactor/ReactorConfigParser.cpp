#include "game/units/reactor/ReactorConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

std::vector<ReactorConfig_t> ReactorConfigParser::ParseConfig(const std::string& rConfigPath)
{
    std::cout << "Loading reactor configuration from: " << rConfigPath << "\n";

    std::ifstream configFile(rConfigPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + rConfigPath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of reactors in '" + rConfigPath + "'");
    }

    std::vector<ReactorConfig_t> configs;
    for (const auto& rReactorJson : configJson)
    {
        configs.push_back(ParseReactorConfig(rReactorJson));
    }

    std::cout << "Loaded " << configs.size() << " reactor configurations\n";
    return configs;
}

ReactorConfig_t ReactorConfigParser::ParseReactorConfig(const nlohmann::json& rReactorJson)
{
    ReactorConfig_t config;
    config.id = rReactorJson["id"];
    config.name = rReactorJson.value("name", config.id);
    config.requiredTech = rReactorJson.value("required_tech", std::string(""));
    config.mineralCost = rReactorJson.value("mineral_cost", 0);
    return config;
}

} // namespace ac
