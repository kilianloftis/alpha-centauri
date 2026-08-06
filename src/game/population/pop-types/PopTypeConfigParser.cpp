#include "game/population/pop-types/PopTypeConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include <nlohmann/json.hpp>

namespace ac
{

PopTypeConfigParser::PopTypeConfigParser()
{
}

std::vector<PopTypeConfig_t> PopTypeConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<PopTypeConfig_t>(
        configPath, "pop type",
        [this](const nlohmann::json& rJson) { return ParsePopTypeConfig(rJson); });
}

PopTypeConfig_t PopTypeConfigParser::ParsePopTypeConfig(const nlohmann::json& popJson)
{
    PopTypeConfig_t config;
    config.id = ConfigFields::ParseId(popJson);
    config.name = ConfigFields::ParseName(popJson, config.id);
    config.bIsDefault         = popJson.value("is_default",          false);
    config.bCanWorkTile       = popJson.value("can_work_tile",       false);
    config.bPlayerAssignable  = popJson.value("player_assignable",   false);
    config.riotContribution      = popJson.value("riot_contribution",       0);
    config.goldenAgeContribution = popJson.value("golden_age_contribution", 0);
    config.requiredTech          = ConfigFields::ParseRequiredTech(popJson);
    config.fallbackPopTypeId     = popJson.value("fallback_pop_type",       "");
    config.obsoletes             = ConfigFields::ParseStringArray(popJson, "obsoletes");
    config.effects               = EffectConfigParser::ParseEffects(popJson, EffectSourceKind_t::PopType, config.id);

    return config;
}

} // namespace ac
