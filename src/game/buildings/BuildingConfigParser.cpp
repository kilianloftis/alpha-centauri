#include "game/buildings/BuildingConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/BonusEffectParser.h"
#include <nlohmann/json.hpp>

namespace ac
{

BuildingConfigParser::BuildingConfigParser()
{
}

std::vector<BuildingConfig_t> BuildingConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadPath<BuildingConfig_t>(
        configPath, "building",
        [this](const nlohmann::json& rJson) { return ParseBuildingConfig(rJson); });
}

BuildingConfig_t BuildingConfigParser::ParseBuildingConfig(const nlohmann::json& buildingJson)
{
    BuildingConfig_t config;
    config.id = ConfigFields::ParseId(buildingJson);
    config.name = ConfigFields::ParseName(buildingJson, config.id);
    config.category = ParseGameCategoryField(buildingJson);
    config.mineralCost = buildingJson.value("mineral_cost", 0);
    config.allowMultiple = buildingJson.value("allow_multiple", false);
    config.bIsSecretProject = buildingJson.value("secret_project", false);
    config.requiredTechs = ConfigFields::ParseStringArray(buildingJson, "required_techs");
    config.effects = BonusEffectParser::ParseEffects(buildingJson, EffectSourceKind::Building, config.id);

    return config;
}

} // namespace ac
