#include "game/research/TechConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <vector>

namespace ac
{

namespace
{

TechAiWeights_t ParseAiWeights_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"power", "tech", "wealth", "growth"});
    TechAiWeights_t weights;
    weights.power = rReader.IntAtLeast("power", 0);
    weights.tech = rReader.IntAtLeast("tech", 0);
    weights.wealth = rReader.IntAtLeast("wealth", 0);
    weights.growth = rReader.IntAtLeast("growth", 0);
    return weights;
}

TechConfig_t ParseTech_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({
        "id", "name", "category", "ai_weights", "prerequisites", "icon", "effects",
        "on_discover_effects",
    });

    TechConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.category = ParseGameCategory(rReader.String("category"));
    config.aiWeights = ParseAiWeights_(rReader.Object("ai_weights"));
    config.prerequisites = rReader.StringArray("prerequisites");
    config.icon = rReader.StringOr("icon", config.icon);
    config.effects =
        EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::Tech, config.id);
    config.onDiscoverEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_discover_effects", config.id);
    return config;
}

} // namespace

std::vector<TechConfig_t> TechConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<TechConfig_t>(configPath, "tech", ParseTech_);
}

} // namespace ac
