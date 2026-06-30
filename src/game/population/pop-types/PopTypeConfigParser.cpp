#include "game/population/pop-types/PopTypeConfigParser.h"
#include "lib/effects/BonusEffectParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

PopTypeConfigParser::PopTypeConfigParser()
{
}

std::vector<PopTypeConfig_t> PopTypeConfigParser::ParseConfig(const std::string& configPath)
{
    std::cout << "Loading pop type configuration from: " << configPath << "\n";

    std::vector<PopTypeConfig_t> configs;

    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + configPath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of pop types in '" + configPath + "'");
    }

    for (const auto& popJson : configJson)
    {
        configs.push_back(ParsePopTypeConfig(popJson));
    }

    std::cout << "Loaded " << configs.size() << " pop type configurations\n";
    return configs;
}

PopTypeConfig_t PopTypeConfigParser::ParsePopTypeConfig(const nlohmann::json& popJson)
{
    PopTypeConfig_t config;
    config.id = popJson["id"];
    config.name = popJson.value("name", config.id);
    config.bIsDefault         = popJson.value("is_default",          false);
    config.bCanWorkTile       = popJson.value("can_work_tile",       false);
    config.bPlayerAssignable  = popJson.value("player_assignable",   false);
    config.riotContribution      = popJson.value("riot_contribution",       0);
    config.goldenAgeContribution = popJson.value("golden_age_contribution", 0);
    config.requiredTech          = popJson.value("required_tech",           "");
    config.fallbackPopTypeId     = popJson.value("fallback_pop_type",       "");
    config.effects               = BonusEffectParser::ParseEffects(popJson);

    if (popJson.contains("obsoletes"))
    {
        for (const auto& entry : popJson["obsoletes"])
        {
            config.obsoletes.push_back(entry.get<std::string>());
        }
    }

    return config;
}

} // namespace ac
