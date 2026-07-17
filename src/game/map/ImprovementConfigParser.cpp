#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/units/MovementConstants.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/Rational.h"
#include "game/effects/BonusEffectParser.h"

#include <stdexcept>
#include <string>
#include <string_view>

namespace ac
{

namespace
{

// Ensures the rational is non-negative and lands on a whole number of move fragments.
// Done at parse so MoveCostCalculator::ToFragments_ cannot fail on config-loaded values.
void ValidateMoveCost_(const Rational_t& rCost, std::string_view field, std::string_view id)
{
    auto wrap = [&](const std::string& message) {
        throw std::runtime_error(
            "Improvement '" + std::string(id) + "' field '" + std::string(field) + "': " + message);
    };

    if (rCost.numerator < 0 || rCost.denominator <= 0)
    {
        wrap("move cost must be non-negative");
    }

    try
    {
        (void)rCost.ScaledInt(MovementConstants_t::k_moveFragmentsPerPoint);
    }
    catch (const std::exception& e)
    {
        wrap(e.what());
    }
}

} // namespace

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
        ValidateMoveCost_(*config.moveCost, "move_cost", config.id);
    }
    if (improvementJson.contains("move_cost_override"))
    {
        config.moveCostOverride = Rational_t::ParseJson(improvementJson.at("move_cost_override"));
        ValidateMoveCost_(*config.moveCostOverride, "move_cost_override", config.id);
    }
    config.effects = BonusEffectParser::ParseEffects(improvementJson, EffectSourceKind_t::Improvement, config.id);

    return config;
}

} // namespace ac
