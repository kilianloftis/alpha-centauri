#include "game/social-engineering/SocialPolicyConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

SocialPoliciesConfig_t SocialPolicyConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<SocialPoliciesConfig_t>(
        configPath, "social policies",
        [this](const nlohmann::json& rJson) {
            if (!rJson.contains("upheaval") || !rJson.at("upheaval").is_object())
            {
                throw std::runtime_error("social_policies: requires object 'upheaval'");
            }
            if (!rJson.contains("policies") || !rJson.at("policies").is_array())
            {
                throw std::runtime_error("social_policies: requires array 'policies'");
            }

            SocialPoliciesConfig_t config;
            config.upheaval = ParseUpheaval_(rJson.at("upheaval"));
            for (const nlohmann::json& rPolicyJson : rJson.at("policies"))
            {
                config.policies.push_back(ParsePolicyConfig_(rPolicyJson));
            }
            return config;
        });
}

UpheavalCostConfig_t SocialPolicyConfigParser::ParseUpheaval_(
    const nlohmann::json& rUpheavalJson) const
{
    if (!rUpheavalJson.contains("cost_formula") || !rUpheavalJson.at("cost_formula").is_string())
    {
        throw std::runtime_error("social_policies upheaval: requires string 'cost_formula'");
    }
    UpheavalCostConfig_t config;
    config.costFormula = rUpheavalJson.at("cost_formula").get<std::string>();
    if (config.costFormula.empty())
    {
        throw std::runtime_error("social_policies upheaval: cost_formula must not be empty");
    }
    return config;
}

SocialPolicyConfig_t SocialPolicyConfigParser::ParsePolicyConfig_(const nlohmann::json& policyJson)
{
    SocialPolicyConfig_t config;
    config.id = ConfigFields::ParseId(policyJson);
    config.name = ConfigFields::ParseName(policyJson, config.id, "name");
    config.category = ParseCategory_(policyJson.at("category"));
    config.requiredTech = ConfigFields::ParseRequiredTech(policyJson);
    config.bDefault = policyJson.value("default", false);
    config.effects = EffectConfigParser::ParseEffects(policyJson, EffectSourceKind_t::SocialPolicy,
                                                      config.id);

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
