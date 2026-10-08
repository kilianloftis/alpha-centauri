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

std::vector<TechConfig_t> TechConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<TechConfig_t>(
        configPath, "tech",
        [this](const nlohmann::json& rJson) { return ParseTechConfig_(rJson); });
}

TechConfig_t TechConfigParser::ParseTechConfig_(const nlohmann::json& techJson)
{
    static const std::vector<std::string> k_KnownKeys = {
        "id", "name", "category", "prerequisites", "icon", "effects", "on_discover_effects",
    };

    TechConfig_t config;
    config.id = ConfigFields::ParseId(techJson);
    config.name = ConfigFields::ParseName(techJson, config.id);

    for (const auto& [rKey, rUnused] : techJson.items())
    {
        if (std::find(k_KnownKeys.begin(), k_KnownKeys.end(), rKey) == k_KnownKeys.end())
        {
            throw std::runtime_error("Tech '" + config.id + "': unknown key '" + rKey + "'");
        }
    }

    config.category = ParseGameCategoryField(techJson);
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
