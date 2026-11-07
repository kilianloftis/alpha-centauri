#include "game/buildings/BuildingConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/production/ScrapConfigParser.h"
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
        "allow_multiple", "secret_project", "orbital", "effects", "destroyed_effects",
        "on_complete_effects", "on_unit_produced_effects", "scrap", "icon", "map_overlay",
        "map_overlay_channel", "map_overlay_priority", "map_overlay_layer",
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

void RejectUnknownBuildingKeys_(const nlohmann::json& rJson, const BuildingId_t& rId)
{
    const std::vector<std::string>& rKnown = KnownBuildingKeys_();
    for (const auto& [rKey, rUnused] : rJson.items())
    {
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error("Building '" + rId + "': unknown key '" + rKey + "'");
        }
    }
}

ScrapOverride_t ParseBuildingScrap_(const nlohmann::json& rScrap, const BuildingConfig_t& rConfig)
{
    if (rConfig.bIsSecretProject)
    {
        throw std::runtime_error("Building '" + rConfig.id
                                 + "': secret projects cannot declare scrap");
    }
    return ScrapConfigParser::ParseOverride(rScrap, "Building '" + rConfig.id + "' scrap");
}

// map_overlay and the channel, priority and layer that only mean something with it.
void ParseMapOverlayFields_(const nlohmann::json& rJson, BuildingConfig_t& rConfig)
{
    if (rJson.contains("map_overlay"))
    {
        rConfig.mapOverlay = ParseMapOverlay_(rJson.at("map_overlay"), rConfig.id);
    }
    rConfig.mapOverlayChannel =
        ParseTyped_<std::string>(rJson, "map_overlay_channel", rConfig.id, std::string{},
                                 &nlohmann::json::is_string, "a string");
    rConfig.mapOverlayPriority =
        ParseTyped_<int>(rJson, "map_overlay_priority", rConfig.id, 0,
                         &nlohmann::json::is_number_integer, "an integer");
    if (rJson.contains("map_overlay_layer") && !rJson.at("map_overlay_layer").is_null())
    {
        if (!rJson.at("map_overlay_layer").is_number_integer())
        {
            throw std::runtime_error("Building '" + rConfig.id
                                     + "': 'map_overlay_layer' must be an integer");
        }
        rConfig.mapOverlayLayer = rJson.at("map_overlay_layer").get<int>();
    }
    if (rConfig.mapOverlay)
    {
        return;
    }
    if (!rConfig.mapOverlayChannel.empty())
    {
        throw std::runtime_error("Building '" + rConfig.id
                                 + "': 'map_overlay_channel' requires 'map_overlay'");
    }
    if (rJson.contains("map_overlay_priority"))
    {
        throw std::runtime_error("Building '" + rConfig.id
                                 + "': 'map_overlay_priority' requires 'map_overlay'");
    }
    if (rConfig.mapOverlayLayer)
    {
        throw std::runtime_error("Building '" + rConfig.id
                                 + "': 'map_overlay_layer' requires 'map_overlay'");
    }
}

std::vector<EffectConfig_t> ParseDestroyedEffects_(const nlohmann::json& rBuildingJson,
                                                   const BuildingConfig_t& rConfig)
{
    if (!rBuildingJson.contains("destroyed_effects"))
    {
        return {};
    }
    if (!rConfig.bIsSecretProject)
    {
        throw std::runtime_error("Building '" + rConfig.id
                                 + "': destroyed_effects requires secret_project");
    }
    // ParseEffects expects a container with an "effects" array; wrap the field.
    nlohmann::json wrapper = nlohmann::json::object();
    wrapper["effects"] = rBuildingJson.at("destroyed_effects");
    std::vector<EffectConfig_t> effects = EffectConfigParser::ParseEffects(
        wrapper, EffectSourceKind_t::Building, rConfig.id);
    for (const EffectConfig_t& rEffect : effects)
    {
        if (rEffect.scope != EffectScope_t::WorldGlobal)
        {
            throw std::runtime_error(
                "Building '" + rConfig.id
                + "': destroyed_effects entries must use scope WorldGlobal");
        }
    }
    return effects;
}

} // namespace

BuildingConfig_t BuildingConfigParser::ParseBuildingConfig_(const nlohmann::json& buildingJson)
{
    BuildingConfig_t config;
    config.id = ConfigFields::ParseId(buildingJson);
    config.name = ConfigFields::ParseName(buildingJson, config.id, "name");

    RejectUnknownBuildingKeys_(buildingJson, config.id);
    // Optional: nothing reads category yet. Tighten to required when something does.
    if (buildingJson.contains("category") && !buildingJson.at("category").is_null())
    {
        config.category = ParseGameCategoryField(buildingJson, "category");
    }
    config.upkeep = ParseTyped_<int>(buildingJson, "upkeep", config.id, 0,
                                    &nlohmann::json::is_number_integer, "an integer");
    config.allowMultiple = ParseTyped_<bool>(buildingJson, "allow_multiple", config.id, false,
                                             &nlohmann::json::is_boolean, "a boolean");
    config.bIsSecretProject = ParseTyped_<bool>(buildingJson, "secret_project", config.id, false,
                                                &nlohmann::json::is_boolean, "a boolean");
    config.orbital = ParseTyped_<bool>(buildingJson, "orbital", config.id, false,
                                       &nlohmann::json::is_boolean, "a boolean");
    config.requiredTech = ConfigFields::ParseRequiredTech(buildingJson);
    config.effects = EffectConfigParser::ParseEffects(buildingJson, EffectSourceKind_t::Building, config.id);
    config.destroyedEffects = ParseDestroyedEffects_(buildingJson, config);
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
        config.scrap = ParseBuildingScrap_(buildingJson.at("scrap"), config);
    }
    ParseMapOverlayFields_(buildingJson, config);

    return config;
}

} // namespace ac
