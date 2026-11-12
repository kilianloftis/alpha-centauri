#include "game/social-engineering/SocialPolicyConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"

namespace ac
{

namespace
{

SocialCategory_t ParseCategory_(const ConfigReader& rReader)
{
    const std::string category = rReader.String("category");
    if (category == "politics")       return SocialCategory_t::Politics;
    if (category == "economics")      return SocialCategory_t::Economics;
    if (category == "values")         return SocialCategory_t::Values;
    if (category == "future_society") return SocialCategory_t::FutureSociety;
    rReader.FailKey("category", "is not a known social policy category: '" + category + "'");
}

UpheavalCostConfig_t ParseUpheaval_(const ConfigReader& rReader)
{
    UpheavalCostConfig_t config;
    config.costFormula = rReader.Formula("cost_formula", {"changed_categories"});
    return config;
}

SocialPolicyConfig_t ParsePolicy_(const ConfigReader& rReader)
{
    SocialPolicyConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.category = ParseCategory_(rReader);
    config.requiredTech = rReader.StringOr("required_tech", "");
    config.bDefault = rReader.BoolOr("default", false);
    config.effects = EffectConfigParser::ParseEffects(
        rReader.Json(), EffectSourceKind_t::SocialPolicy, config.id);
    return config;
}

SocialPoliciesConfig_t ParseSocialPolicies_(const ConfigReader& rReader)
{
    SocialPoliciesConfig_t config;
    config.upheaval = ParseUpheaval_(rReader.Object("upheaval"));
    for (const ConfigReader& rPolicy : rReader.Objects("policies"))
    {
        config.policies.push_back(ParsePolicy_(rPolicy));
    }
    return config;
}

} // namespace

SocialPoliciesConfig_t SocialPolicyConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<SocialPoliciesConfig_t>(
        configPath, "social policies", ParseSocialPolicies_);
}

} // namespace ac
