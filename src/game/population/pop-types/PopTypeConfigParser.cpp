#include "game/population/pop-types/PopTypeConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/EnumNames.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

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
    if (!popJson.contains("role"))
    {
        throw std::runtime_error("Pop type '" + config.id + "': missing required field 'role'");
    }
    config.role =
        EnumFromName<PopRole_t>(popJson.at("role").get<std::string>(), "pop type role");
    config.bIsDefault         = popJson.value("is_default",          false);
    config.bCanWorkTile       = popJson.value("can_work_tile",       false);
    config.bPlayerAssignable  = popJson.value("player_assignable",   false);
    config.requiredTech          = ConfigFields::ParseRequiredTech(popJson);
    config.fallbackPopTypeId     = popJson.value("fallback_pop_type",       "");
    config.obsoletes             = ConfigFields::ParseStringArray(popJson, "obsoletes");
    config.effects               = EffectConfigParser::ParseEffects(popJson, EffectSourceKind_t::PopType, config.id);

    return config;
}

} // namespace ac
