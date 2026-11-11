#include "game/units/UnitComponentConfigParser.h"
#include "game/units/UnitComponentRegistry.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/production/ScrapConfigParser.h"
#include <algorithm>
#include <stdexcept>

namespace ac
{

namespace
{

std::vector<CombatRatingModifier_t> ParseCombatRatingModifiers_(const ConfigReader& rReader)
{
    std::vector<CombatRatingModifier_t> modifiers;
    if (!rReader.Has("combat_rating_modifiers"))
    {
        return modifiers;
    }
    for (const ConfigReader& rEntry : rReader.Objects("combat_rating_modifiers"))
    {
        CombatRatingModifier_t modifier;
        modifier.target = rEntry.Enum<CombatRatingTarget_t>("target", "combat_rating target");
        modifier.prefix = rEntry.StringOr("prefix", "");
        modifier.suffix = rEntry.StringOr("suffix", "");
        if (modifier.prefix.empty() && modifier.suffix.empty())
        {
            rEntry.Fail("needs a prefix and/or suffix");
        }
        modifiers.push_back(std::move(modifier));
    }
    return modifiers;
}

std::vector<std::string> ParseCombatRatingLabels_(const ConfigReader& rReader)
{
    std::vector<std::string> labels = rReader.StringArray("combat_rating_labels");
    if (std::ranges::contains(labels, std::string()))
    {
        rReader.FailKey("combat_rating_labels", "must not contain empty strings");
    }
    return labels;
}

std::vector<std::string> ParseRequiresChassis_(const ConfigReader& rReader)
{
    if (!rReader.Has("requires_chassis"))
    {
        return {};
    }
    rReader.NonEmptyArray("requires_chassis");
    std::vector<std::string> chassisIds = rReader.StringArray("requires_chassis");
    if (std::ranges::contains(chassisIds, std::string()))
    {
        rReader.FailKey("requires_chassis", "must not contain empty strings");
    }
    return chassisIds;
}

// Only a chassis has a domain, and every chassis must.
std::optional<UnitDomain_t> ParseDomain_(const ConfigReader& rReader,
                                         const UnitComponentConfig_t& rConfig)
{
    if (rConfig.type != "chassis")
    {
        if (rReader.Has("domain"))
        {
            rReader.FailKey("domain", "is for chassis only, not type '" + rConfig.type + "'");
        }
        return std::nullopt;
    }
    return EffectConfigParser::ParseUnitDomain(rReader.String("domain"));
}

// make_proto overrides the derived plan for an ability in these three cases only.
bool IsAbilityPlanOverride_(UnitPlan_t plan)
{
    return plan == UnitPlan_t::Offense || plan == UnitPlan_t::AirSuperiority
        || plan == UnitPlan_t::Defense;
}

std::optional<UnitPlan_t> ParsePlan_(const ConfigReader& rReader,
                                     const UnitComponentConfig_t& rConfig)
{
    if (!rReader.Has("plan"))
    {
        return std::nullopt;
    }
    if (rConfig.type != "weapon" && rConfig.type != "ability")
    {
        rReader.FailKey("plan", "is for weapons and abilities only, not type '" + rConfig.type
                                    + "'");
    }
    const UnitPlan_t plan = ParseUnitPlan(rReader.String("plan"));
    if (rConfig.type == "ability" && !IsAbilityPlanOverride_(plan))
    {
        rReader.FailKey("plan", "of an ability must be offense, air_superiority, or defense");
    }
    return plan;
}

UnitComponentConfig_t ParseComponent_(const ConfigReader& rReader)
{
    UnitComponentConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.unitName = rReader.StringOr("unit_name", "");
    config.type = rReader.String("type");
    config.requiredTech = rReader.StringOr("required_tech", "");
    config.mineralCost = rReader.Int("mineral_cost");
    config.requiresChassis = ParseRequiresChassis_(rReader);
    config.effects = EffectConfigParser::ParseEffects(
        rReader.Json(), EffectSourceKind_t::UnitComponent, config.id);
    config.onCompleteEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_complete_effects", config.id);
    config.onHoldEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_hold_effects", config.id);
    config.onDetonateEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_detonate_effects", config.id);
    config.onFoundEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_found_effects", config.id);
    config.combatRatingModifiers = ParseCombatRatingModifiers_(rReader);
    config.combatRatingLabels = ParseCombatRatingLabels_(rReader);
    if (rReader.Has("scrap"))
    {
        config.scrap = ScrapConfigParser::ParseOverride(rReader.Object("scrap"));
    }
    config.domain = ParseDomain_(rReader, config);
    config.plan = ParsePlan_(rReader, config);
    if (rReader.Has("upgrade_class"))
    {
        config.upgradeClass = rReader.NonEmptyString("upgrade_class");
    }
    return config;
}

} // namespace

std::vector<UnitComponentConfig_t> UnitComponentConfigParser::ParseConfig(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadPath<UnitComponentConfig_t>(rConfigPath, "unit component",
                                                             ParseComponent_);
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
