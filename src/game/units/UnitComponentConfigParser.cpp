#include "game/units/UnitComponentConfigParser.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

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

    if (rComponentJson.contains("stats"))
    {
        for (const auto& [key, val] : rComponentJson["stats"].items())
        {
            config.stats[key] = ParseStatBlock(val);
        }
    }

    if (rComponentJson.contains("flags"))
    {
        for (const auto& [key, val] : rComponentJson["flags"].items())
        {
            config.flags[key] = val.get<bool>();
        }
    }

    if (rComponentJson.contains("bonus_tables"))
    {
        for (const auto& [tableName, tableVal] : rComponentJson["bonus_tables"].items())
        {
            for (const auto& [key, val] : tableVal.items())
            {
                config.bonusTables[tableName][key] = val.get<float>();
            }
        }
    }

    return config;
}

StatBlock_t UnitComponentConfigParser::ParseStatBlock(const nlohmann::json& rStatJson)
{
    StatBlock_t stat;
    stat.base = rStatJson.value("base", 0.0f);
    stat.additiveMult = rStatJson.value("additive_mult", 0.0f);
    stat.geometricMult = rStatJson.value("geometric_mult", 1.0f);
    return stat;
}


} // namespace ac
