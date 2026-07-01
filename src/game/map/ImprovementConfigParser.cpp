#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
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

bool CanBuildImprovement(const Tile& rTile, const ImprovementConfig_t& rCandidate)
{
    for (const std::string& excludedId : rCandidate.excludes)
    {
        if (rTile.HasFeature(excludedId))
            return false;
    }
    return true;
}

std::vector<ImprovementConfig_t> ImprovementConfigParser::ParseConfig(const std::string& configPath)
{
    if (fs::is_directory(configPath))
    {
        std::vector<fs::path> files;
        for (const auto& rEntry : fs::directory_iterator(configPath))
        {
            if (rEntry.path().extension() == ".json")
                files.push_back(rEntry.path());
        }
        std::sort(files.begin(), files.end());

        std::vector<ImprovementConfig_t> all;
        for (const auto& rFile : files)
        {
            auto configs = ParseFile_(rFile.string());
            all.insert(all.end(), configs.begin(), configs.end());
        }
        return all;
    }

    return ParseFile_(configPath);
}

std::vector<ImprovementConfig_t> ImprovementConfigParser::ParseFile_(const std::string& filePath)
{
    std::cout << "Loading improvement configuration from: " << filePath << "\n";

    std::ifstream configFile(filePath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + filePath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of improvements in '" + filePath + "'");
    }

    std::vector<ImprovementConfig_t> configs;
    for (const auto& improvementJson : configJson)
    {
        configs.push_back(ParseImprovementConfig(improvementJson));
    }

    std::cout << "Loaded " << configs.size() << " improvement configurations\n";
    return configs;
}

ImprovementConfig_t ImprovementConfigParser::ParseImprovementConfig(const nlohmann::json& improvementJson)
{
    ImprovementConfig_t config;
    config.id = improvementJson["id"];
    config.name = improvementJson.value("name", config.id);
    config.description = improvementJson.value("description", "");
    config.mineralCost = improvementJson.value("mineral_cost", 0);
    config.requiredTech = improvementJson.value("required_tech", "");
    config.radius = improvementJson.value("radius", 0);
    config.frequency = improvementJson.value("frequency", 0);
    config.spritePath = improvementJson.value("sprite_path", "");

    if (improvementJson.contains("excludes"))
    {
        for (const auto& excludedId : improvementJson["excludes"])
        {
            config.excludes.push_back(excludedId.get<std::string>());
        }
    }

    config.effects = BonusEffectParser::ParseEffects(improvementJson);

    return config;
}

} // namespace ac
