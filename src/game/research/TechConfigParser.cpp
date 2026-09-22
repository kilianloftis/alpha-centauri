#include "game/research/TechConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

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
    TechConfig_t config;
    config.id = ConfigFields::ParseId(techJson);
    config.name = ConfigFields::ParseName(techJson, config.id);
    config.category = ParseGameCategoryField(techJson);
    config.prerequisites = ConfigFields::ParseStringArray(techJson, "prerequisites");
    config.effects = EffectConfigParser::ParseEffects(
        techJson, EffectSourceKind_t::Tech, config.id);
    config.onDiscoverEffects = TriggeredEffectParser::ParseTriggeredEffects(
        techJson, "on_discover_effects", config.id);

    return config;
}

} // namespace ac
