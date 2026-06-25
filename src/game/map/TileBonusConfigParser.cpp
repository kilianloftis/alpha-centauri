#include "game/map/TileBonusConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

TileBonusConfig_tParser::TileBonusConfig_tParser()
{
}

std::vector<TileBonusConfig_t> TileBonusConfig_tParser::ParseConfig(const std::string& configPath)
{
    std::cout << "Loading tile bonus configuration from: " << configPath << "\n";

    std::vector<TileBonusConfig_t> configs;

    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + configPath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of tile bonuses in '" + configPath + "'");
    }

    for (const auto& bonusJson : configJson)
    {
        configs.push_back(ParseTileBonusConfig_t(bonusJson));
    }

    std::cout << "Loaded " << configs.size() << " tile bonus configurations\n";
    return configs;
}

TileBonusConfig_t TileBonusConfig_tParser::ParseTileBonusConfig_t(const nlohmann::json& bonusJson)
{
    TileBonusConfig_t config;
    config.id = bonusJson["id"];
    config.name = bonusJson.value("name", config.id);
    config.description = bonusJson.value("description", "");

    if (bonusJson.contains("bonuses"))
    {
        const auto& bJson = bonusJson["bonuses"];
        config.nutrients = bJson.value("nutrients", 0);
        config.minerals  = bJson.value("minerals", 0);
        config.energy    = bJson.value("energy", 0);
    }
    else
    {
        config.nutrients = 0;
        config.minerals = 0;
        config.energy = 0;
    }

    config.spritePath = bonusJson.value("sprite_path", "");
    config.frequency = bonusJson.value("frequency", 0);

    return config;
}

} // namespace ac
