#include "game/units/UnitComponentConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/BonusEffectParser.h"

#include <stdexcept>

namespace ac
{

namespace
{

UnitDomain_t ParseUnitDomain_(const std::string& rDomain)
{
    if (rDomain == "land") return UnitDomain_t::Land;
    if (rDomain == "sea")  return UnitDomain_t::Sea;
    if (rDomain == "air")  return UnitDomain_t::Air;
    throw std::runtime_error("Unknown unit domain '" + rDomain + "' (expected land, sea, or air)");
}

} // namespace

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
    config.effects = BonusEffectParser::ParseEffects(rComponentJson, EffectSourceKind_t::UnitComponent, config.id);

    const bool bHasDomain = rComponentJson.contains("domain");
    if (config.type == "chassis")
    {
        if (!bHasDomain)
        {
            throw std::runtime_error("Chassis component '" + config.id + "' missing required 'domain'");
        }
        config.domain = ParseUnitDomain_(rComponentJson.at("domain").get<std::string>());
    }
    else if (bHasDomain)
    {
        throw std::runtime_error(
            "Component '" + config.id + "' of type '" + config.type
            + "' must not declare 'domain' (chassis only)");
    }

    return config;
}

} // namespace ac
