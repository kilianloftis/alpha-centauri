#include "game/social-engineering/SocialPolicyConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/effects/BonusEffectParser.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

SocialPolicyConfigParser::SocialPolicyConfigParser()
{
}

std::vector<SocialPolicyConfig> SocialPolicyConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<SocialPolicyConfig>(
        configPath, "social policy",
        [this](const nlohmann::json& rJson) { return ParsePolicyConfig(rJson); });
}

SocialPolicyConfig SocialPolicyConfigParser::ParsePolicyConfig(const nlohmann::json& policyJson)
{
    SocialPolicyConfig config;
    config.id = ConfigFields::ParseId(policyJson);
    config.name = ConfigFields::ParseName(policyJson, config.id);
    config.category = ParseCategory(policyJson.at("category"));
    config.requiredTech = ConfigFields::ParseRequiredTech(policyJson);
    config.effects = BonusEffectParser::ParseEffects(policyJson, EffectSourceKind::SocialPolicy, config.id);

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

} // namespace ac
