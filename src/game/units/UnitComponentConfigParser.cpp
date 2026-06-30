#include "game/units/UnitComponentConfigParser.h"
#include "lib/effects/BonusEffectParser.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace ac
{

std::vector<UnitComponentConfig_t> UnitComponentConfigParser::ParseConfig(const std::string& rConfigPath)
{
    if (fs::is_directory(rConfigPath))
    {
        std::vector<fs::path> files;
        for (const auto& rEntry : fs::directory_iterator(rConfigPath))
        {
            if (rEntry.path().extension() == ".json")
                files.push_back(rEntry.path());
        }
        std::sort(files.begin(), files.end());

        std::vector<UnitComponentConfig_t> all;
        for (const auto& rFile : files)
        {
            auto configs = ParseFile_(rFile.string());
            all.insert(all.end(), configs.begin(), configs.end());
        }
        return all;
    }

    return ParseFile_(rConfigPath);
}

std::vector<UnitComponentConfig_t> UnitComponentConfigParser::ParseFile_(const std::string& rFilePath)
{
    std::cout << "Loading unit component configuration from: " << rFilePath << "\n";

    std::ifstream configFile(rFilePath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + rFilePath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of unit components in '" + rFilePath + "'");
    }

    std::vector<UnitComponentConfig_t> configs;
    for (const auto& rComponentJson : configJson)
    {
        configs.push_back(ParseComponentConfig(rComponentJson));
    }

    std::cout << "Loaded " << configs.size() << " unit component configurations\n";
    return configs;
}

UnitComponentConfig_t UnitComponentConfigParser::ParseComponentConfig(const nlohmann::json& rComponentJson)
{
    UnitComponentConfig_t config;
    config.id = rComponentJson["id"];
    config.name = rComponentJson.value("name", config.id);
    config.type = rComponentJson["type"].get<std::string>();
    config.requiredTech = rComponentJson.value("required_tech", std::string(""));
    config.mineralCost = rComponentJson.value("mineral_cost", 0);
    config.effects = BonusEffectParser::ParseEffects(rComponentJson);

    return config;
}

} // namespace ac
