#include "game/units/UnitComponentConfigParser.h"
#include "game/units/UnitComponentRegistry.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/production/ScrapConfigParser.h"

#include <magic_enum.hpp>

#include <stdexcept>

namespace ac
{

namespace
{

CombatRatingTarget_t ParseCombatRatingTarget_(const std::string& rTarget)
{
    const auto parsed =
        magic_enum::enum_cast<CombatRatingTarget_t>(rTarget, magic_enum::case_insensitive);
    if (!parsed)
    {
        throw std::runtime_error(
            "Unknown combat_rating target '" + rTarget
            + "' (expected attack, defense, movement, or rating)");
    }
    return *parsed;
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

std::vector<std::string> ParseRequiresChassis_(const nlohmann::json& rComponentJson,
                                               const std::string& rComponentId)
{
    if (!rComponentJson.contains("requires_chassis"))
    {
        return {};
    }
    std::vector<std::string> chassisIds =
        ConfigFields::ParseStringArray(rComponentJson, "requires_chassis");
    if (chassisIds.empty())
    {
        throw std::runtime_error(
            "Component '" + rComponentId + "': requires_chassis must be a non-empty array");
    }
    if (std::ranges::contains(chassisIds, std::string()))
    {
        throw std::runtime_error(
            "Component '" + rComponentId + "': requires_chassis must not contain empty strings");
    }
    return chassisIds;
}

// Only a chassis has a domain, and every chassis must.
std::optional<UnitDomain_t> ParseDomain_(const nlohmann::json& rComponentJson,
                                         const UnitComponentConfig_t& rConfig)
{
    const bool bHasDomain = rComponentJson.contains("domain");
    if (rConfig.type != "chassis")
    {
        if (bHasDomain)
        {
            throw std::runtime_error(
                "Component '" + rConfig.id + "' of type '" + rConfig.type
                + "' must not declare 'domain' (chassis only)");
        }
        return std::nullopt;
    }
    if (!bHasDomain)
    {
        throw std::runtime_error("Chassis component '" + rConfig.id + "' missing required 'domain'");
    }
    const std::string domain = rComponentJson.at("domain").get<std::string>();
    return EffectConfigParser::ParseUnitDomain(domain);
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
    config.name = ConfigFields::ParseName(rComponentJson, config.id, "name");
    config.unitName = rComponentJson.value("unit_name", "");
    config.type = rComponentJson.at("type").get<std::string>();
    config.requiredTech = ConfigFields::ParseRequiredTech(rComponentJson);
    config.mineralCost = rComponentJson.at("mineral_cost").get<int>();
    config.requiresChassis = ParseRequiresChassis_(rComponentJson, config.id);
    config.effects = EffectConfigParser::ParseEffects(rComponentJson, EffectSourceKind_t::UnitComponent, config.id);
    config.onCompleteEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rComponentJson, "on_complete_effects", config.id);
    config.onHoldEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rComponentJson, "on_hold_effects", config.id);
    config.onDetonateEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rComponentJson, "on_detonate_effects", config.id);
    config.combatRatingModifiers = ParseCombatRatingModifiers_(rComponentJson, config.id);
    config.combatRatingLabels = ParseCombatRatingLabels_(rComponentJson, config.id);
    if (rComponentJson.contains("scrap"))
    {
        config.scrap = ScrapConfigParser::ParseOverride(rComponentJson.at("scrap"),
                                                "Component '" + config.id + "' scrap");
    }
    config.domain = ParseDomain_(rComponentJson, config);
    return config;
}

void ValidateComponentChassisRequirements(const UnitComponentRegistry& rRegistry)
{
    for (const UnitComponentConfig_t& rComponent : rRegistry.GetAll())
    {
        for (const std::string& rChassisId : rComponent.requiresChassis)
        {
            const UnitComponentConfig_t* pChassis = rRegistry.Find(rChassisId);
            if (!pChassis || pChassis->type != "chassis")
            {
                throw std::runtime_error(
                    "Component '" + rComponent.id + "' requires_chassis '" + rChassisId
                    + "' is not a chassis component");
            }
        }
    }
}

} // namespace ac
