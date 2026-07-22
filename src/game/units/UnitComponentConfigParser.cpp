#include "game/units/UnitComponentConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/BonusEffectParser.h"

#include <stdexcept>

namespace ac
{

namespace
{

CombatRatingTarget_t ParseCombatRatingTarget_(const std::string& rTarget)
{
    if (rTarget == "attack")   return CombatRatingTarget_t::Attack;
    if (rTarget == "defense")  return CombatRatingTarget_t::Defense;
    if (rTarget == "movement") return CombatRatingTarget_t::Movement;
    if (rTarget == "rating")   return CombatRatingTarget_t::Rating;
    throw std::runtime_error(
        "Unknown combat_rating target '" + rTarget
        + "' (expected attack, defense, movement, or rating)");
}

std::vector<CombatRatingModifier_t> ParseCombatRatingModifiers_(
    const nlohmann::json& rComponentJson,
    const std::string& rComponentId)
{
    std::vector<CombatRatingModifier_t> modifiers;
    if (!rComponentJson.contains("combat_rating_modifiers"))
    {
        return modifiers;
    }

    const nlohmann::json& rArray = rComponentJson.at("combat_rating_modifiers");
    if (!rArray.is_array())
    {
        throw std::runtime_error(
            "Component '" + rComponentId + "': combat_rating_modifiers must be an array");
    }

    for (const nlohmann::json& rEntry : rArray)
    {
        CombatRatingModifier_t modifier;
        modifier.target = ParseCombatRatingTarget_(rEntry.at("target").get<std::string>());
        modifier.prefix = rEntry.value("prefix", "");
        modifier.suffix = rEntry.value("suffix", "");
        if (modifier.prefix.empty() && modifier.suffix.empty())
        {
            throw std::runtime_error(
                "Component '" + rComponentId
                + "': combat_rating_modifiers entry needs a prefix and/or suffix");
        }
        modifiers.push_back(std::move(modifier));
    }
    return modifiers;
}

std::vector<std::string> ParseCombatRatingLabels_(
    const nlohmann::json& rComponentJson,
    const std::string& rComponentId)
{
    std::vector<std::string> labels =
        ConfigFields::ParseStringArray(rComponentJson, "combat_rating_labels");
    for (const std::string& rLabel : labels)
    {
        if (rLabel.empty())
        {
            throw std::runtime_error(
                "Component '" + rComponentId + "': combat_rating_labels must not contain empty strings");
        }
    }
    return labels;
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
    config.unitName = rComponentJson.value("unit_name", "");
    config.type = rComponentJson.at("type").get<std::string>();
    config.requiredTech = ConfigFields::ParseRequiredTech(rComponentJson);
    config.mineralCost = rComponentJson.value("mineral_cost", 0);
    config.effects = BonusEffectParser::ParseEffects(rComponentJson, EffectSourceKind_t::UnitComponent, config.id);
    config.combatRatingModifiers = ParseCombatRatingModifiers_(rComponentJson, config.id);
    config.combatRatingLabels = ParseCombatRatingLabels_(rComponentJson, config.id);

    const bool bHasDomain = rComponentJson.contains("domain");
    if (config.type == "chassis")
    {
        if (!bHasDomain)
        {
            throw std::runtime_error("Chassis component '" + config.id + "' missing required 'domain'");
        }
        config.domain = BonusEffectParser::ParseUnitDomain(rComponentJson.at("domain").get<std::string>());
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
