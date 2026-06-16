#include "game/social-engineering/SocialPolicyConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;

namespace ac
{

SocialPolicyConfigParser::SocialPolicyConfigParser()
{
}

std::vector<SocialPolicyConfig> SocialPolicyConfigParser::ParseConfig(const std::string& configPath)
{
    std::cout << "Loading social policy configuration from: " << configPath << "\n";

    std::vector<SocialPolicyConfig> configs;

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
            std::cout << "Error: Expected array of social policies in config\n";
            return configs;
        }

        for (const auto& policyJson : configJson)
        {
            configs.push_back(ParsePolicyConfig(policyJson));
        }

        std::cout << "Loaded " << configs.size() << " social policy configurations\n";
        return configs;
    }
    catch (const std::exception& e)
    {
        std::cout << "Error loading social policy config: " << e.what() << "\n";
        return configs;
    }
}

SocialPolicyConfig SocialPolicyConfigParser::ParsePolicyConfig(const nlohmann::json& policyJson)
{
    SocialPolicyConfig config;
    config.id = policyJson["id"];
    config.name = policyJson.value("name", config.id);
    config.category = ParseCategory(policyJson["category"]);
    config.prerequisiteTech = policyJson.value("prerequisite_tech", "");

    if (policyJson.contains("effects"))
    {
        config.effects = ParseEffects(policyJson["effects"]);
    }

    return config;
}

SocialCategory SocialPolicyConfigParser::ParseCategory(const std::string& category)
{
    if (category == "politics")       return SocialCategory::Politics;
    if (category == "economics")      return SocialCategory::Economics;
    if (category == "values")         return SocialCategory::Values;
    if (category == "future_society") return SocialCategory::FutureSociety;

    throw std::runtime_error("Unknown social policy category: '" + category + "'");
}

SocialScores SocialPolicyConfigParser::ParseEffects(const nlohmann::json& effectsJson)
{
    SocialScores scores;
    scores.economy    = effectsJson.value("economy",    0);
    scores.efficiency = effectsJson.value("efficiency", 0);
    scores.support    = effectsJson.value("support",    0);
    scores.police     = effectsJson.value("police",     0);
    scores.morale     = effectsJson.value("morale",     0);
    scores.growth     = effectsJson.value("growth",     0);
    scores.planet     = effectsJson.value("planet",     0);
    scores.research   = effectsJson.value("research",   0);
    scores.industry   = effectsJson.value("industry",   0);
    scores.probe      = effectsJson.value("probe",      0);
    return scores;
}

} // namespace ac
