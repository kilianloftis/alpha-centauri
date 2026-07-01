#include "lib/effects/BonusEffectParser.h"

#include <stdexcept>

namespace ac
{
namespace BonusEffectParser
{

StatId ParseStatId(const std::string& rStat)
{
    if (rStat == "nutrients")               return StatId::Nutrients;
    if (rStat == "minerals")                return StatId::Minerals;
    if (rStat == "energy")                  return StatId::Energy;
    if (rStat == "econ")                    return StatId::Econ;
    if (rStat == "labs")                    return StatId::Labs;
    if (rStat == "psych")                   return StatId::Psych;
    if (rStat == "attack")                  return StatId::Attack;
    if (rStat == "defense")                 return StatId::Defense;
    if (rStat == "movement")                return StatId::Movement;
    if (rStat == "hit_points")              return StatId::HitPoints;
    if (rStat == "disengage_chance")        return StatId::DisengageChance;
    if (rStat == "fuel")                    return StatId::Fuel;
    if (rStat == "damage_from_out_of_fuel") return StatId::DamageFromOutOfFuel;
    if (rStat == "cargo_capacity")          return StatId::CargoCapacity;
    if (rStat == "difficult_terrain_cost")  return StatId::DifficultTerrainCost;
    if (rStat == "cost_multiplier")         return StatId::CostMultiplier;
    if (rStat == "moisture_tier")           return StatId::MoistureTier;
    throw std::runtime_error("Unknown stat id: '" + rStat + "'");
}

RuleFlagId ParseRuleFlagId(const std::string& rFlag)
{
    if (rFlag == "population_boom") return RuleFlagId::PopulationBoom;
    if (rFlag == "flight")          return RuleFlagId::Flight;
    if (rFlag == "single_use")      return RuleFlagId::SingleUse;
    throw std::runtime_error("Unknown rule flag id: '" + rFlag + "'");
}

ModifierOp ParseModifierOp(const std::string& rOp)
{
    if (rOp == "Add")                 return ModifierOp::Add;
    if (rOp == "AddPercent")          return ModifierOp::AddPercent;
    if (rOp == "MultiplyGeometric")   return ModifierOp::MultiplyGeometric;
    throw std::runtime_error("Unknown modifier op: '" + rOp + "'");
}

EffectScope_t ParseEffectScope(const std::string& rScope)
{
    if (rScope == "ThisBase")        return EffectScope_t::ThisBase;
    if (rScope == "AllOwnerBases")   return EffectScope_t::AllOwnerBases;
    if (rScope == "ThisUnit")        return EffectScope_t::ThisUnit;
    if (rScope == "FactionUnits")    return EffectScope_t::FactionUnits;
    if (rScope == "FactionGlobal")   return EffectScope_t::FactionGlobal;
    if (rScope == "WorldGlobal")     return EffectScope_t::WorldGlobal;
    if (rScope == "ThisPop")         return EffectScope_t::ThisPop;
    if (rScope == "ThisTile")        return EffectScope_t::ThisTile;
    throw std::runtime_error("Unknown effect scope: '" + rScope + "'");
}

EffectPersistence_t ParseEffectPersistence(const std::string& rPersistence)
{
    if (rPersistence == "Instantaneous") return EffectPersistence_t::Instantaneous;
    if (rPersistence == "Continuous")    return EffectPersistence_t::Continuous;
    throw std::runtime_error("Unknown effect persistence: '" + rPersistence + "'");
}

double ParseNumber(const nlohmann::json& parameters, const std::string& key, double defaultValue)
{
    const auto it = parameters.find(key);
    if (it == parameters.end())
        return defaultValue;
    if (it->is_number())
        return it->get<double>();
    if (it->is_string())
        return std::stod(it->get<std::string>());
    throw std::runtime_error("Expected a number or numeric string for parameter '" + key + "'");
}

ConditionKind ParseConditionKind(const std::string& rKind)
{
    if (rKind == "TargetTileHas") return ConditionKind::TargetTileHas;
    throw std::runtime_error("Unknown condition kind: '" + rKind + "'");
}

Condition_t ParseCondition(const nlohmann::json& conditionJson)
{
    Condition_t condition;
    condition.kind = ParseConditionKind(conditionJson.value("kind", ""));
    condition.value = conditionJson.value("value", "");
    if (condition.value.empty())
    {
        throw std::runtime_error("Condition requires a non-empty 'value'");
    }
    return condition;
}

TileSelector_t ParseTileSelector(const nlohmann::json& selectorJson)
{
    TileSelector_t selector;

    const std::string kindStr = selectorJson.value("kind", "BaseTile");
    if (kindStr == "BaseTile")
    {
        selector.kind = TileSelectorKind::BaseTile;
    }
    else if (kindStr == "HasImprovement")
    {
        selector.kind = TileSelectorKind::HasImprovement;
        const std::string improvementId = selectorJson.value("improvement", "");
        if (improvementId.empty())
        {
            throw std::runtime_error("HasImprovement selector requires a non-empty 'improvement' id");
        }
        selector.improvement = improvementId;
    }
    else
    {
        throw std::runtime_error("Unknown tile selector kind: '" + kindStr + "'");
    }

    return selector;
}

EffectConfig_t ParseEffectConfig(const nlohmann::json& effectJson)
{
    EffectConfig_t effect;

    const std::string typeStr = effectJson["type"];
    const std::string scopeStr = effectJson["scope"];
    const std::string persistenceStr = effectJson.value("persistence", "Continuous");
    const auto& parameters = effectJson.value("parameters", nlohmann::json::object());

    effect.scope = ParseEffectScope(scopeStr);
    effect.persistence = ParseEffectPersistence(persistenceStr);
    if (effectJson.contains("condition"))
    {
        effect.condition = ParseCondition(effectJson.at("condition"));
    }

    if (typeStr == "GrantBuilding")
    {
        GrantBuildingEffect_t grantBuilding;
        grantBuilding.buildingId = parameters.value("building_id", "");
        if (grantBuilding.buildingId.empty())
            throw std::runtime_error("GrantBuilding effect missing required 'building_id'");
        effect.effect = grantBuilding;
    }
    else if (typeStr == "GrantTech")
    {
        GrantTechEffect_t grantTech;
        grantTech.techId = parameters.value("tech_id", "");
        if (grantTech.techId.empty())
            throw std::runtime_error("GrantTech effect missing required 'tech_id'");
        effect.effect = grantTech;
    }
    else if (typeStr == "GrantUnit")
    {
        GrantUnitEffect_t grantUnit;
        grantUnit.unitId = parameters.value("unit_id", "");
        if (grantUnit.unitId.empty())
            throw std::runtime_error("GrantUnit effect missing required 'unit_id'");
        effect.effect = grantUnit;
    }
    else if (typeStr == "StatModifier")
    {
        StatModifierEffect_t statModifier;
        statModifier.stat = ParseStatId(parameters.value("stat", ""));
        statModifier.amount = ParseNumber(parameters, "amount", 0.0);
        statModifier.op = ParseModifierOp(parameters.value("op", "Add"));
        // Optional per-tile selector: when present, this modifier applies to each worked
        // tile satisfying the selector instead of once at the base level.
        if (parameters.contains("selector"))
        {
            statModifier.selector = ParseTileSelector(parameters.at("selector"));
        }
        effect.effect = statModifier;
    }
    else if (typeStr == "RuleFlag")
    {
        RuleFlagEffect_t ruleFlag;
        const std::string flagStr = parameters.value("flag", "");
        if (flagStr.empty())
            throw std::runtime_error("RuleFlag effect missing required 'flag'");
        ruleFlag.flag = ParseRuleFlagId(flagStr);
        effect.effect = ruleFlag;
    }
    else if (typeStr == "SocialEngineeringOverride")
    {
        // TODO: define parsing when social engineering rules are finalized
        SocialEngineeringOverrideEffect_t override;
        override.category = parameters.value("category", "");
        override.choice = parameters.value("choice", "");
        effect.effect = override;
    }
    else if (typeStr == "DiplomaticModifier")
    {
        // TODO: define parsing when diplomatic modifier rules are finalized
        DiplomaticModifierEffect_t diplomatic;
        diplomatic.targetFactionId = parameters.value("target_faction_id", "");
        diplomatic.value = static_cast<int>(ParseNumber(parameters, "value", 0.0));
        effect.effect = diplomatic;
    }
    else
    {
        throw std::runtime_error("Unknown effect type: '" + typeStr + "'");
    }

    return effect;
}

std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson)
{
    std::vector<EffectConfig_t> effects;
    if (rContainerJson.contains("effects"))
    {
        for (const auto& rEffectJson : rContainerJson["effects"])
        {
            effects.push_back(ParseEffectConfig(rEffectJson));
        }
    }
    return effects;
}

} // namespace BonusEffectParser
} // namespace ac
