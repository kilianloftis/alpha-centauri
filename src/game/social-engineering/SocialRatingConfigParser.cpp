#include "game/social-engineering/SocialRatingConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

std::vector<SocialRatingConfig_t> SocialRatingConfigParser::ParseConfig(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadFile<SocialRatingConfig_t>(
        rConfigPath, "social rating",
        [this](const nlohmann::json& rJson) { return ParseRatingConfig_(rJson); });
}

SocialRatingConfig_t SocialRatingConfigParser::ParseRatingConfig_(const nlohmann::json& rRatingJson)
{
    SocialRatingConfig_t config;
    config.id = ConfigFields::ParseId(rRatingJson);
    config.rating = ParseSocialRatingId(config.id);

    if (!rRatingJson.contains("levels"))
    {
        throw std::runtime_error("Social rating '" + config.id
                                 + "': missing required field 'levels'");
    }
    const auto& rLevels = rRatingJson.at("levels");
    if (!rLevels.is_object())
    {
        throw std::runtime_error("Social rating '" + config.id
                                 + "': 'levels' must be an object keyed by level");
    }

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
