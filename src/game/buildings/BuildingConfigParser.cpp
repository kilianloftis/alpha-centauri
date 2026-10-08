#include "game/buildings/BuildingConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/faction/base/production/ScrapConfigParser.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <variant>
#include <vector>

namespace ac
{

BuildingConfigParser::BuildingConfigParser()
{
}

std::vector<BuildingConfig_t> BuildingConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadPath<BuildingConfig_t>(
        configPath, "building",
        [this](const nlohmann::json& rJson) { return ParseBuildingConfig_(rJson); });
}

namespace
{

// Type-checks a present key and names the building in the failure, which nlohmann's own
// type_error does not. An absent or null key takes defaultValue.
template<typename T>
T ParseTyped_(const nlohmann::json& rJson, const char* pKey, const BuildingId_t& rBuildingId,
              T defaultValue, bool (nlohmann::json::*pIsRightType)() const,
              const char* pExpected)
{
    if (!rJson.contains(pKey) || rJson.at(pKey).is_null())
    {
        return defaultValue;
    }
    const nlohmann::json& rValue = rJson.at(pKey);
    if (!(rValue.*pIsRightType)())
    {
        throw std::runtime_error("Building '" + rBuildingId + "': '" + std::string(pKey)
                                 + "' must be " + pExpected);
    }
    return rValue.get<T>();
}

const std::vector<std::string>& KnownBuildingKeys_()
{
    static const std::vector<std::string> keys = {
        "id", "name", "category", "mineral_cost", "upkeep", "required_tech",
        "allow_multiple", "secret_project", "orbital", "effects", "on_complete_effects",
        "on_unit_produced_effects", "scrap", "icon", "map_overlay", "map_overlay_channel",
        "map_overlay_priority", "map_overlay_layer",
    };
    return keys;
}

BuildingMapOverlay_t ParseMapOverlay_(const nlohmann::json& rJson, const BuildingId_t& rId)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error("Building '" + rId + "': 'map_overlay' must be an object");
    }
    for (const auto& [rKey, rUnused] : rJson.items())
    {
        if (rKey != "land" && rKey != "sea")
        {
            throw std::runtime_error("Building '" + rId + "': 'map_overlay' unknown key '" + rKey
                                     + "'");
        }
    }
    BuildingMapOverlay_t overlay;
    if (rJson.contains("land"))
    {
        if (!rJson.at("land").is_string())
        {
            throw std::runtime_error("Building '" + rId + "': 'map_overlay.land' must be a string");
        }
        overlay.landPath = rJson.at("land").get<std::string>();
    }
    if (rJson.contains("sea"))
    {
        if (!rJson.at("sea").is_string())
        {
            throw std::runtime_error("Building '" + rId + "': 'map_overlay.sea' must be a string");
        }
        overlay.seaPath = rJson.at("sea").get<std::string>();
    }
    if (overlay.landPath.empty() && overlay.seaPath.empty())
    {
        throw std::runtime_error("Building '" + rId
                                 + "': 'map_overlay' needs at least one of 'land' or 'sea'");
    }
    return overlay;
}

} // namespace

BuildingConfig_t BuildingConfigParser::ParseBuildingConfig_(const nlohmann::json& buildingJson)
{
    BuildingConfig_t config;
    config.id = ConfigFields::ParseId(buildingJson);
    config.name = ConfigFields::ParseName(buildingJson, config.id);

    for (const auto& [rKey, rUnused] : buildingJson.items())
    {
        const std::vector<std::string>& rKnown = KnownBuildingKeys_();
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error("Building '" + config.id + "': unknown key '" + rKey + "'");
        }
    }

    // Optional: nothing reads category yet. Tighten to required when something does.
    if (buildingJson.contains("category") && !buildingJson.at("category").is_null())
    {
        config.category = ParseGameCategoryField(buildingJson);
    }
    config.upkeep = ParseTyped_<int>(buildingJson, "upkeep", config.id, 0,
                                    &nlohmann::json::is_number_integer, "an integer");
    config.allowMultiple = ParseTyped_<bool>(buildingJson, "allow_multiple", config.id, false,
                                             &nlohmann::json::is_boolean, "a boolean");
    config.bIsSecretProject = ParseTyped_<bool>(buildingJson, "secret_project", config.id, false,
                                                &nlohmann::json::is_boolean, "a boolean");
    config.orbital = ParseTyped_<bool>(buildingJson, "orbital", config.id, false,
                                       &nlohmann::json::is_boolean, "a boolean");
    if (buildingJson.contains("required_techs"))
    {
        throw std::runtime_error(
            "Building '" + config.id + "': 'required_techs' is no longer supported; "
            "use singular 'required_tech' (omit or \"\" = always available)");
    }
    config.requiredTech = ConfigFields::ParseRequiredTech(buildingJson);
    config.effects = EffectConfigParser::ParseEffects(buildingJson, EffectSourceKind_t::Building, config.id);
    config.onCompleteEffects = TriggeredEffectParser::ParseTriggeredEffects(
        buildingJson, "on_complete_effects", config.id);
    config.onUnitProducedEffects = TriggeredEffectParser::ParseTriggeredEffects(
        buildingJson, "on_unit_produced_effects", config.id);
    config.mineralCost = ParseTyped_<int>(buildingJson, "mineral_cost", config.id, 0,
                                          &nlohmann::json::is_number_integer, "an integer");
    config.icon = ParseTyped_<std::string>(buildingJson, "icon", config.id, std::string{},
                                           &nlohmann::json::is_string, "a string");
    if (buildingJson.contains("scrap"))
    {
        if (config.bIsSecretProject)
        {
            throw std::runtime_error("Building '" + config.id
                                     + "': secret projects cannot declare scrap");
        }
        config.scrap = ScrapConfigParser::ParseOverride(buildingJson.at("scrap"),
                                                "Building '" + config.id + "' scrap");
    }
    if (buildingJson.contains("map_overlay"))
    {
        config.mapOverlay = ParseMapOverlay_(buildingJson.at("map_overlay"), config.id);
    }
    config.mapOverlayChannel =
        ParseTyped_<std::string>(buildingJson, "map_overlay_channel", config.id, std::string{},
                                 &nlohmann::json::is_string, "a string");
    config.mapOverlayPriority =
        ParseTyped_<int>(buildingJson, "map_overlay_priority", config.id, 0,
                         &nlohmann::json::is_number_integer, "an integer");
    if (buildingJson.contains("map_overlay_layer") && !buildingJson.at("map_overlay_layer").is_null())
    {
        if (!buildingJson.at("map_overlay_layer").is_number_integer())
        {
            throw std::runtime_error("Building '" + config.id
                                     + "': 'map_overlay_layer' must be an integer");
        }
        config.mapOverlayLayer = buildingJson.at("map_overlay_layer").get<int>();
    }
    if (!config.mapOverlayChannel.empty() && !config.mapOverlay)
    {
        throw std::runtime_error("Building '" + config.id
                                 + "': 'map_overlay_channel' requires 'map_overlay'");
    }
    if (buildingJson.contains("map_overlay_priority") && !config.mapOverlay)
    {
        throw std::runtime_error("Building '" + config.id
                                 + "': 'map_overlay_priority' requires 'map_overlay'");
    }
    if (config.mapOverlayLayer && !config.mapOverlay)
    {
        throw std::runtime_error("Building '" + config.id
                                 + "': 'map_overlay_layer' requires 'map_overlay'");
    }

    return config;
}

} // namespace ac
