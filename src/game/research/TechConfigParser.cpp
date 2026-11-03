#include "game/research/TechConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

namespace ac
{

namespace
{

TechAiWeights_t ParseAiWeights_(const nlohmann::json& rTechJson, const std::string& rTechId)
{
    const std::string context = "Tech '" + rTechId + "' ai_weights";
    if (!rTechJson.contains("ai_weights") || !rTechJson.at("ai_weights").is_object())
    {
        throw std::runtime_error(context + " must be an object");
    }
    const nlohmann::json& rWeights = rTechJson.at("ai_weights");
    ConfigFields::RejectUnknownKeys(rWeights, {"power", "tech", "wealth", "growth"}, context);
    TechAiWeights_t weights;
    weights.power = ConfigFields::RequireIntAtLeast(rWeights, "power", 0, context);
    weights.tech = ConfigFields::RequireIntAtLeast(rWeights, "tech", 0, context);
    weights.wealth = ConfigFields::RequireIntAtLeast(rWeights, "wealth", 0, context);
    weights.growth = ConfigFields::RequireIntAtLeast(rWeights, "growth", 0, context);
    return weights;
}

} // namespace

std::vector<TechConfig_t> TechConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<TechConfig_t>(
        configPath, "tech",
        [this](const nlohmann::json& rJson) { return ParseTechConfig_(rJson); });
}

TechConfig_t TechConfigParser::ParseTechConfig_(const nlohmann::json& techJson)
{
    static const std::vector<std::string> k_KnownKeys = {
        "id", "name", "category", "ai_weights", "prerequisites", "icon", "effects",
        "on_discover_effects",
    };

    TechConfig_t config;
    config.id = ConfigFields::ParseId(techJson);
    config.name = ConfigFields::ParseName(techJson, config.id, "name");

    for (const auto& [rKey, rUnused] : techJson.items())
    {
        if (std::find(k_KnownKeys.begin(), k_KnownKeys.end(), rKey) == k_KnownKeys.end())
        {
            throw std::runtime_error("Tech '" + config.id + "': unknown key '" + rKey + "'");
        }
    }

    config.category = ParseGameCategoryField(techJson, "category");
    config.aiWeights = ParseAiWeights_(techJson, config.id);
    config.prerequisites = ConfigFields::ParseStringArray(techJson, "prerequisites");
    if (techJson.contains("icon"))
    {
        if (!techJson.at("icon").is_string())
        {
            throw std::runtime_error("Tech '" + config.id + "': 'icon' must be a string");
        }
        config.icon = techJson.at("icon").get<std::string>();
    }
    config.effects = EffectConfigParser::ParseEffects(
        techJson, EffectSourceKind_t::Tech, config.id);
    config.onDiscoverEffects = TriggeredEffectParser::ParseTriggeredEffects(
        techJson, "on_discover_effects", config.id);

    return config;
}

} // namespace ac
