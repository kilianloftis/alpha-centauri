#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/effects/BonusEffectParser.h"

namespace ac
{

bool CanBuildImprovement(const Tile& rTile, const ImprovementConfig_t& rCandidate)
{
    for (const std::string& excludedId : rCandidate.excludes)
    {
        if (rTile.HasFeature(excludedId))
            return false;
    }
    return true;
}

std::vector<ImprovementConfig_t> ImprovementConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadPath<ImprovementConfig_t>(
        configPath, "improvement",
        [this](const nlohmann::json& rJson) { return ParseImprovementConfig(rJson); });
}

ImprovementConfig_t ImprovementConfigParser::ParseImprovementConfig(const nlohmann::json& improvementJson)
{
    ImprovementConfig_t config;
    config.id = ConfigFields::ParseId(improvementJson);
    config.name = ConfigFields::ParseName(improvementJson, config.id);
    config.description = improvementJson.value("description", "");
    config.mineralCost = improvementJson.value("mineral_cost", 0);
    config.requiredTech = ConfigFields::ParseRequiredTech(improvementJson);
    config.radius = improvementJson.value("radius", 0);
    config.frequency = improvementJson.value("frequency", 0);
    config.spritePath = improvementJson.value("sprite_path", "");
    config.excludes = ConfigFields::ParseStringArray(improvementJson, "excludes");
    config.effects = BonusEffectParser::ParseEffects(improvementJson);

    return config;
}

} // namespace ac
