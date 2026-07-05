#include "game/units/UnitComponentConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/effects/BonusEffectParser.h"

namespace ac
{

std::vector<UnitComponentConfig_t> UnitComponentConfigParser::ParseConfig(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadPath<UnitComponentConfig_t>(
        rConfigPath, "unit component",
        [this](const nlohmann::json& rJson) { return ParseComponentConfig(rJson); });
}

UnitComponentConfig_t UnitComponentConfigParser::ParseComponentConfig(const nlohmann::json& rComponentJson)
{
    UnitComponentConfig_t config;
    config.id = ConfigFields::ParseId(rComponentJson);
    config.name = ConfigFields::ParseName(rComponentJson, config.id);
    config.type = rComponentJson.at("type").get<std::string>();
    config.requiredTech = ConfigFields::ParseRequiredTech(rComponentJson);
    config.mineralCost = rComponentJson.value("mineral_cost", 0);
    config.effects = BonusEffectParser::ParseEffects(rComponentJson, EffectSourceKind::UnitComponent, config.id);

    return config;
}

} // namespace ac
