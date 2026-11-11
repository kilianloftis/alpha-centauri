#include "game/buildings/BuildingConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/production/ScrapConfigParser.h"
#include <variant>
#include <vector>

namespace ac
{

namespace
{

BuildingMapOverlay_t ParseMapOverlay_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"land", "sea"});
    BuildingMapOverlay_t overlay;
    overlay.landPath = rReader.StringOr("land", "");
    overlay.seaPath = rReader.StringOr("sea", "");
    if (overlay.landPath.empty() && overlay.seaPath.empty())
    {
        rReader.Fail("needs at least one of 'land' or 'sea'");
    }
    return overlay;
}

ScrapOverride_t ParseBuildingScrap_(const ConfigReader& rReader, const BuildingConfig_t& rConfig)
{
    if (rConfig.bIsSecretProject)
    {
        rReader.FailKey("scrap", "cannot be declared by a secret project");
    }
    return ScrapConfigParser::ParseOverride(rReader.Object("scrap"));
}

void RequireMapOverlayFor_(const ConfigReader& rReader, const char* pKey)
{
    if (rReader.Has(pKey))
    {
        rReader.FailKey(pKey, "requires 'map_overlay'");
    }
}

// map_overlay and the channel, priority and layer that only mean something with it.
void ParseMapOverlayFields_(const ConfigReader& rReader, BuildingConfig_t& rConfig)
{
    if (!rReader.Has("map_overlay"))
    {
        RequireMapOverlayFor_(rReader, "map_overlay_channel");
        RequireMapOverlayFor_(rReader, "map_overlay_priority");
        RequireMapOverlayFor_(rReader, "map_overlay_layer");
        return;
    }
    rConfig.mapOverlay = ParseMapOverlay_(rReader.Object("map_overlay"));
    rConfig.mapOverlayChannel = rReader.StringOr("map_overlay_channel", "");
    rConfig.mapOverlayPriority = rReader.IntOr("map_overlay_priority", 0);
    if (rReader.Has("map_overlay_layer"))
    {
        rConfig.mapOverlayLayer = rReader.Int("map_overlay_layer");
    }
}

std::vector<EffectConfig_t> ParseDestroyedEffects_(const ConfigReader& rReader,
                                                   const BuildingConfig_t& rConfig)
{
    if (!rReader.Has("destroyed_effects"))
    {
        return {};
    }
    if (!rConfig.bIsSecretProject)
    {
        rReader.FailKey("destroyed_effects", "requires secret_project");
    }
    // ParseEffects expects a container with an "effects" array; wrap the field.
    nlohmann::json wrapper = nlohmann::json::object();
    wrapper["effects"] = rReader.Json().at("destroyed_effects");
    std::vector<EffectConfig_t> effects = EffectConfigParser::ParseEffects(
        wrapper, EffectSourceKind_t::Building, rConfig.id);
    for (const EffectConfig_t& rEffect : effects)
    {
        if (rEffect.scope != EffectScope_t::WorldGlobal)
        {
            rReader.FailKey("destroyed_effects", "entries must use scope WorldGlobal");
        }
    }
    return effects;
}

BuildingConfig_t ParseBuilding_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({
        "id", "name", "category", "mineral_cost", "upkeep", "required_tech",
        "allow_multiple", "secret_project", "orbital", "effects", "destroyed_effects",
        "on_complete_effects", "on_unit_produced_effects", "scrap", "icon", "map_overlay",
        "map_overlay_channel", "map_overlay_priority", "map_overlay_layer",
    });

    BuildingConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    // Optional: nothing reads category yet. Tighten to required when something does.
    if (rReader.Has("category"))
    {
        config.category = ParseGameCategory(rReader.String("category"));
    }
    config.upkeep = rReader.IntOr("upkeep", 0);
    config.allowMultiple = rReader.BoolOr("allow_multiple", false);
    config.bIsSecretProject = rReader.BoolOr("secret_project", false);
    config.orbital = rReader.BoolOr("orbital", false);
    config.requiredTech = rReader.StringOr("required_tech", "");
    config.effects =
        EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::Building, config.id);
    config.destroyedEffects = ParseDestroyedEffects_(rReader, config);
    config.onCompleteEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_complete_effects", config.id);
    config.onUnitProducedEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_unit_produced_effects", config.id);
    config.mineralCost = rReader.IntOr("mineral_cost", 0);
    config.icon = rReader.StringOr("icon", "");
    if (rReader.Has("scrap"))
    {
        config.scrap = ParseBuildingScrap_(rReader, config);
    }
    ParseMapOverlayFields_(rReader, config);
    return config;
}

} // namespace

std::vector<BuildingConfig_t> BuildingConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadPath<BuildingConfig_t>(configPath, "building", ParseBuilding_);
}

} // namespace ac
