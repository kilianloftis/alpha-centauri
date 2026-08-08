#include "game/social-engineering/SocialPolicyConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

std::vector<SocialPolicyConfig_t> SocialPolicyConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<SocialPolicyConfig_t>(
        configPath, "social policy",
        [this](const nlohmann::json& rJson) { return ParsePolicyConfig_(rJson); });
}

SocialPolicyConfig_t SocialPolicyConfigParser::ParsePolicyConfig_(const nlohmann::json& policyJson)
{
    SocialPolicyConfig_t config;
    config.id = ConfigFields::ParseId(policyJson);
    config.name = ConfigFields::ParseName(policyJson, config.id);
    config.category = ParseCategory_(policyJson.at("category"));
    config.requiredTech = ConfigFields::ParseRequiredTech(policyJson);
    config.bDefault = policyJson.value("default", false);
    config.effects = EffectConfigParser::ParseEffects(policyJson, EffectSourceKind_t::SocialPolicy, config.id);

    return config;
}

SocialCategory_t SocialPolicyConfigParser::ParseCategory_(const std::string& category)
{
    if (category == "politics")       return SocialCategory_t::Politics;
    if (category == "economics")      return SocialCategory_t::Economics;
    if (category == "values")         return SocialCategory_t::Values;
    if (category == "future_society") return SocialCategory_t::FutureSociety;

    throw std::runtime_error("Unknown social policy category: '" + category + "'");
}

} // namespace ac
