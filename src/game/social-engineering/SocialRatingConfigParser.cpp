#include "game/social-engineering/SocialRatingConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;

namespace ac
{

SocialRatingConfigParser::SocialRatingConfigParser()
{
}

std::vector<SocialRatingConfig_t> SocialRatingConfigParser::ParseConfig(const std::string& rConfigPath)
{
    std::cout << "Loading social rating configuration from: " << rConfigPath << "\n";

    std::ifstream configFile(rConfigPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + rConfigPath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of social rating configs in '" + rConfigPath + "'");
    }

    std::vector<SocialRatingConfig_t> configs;
    for (const auto& rRatingJson : configJson)
    {
        configs.push_back(ParseRatingConfig(rRatingJson));
    }

    std::cout << "Loaded " << configs.size() << " social rating configurations\n";
    return configs;
}

SocialRatingConfig_t SocialRatingConfigParser::ParseRatingConfig(const nlohmann::json& rRatingJson)
{
    SocialRatingConfig_t config;
    config.id = rRatingJson["id"];
    config.rating = ParseSocialRatingId(config.id);

    const auto& rLevels = rRatingJson["levels"];
    for (auto it = rLevels.begin(); it != rLevels.end(); ++it)
    {
        const int level = std::stoi(it.key());
        std::vector<EffectConfig_t> effects;
        for (const auto& rEffectJson : it.value())
        {
            EffectConfig_t effect = EffectConfigParser::ParseEffectConfig(rEffectJson);
            EffectConfigParser::ValidateScopeForSource(effect.scope, EffectSourceKind_t::SocialRating,
                                                      config.id + " level " + it.key());
            effects.push_back(std::move(effect));
        }
        config.levelEffects[level] = std::move(effects);
    }

    return config;
}

} // namespace ac
