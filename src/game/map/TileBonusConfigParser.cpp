#include "game/map/TileBonusConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

TileBonusConfigParser::TileBonusConfigParser()
{
}

std::vector<TileBonusConfig> TileBonusConfigParser::ParseConfig(const std::string& configPath)
{
    std::cout << "Loading tile bonus configuration from: " << configPath << "\n";

    std::vector<TileBonusConfig> configs;

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
            std::cout << "Error: Expected array of tile bonuses in config\n";
            return configs;
        }

        for (const auto& bonusJson : configJson)
        {
            configs.push_back(ParseTileBonusConfig(bonusJson));
        }

        std::cout << "Loaded " << configs.size() << " tile bonus configurations\n";
        return configs;
    }
    catch (const std::exception& e)
    {
        std::cout << "Error loading tile bonus config: " << e.what() << "\n";
        return configs;
    }
}

TileBonusConfig TileBonusConfigParser::ParseTileBonusConfig(const nlohmann::json& bonusJson)
{
    TileBonusConfig config;
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
