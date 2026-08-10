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
    if (!popJson.contains("display_glyph"))
    {
        throw std::runtime_error("Pop type '" + config.id
                                 + "': missing required field 'display_glyph'");
    }
    const std::string glyph = popJson.at("display_glyph").get<std::string>();
    if (glyph.size() != 1)
    {
        throw std::runtime_error("Pop type '" + config.id
                                 + "': display_glyph must be exactly one character, got '" + glyph
                                 + "'");
    }
    config.displayGlyph = glyph[0];
    config.bIsDefault         = popJson.value("is_default",          false);
    config.bCanWorkTile       = popJson.value("can_work_tile",       false);
    config.bPlayerAssignable  = popJson.value("player_assignable",   false);
    config.requiredTech          = ConfigFields::ParseRequiredTech(popJson);
    config.fallbackPopTypeId     = popJson.value("fallback_pop_type",       "");
    config.obsoletes             = ConfigFields::ParseStringArray(popJson, "obsoletes");
    config.effects               = EffectConfigParser::ParseEffects(popJson, EffectSourceKind_t::PopType, config.id);

    // Riot weight: drones default to 1, everyone else to 0. Explicit override for Super Drone.
    const int defaultRiot = (config.role == PopRole_t::Drone) ? 1 : 0;
    config.riotContribution = popJson.value("riot_contribution", defaultRiot);
    if (config.riotContribution < 0)
    {
        throw std::runtime_error("Pop type '" + config.id
                                 + "': riot_contribution must be >= 0");
    }

    return config;
}

} // namespace ac
