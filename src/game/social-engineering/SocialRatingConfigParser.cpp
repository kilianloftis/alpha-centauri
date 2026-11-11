#include "game/social-engineering/SocialRatingConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <string>

namespace ac
{

namespace
{

SocialRatingConfig_t ParseRating_(const ConfigReader& rReader)
{
    SocialRatingConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.rating = ParseSocialRatingId(config.id);

    const ConfigReader levels = rReader.Object("levels");
    for (const auto& [rLevelKey, rEffectsJson] : levels.Json().items())
    {
        const int level = std::stoi(rLevelKey);
        std::vector<EffectConfig_t> effects;
        for (const auto& rEffectJson : rEffectsJson)
        {
            EffectConfig_t effect = EffectConfigParser::ParseEffectConfig(rEffectJson);
            EffectConfigParser::ValidateEffectForSource(
                effect, EffectSourceKind_t::SocialRating, config.id + " level " + rLevelKey);
            effects.push_back(std::move(effect));
        }
        config.levelEffects[level] = std::move(effects);
    }
    return config;
}

} // namespace

std::vector<SocialRatingConfig_t> SocialRatingConfigParser::ParseConfig(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadFile<SocialRatingConfig_t>(rConfigPath, "social rating",
                                                            ParseRating_);
}

} // namespace ac
