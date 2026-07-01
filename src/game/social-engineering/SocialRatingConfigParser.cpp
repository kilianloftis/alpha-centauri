#include "game/social-engineering/SocialRatingConfigParser.h"
#include "lib/effects/BonusEffectParser.h"
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

std::vector<SocialRatingConfig> SocialRatingConfigParser::ParseConfig(const std::string& rConfigPath)
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

    std::vector<SocialRatingConfig> configs;
    for (const auto& rRatingJson : configJson)
    {
        configs.push_back(ParseRatingConfig(rRatingJson));
    }

    std::cout << "Loaded " << configs.size() << " social rating configurations\n";
    return configs;
}

SocialRatingConfig SocialRatingConfigParser::ParseRatingConfig(const nlohmann::json& rRatingJson)
{
    SocialRatingConfig config;
    config.id = rRatingJson["id"];
    config.rating = BonusEffectParser::ParseSocialRatingId(config.id);

    const auto& rLevels = rRatingJson["levels"];
    for (auto it = rLevels.begin(); it != rLevels.end(); ++it)
    {
        const int level = std::stoi(it.key());
        std::vector<EffectConfig_t> effects;
        for (const auto& rEffectJson : it.value())
        {
            effects.push_back(BonusEffectParser::ParseEffectConfig(rEffectJson));
        }
        config.levelEffects[level] = std::move(effects);
    }

    return config;
}

} // namespace ac
