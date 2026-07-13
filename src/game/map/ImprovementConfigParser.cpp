#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/units/MovementConstants.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/Rational.h"
#include "game/effects/BonusEffectParser.h"

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
    config.ownedByTerritory = improvementJson.value("owned_by_territory", false);
    config.frequency = improvementJson.value("frequency", 0);
    config.spritePath = improvementJson.value("sprite_path", "");
    config.excludes = ConfigFields::ParseStringArray(improvementJson, "excludes");
    if (improvementJson.contains("move_cost"))
    {
        config.moveCost = Rational_t::ParseJson(improvementJson.at("move_cost"));
    }
    else
    {
        config.moveCost = MovementConstants_t{}.defaultMoveCost;
    }
    if (improvementJson.contains("move_cost_override"))
    {
        config.moveCostOverride = Rational_t::ParseJson(improvementJson.at("move_cost_override"));
    }
    config.effects = BonusEffectParser::ParseEffects(improvementJson, EffectSourceKind_t::Improvement, config.id);

    return config;
}

} // namespace ac
