#include "game/effects/BonusEffectParser.h"

#include <stdexcept>

namespace ac
{
namespace BonusEffectParser
{

StatId_t ParseStatId(const std::string& rStat)
{
    if (rStat == "nutrients")               return StatId_t::Nutrients;
    if (rStat == "minerals")                return StatId_t::Minerals;
    if (rStat == "energy")                  return StatId_t::Energy;
    if (rStat == "econ")                    return StatId_t::Econ;
    if (rStat == "labs")                    return StatId_t::Labs;
    if (rStat == "psych")                   return StatId_t::Psych;
    if (rStat == "attack")                  return StatId_t::Attack;
    if (rStat == "defense")                 return StatId_t::Defense;
    if (rStat == "movement")                return StatId_t::Movement;
    if (rStat == "vision")                  return StatId_t::Vision;
    if (rStat == "hit_points")              return StatId_t::HitPoints;
    if (rStat == "psi_damage")              return StatId_t::PsiDamage;
    if (rStat == "disengage_chance")        return StatId_t::DisengageChance;
    if (rStat == "fuel")                    return StatId_t::Fuel;
    if (rStat == "damage_from_out_of_fuel") return StatId_t::DamageFromOutOfFuel;
    if (rStat == "cargo_capacity")          return StatId_t::CargoCapacity;
    if (rStat == "cost_multiplier")         return StatId_t::CostMultiplier;
    if (rStat == "probe_action_cost")       return StatId_t::ProbeActionCost;
    if (rStat == "probe_defense")           return StatId_t::ProbeDefense;
    if (rStat == "probe_failure_scale")     return StatId_t::ProbeFailureScale;
    if (rStat == "probe_success_scale")     return StatId_t::ProbeSuccessScale;
    if (rStat == "starting_experience")     return StatId_t::StartingExperience;
    if (rStat == "morale_bonus")            return StatId_t::MoraleBonus;
    if (rStat == "positive_morale_scale")   return StatId_t::PositiveMoraleScale;
    if (rStat == "growth_rate")             return StatId_t::GrowthRate;
    if (rStat == "tech_cost")               return StatId_t::TechCost;
    if (rStat == "moisture_tier")           return StatId_t::MoistureTier;
    throw std::runtime_error("Unknown stat id: '" + rStat + "'");
}

RuleFlagId_t ParseRuleFlagId(const std::string& rFlag)
{
    if (rFlag == "population_boom")             return RuleFlagId_t::PopulationBoom;
    if (rFlag == "near_zero_growth")            return RuleFlagId_t::NearZeroGrowth;
    if (rFlag == "remove_shroud")               return RuleFlagId_t::RemoveShroud;
    if (rFlag == "remove_fog")                  return RuleFlagId_t::RemoveFog;
    if (rFlag == "flight")                      return RuleFlagId_t::Flight;
    if (rFlag == "single_use")                  return RuleFlagId_t::SingleUse;
    if (rFlag == "ignore_zone_of_control")      return RuleFlagId_t::IgnoreZoneOfControl;
    if (rFlag == "ignores_difficult_terrain")   return RuleFlagId_t::IgnoreDifficultTerrain;
    if (rFlag == "treat_fungus_as_road")         return RuleFlagId_t::TreatFungusAsRoad;
    if (rFlag == "forces_psi_combat")            return RuleFlagId_t::ForcesPsiCombat;
    if (rFlag == "found_base")                   return RuleFlagId_t::FoundBase;
    if (rFlag == "terraform")                   return RuleFlagId_t::Terraform;
    if (rFlag == "supply_crawl")                return RuleFlagId_t::SupplyCrawl;
    if (rFlag == "probe_team")                  return RuleFlagId_t::ProbeTeam;
    if (rFlag == "prevents_disengage")          return RuleFlagId_t::PreventsDisengage;
    if (rFlag == "creche")                      return RuleFlagId_t::Creche;
    if (rFlag == "headquarters")                return RuleFlagId_t::Headquarters;
    if (rFlag == "probe_subversion_immune")     return RuleFlagId_t::ProbeSubversionImmune;
    if (rFlag == "blocks_probe_teams")          return RuleFlagId_t::BlocksProbeTeams;
    if (rFlag == "ignores_probe_block")         return RuleFlagId_t::IgnoresProbeBlock;
    throw std::runtime_error("Unknown rule flag id: '" + rFlag + "'");
}

SocialRatingId_t ParseSocialRatingId(const std::string& rRating)
{
    if (rRating == "economy")    return SocialRatingId_t::Economy;
    if (rRating == "efficiency") return SocialRatingId_t::Efficiency;
    if (rRating == "support")    return SocialRatingId_t::Support;
    if (rRating == "police")     return SocialRatingId_t::Police;
    if (rRating == "morale")     return SocialRatingId_t::Morale;
    if (rRating == "growth")     return SocialRatingId_t::Growth;
    if (rRating == "planet")     return SocialRatingId_t::Planet;
    if (rRating == "research")   return SocialRatingId_t::Research;
    if (rRating == "industry")   return SocialRatingId_t::Industry;
    if (rRating == "probe")      return SocialRatingId_t::Probe;
    throw std::runtime_error("Unknown social rating id: '" + rRating + "'");
}

ModifierOp_t ParseModifierOp(const std::string& rOp)
{
    if (rOp == "Add")                 return ModifierOp_t::Add;
    if (rOp == "AddPercent")          return ModifierOp_t::AddPercent;
    if (rOp == "MultiplyGeometric")   return ModifierOp_t::MultiplyGeometric;
    throw std::runtime_error("Unknown modifier op: '" + rOp + "'");
}

StatModifierEffect_t::AmountSource_t ParseAmountSource(const std::string& rSource)
{
    if (rSource == "ElevationEnergySeed")
    {
        return StatModifierEffect_t::AmountSource_t::ElevationEnergySeed;
    }
    throw std::runtime_error("Unknown amount_source: '" + rSource + "'");
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
    if (rScope == "ProducedAtThisBase") return EffectScope_t::ProducedAtThisBase;
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

ConditionKind_t ParseConditionKind(const std::string& rKind)
{
    if (rKind == "TargetTileHas") return ConditionKind_t::TargetTileHas;
    if (rKind == "AllOf") return ConditionKind_t::AllOf;
    if (rKind == "IsDefending") return ConditionKind_t::IsDefending;
    if (rKind == "OriginBaseIsTargetBase") return ConditionKind_t::OriginBaseIsTargetBase;
    throw std::runtime_error("Unknown condition kind: '" + rKind + "'");
}

Condition_t ParseCondition(const nlohmann::json& conditionJson)
{
    Condition_t condition;
    condition.kind = ParseConditionKind(conditionJson.value("kind", ""));
    if (condition.kind == ConditionKind_t::IsDefending
        || condition.kind == ConditionKind_t::OriginBaseIsTargetBase)
    {
        return condition;
    }
    if (condition.kind == ConditionKind_t::AllOf)
    {
        const bool bHasValues = conditionJson.contains("values")
            && conditionJson.at("values").is_array()
            && !conditionJson.at("values").empty();
        const bool bHasConditions = conditionJson.contains("conditions")
            && conditionJson.at("conditions").is_array()
            && !conditionJson.at("conditions").empty();
        if (!bHasValues && !bHasConditions)
        {
            throw std::runtime_error(
                "AllOf condition requires a non-empty 'values' and/or 'conditions' array");
        }
        if (bHasValues)
        {
            for (const auto& rValue : conditionJson.at("values"))
            {
                if (!rValue.is_string() || rValue.get<std::string>().empty())
                {
                    throw std::runtime_error("AllOf condition values must be non-empty strings");
                }
                condition.values.push_back(rValue.get<std::string>());
            }
        }
        if (bHasConditions)
        {
            for (const auto& rNested : conditionJson.at("conditions"))
            {
                condition.conditions.push_back(ParseCondition(rNested));
            }
        }
        return condition;
    }

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
        selector.kind = TileSelectorKind_t::BaseTile;
    }
    else if (kindStr == "HasImprovement")
    {
        selector.kind = TileSelectorKind_t::HasImprovement;
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

UnitDomain_t ParseUnitDomain(const std::string& rDomain)
{
    if (rDomain == "land") return UnitDomain_t::Land;
    if (rDomain == "sea")  return UnitDomain_t::Sea;
    if (rDomain == "air")  return UnitDomain_t::Air;
    throw std::runtime_error("Unknown unit domain '" + rDomain + "' (expected land, sea, or air)");
}

UnitFilter_t ParseUnitFilter(const nlohmann::json& filterJson)
{
    UnitFilter_t filter;
    const std::string kindStr = filterJson.value("kind", "");
    if (kindStr == "Domain")
    {
        filter.kind = UnitFilterKind_t::Domain;
        const std::string domainStr = filterJson.value("domain", "");
        if (domainStr.empty())
        {
            throw std::runtime_error("Domain unitFilter requires a non-empty 'domain'");
        }
        filter.domain = ParseUnitDomain(domainStr);
    }
    else if (kindStr == "HasComponent")
    {
        filter.kind = UnitFilterKind_t::HasComponent;
        const std::string componentId = filterJson.value("component", "");
        if (componentId.empty())
        {
            throw std::runtime_error("HasComponent unitFilter requires a non-empty 'component' id");
        }
        filter.component = componentId;
    }
    else if (kindStr == "HasFlag")
    {
        filter.kind = UnitFilterKind_t::HasFlag;
        const std::string flagId = filterJson.value("flag", "");
        if (flagId.empty())
        {
            throw std::runtime_error("HasFlag unitFilter requires a non-empty 'flag' id");
        }
        filter.flag = ParseRuleFlagId(flagId);
    }
    else
    {
        throw std::runtime_error("Unknown unitFilter kind: '" + kindStr + "'");
    }
    return filter;
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
    effect.radius = effectJson.value("radius", 0);
    if (effect.radius < 0)
    {
        throw std::runtime_error("Effect 'radius' must be >= 0");
    }
    if (effectJson.contains("condition"))
    {
        effect.condition = ParseCondition(effectJson.at("condition"));
    }
    if (effectJson.contains("unitFilter"))
    {
        effect.unitFilter = ParseUnitFilter(effectJson.at("unitFilter"));
    }
    effect.removedByTech = effectJson.value("removed_by_tech", "");

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
        statModifier.op = ParseModifierOp(parameters.value("op", "Add"));
        if (parameters.contains("amount_source"))
        {
            statModifier.amountSource = ParseAmountSource(parameters.at("amount_source").get<std::string>());
            // amount is the per-band scale when amount_source is set (default 1).
            statModifier.amount = ParseNumber(parameters, "amount", 1.0);
            if (statModifier.stat != StatId_t::Energy)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' is only valid on the energy stat, got '"
                    + parameters.value("stat", "") + "'");
            }
            if (effect.scope != EffectScope_t::ThisTile)
            {
                throw std::runtime_error(
                    "StatModifier 'amount_source' requires scope ThisTile");
            }
        }
        else
        {
            statModifier.amount = ParseNumber(parameters, "amount", 0.0);
        }
        // Optional per-tile selector: when present, this modifier applies to each worked
        // tile satisfying the selector instead of once at the base level. Selectors are
        // resolved only during tile-yield resolution, so they are rejected on any stat
        // that isn't a tile resource — such a modifier would silently never apply.
        if (parameters.contains("selector"))
        {
            if (statModifier.stat != StatId_t::Nutrients && statModifier.stat != StatId_t::Minerals
                && statModifier.stat != StatId_t::Energy)
            {
                throw std::runtime_error("StatModifier 'selector' is only valid on tile resource "
                    "stats (nutrients/minerals/energy), got '" + parameters.value("stat", "") + "'");
            }
            statModifier.selector = ParseTileSelector(parameters.at("selector"));
        }
        statModifier.applyAfterRestriction = parameters.value("apply_after_restriction", false);
        if (statModifier.applyAfterRestriction
            && statModifier.stat != StatId_t::Nutrients && statModifier.stat != StatId_t::Minerals
            && statModifier.stat != StatId_t::Energy)
        {
            throw std::runtime_error(
                "StatModifier 'apply_after_restriction' is only valid on tile resource "
                "stats (nutrients/minerals/energy), got '" + parameters.value("stat", "") + "'");
        }
        if (statModifier.applyAfterRestriction && statModifier.op != ModifierOp_t::Add)
        {
            throw std::runtime_error(
                "StatModifier 'apply_after_restriction' requires op Add");
        }
        effect.effect = statModifier;
    }
    else if (typeStr == "TileResourceCap")
    {
        TileResourceCapEffect_t cap;
        cap.stat = ParseStatId(parameters.value("stat", ""));
        if (cap.stat != StatId_t::Nutrients && cap.stat != StatId_t::Minerals
            && cap.stat != StatId_t::Energy)
        {
            throw std::runtime_error(
                "TileResourceCap 'stat' must be nutrients/minerals/energy, got '"
                + parameters.value("stat", "") + "'");
        }
        cap.max = static_cast<int>(ParseNumber(parameters, "max", 2.0));
        if (cap.max < 0)
        {
            throw std::runtime_error("TileResourceCap 'max' must be >= 0");
        }
        if (effect.scope != EffectScope_t::FactionGlobal
            && effect.scope != EffectScope_t::AllOwnerBases
            && effect.scope != EffectScope_t::WorldGlobal)
        {
            throw std::runtime_error(
                "TileResourceCap requires a faction-wide scope (FactionGlobal / AllOwnerBases / "
                "WorldGlobal)");
        }
        effect.effect = cap;
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
    else if (typeStr == "SocialRatingModifier")
    {
        SocialRatingModifierEffect_t ratingMod;
        const std::string ratingStr = parameters.value("rating", "");
        if (ratingStr.empty())
            throw std::runtime_error("SocialRatingModifier effect missing required 'rating'");
        ratingMod.rating = ParseSocialRatingId(ratingStr);
        ratingMod.amount = static_cast<int>(ParseNumber(parameters, "amount", 0.0));
        effect.effect = ratingMod;
    }
    else if (typeStr == "Conceal")
    {
        ConcealEffect_t conceal;
        conceal.channel = parameters.value("channel", "");
        if (conceal.channel.empty())
            throw std::runtime_error("Conceal effect missing required 'channel'");
        effect.effect = conceal;
    }
    else if (typeStr == "Detect")
    {
        DetectEffect_t detect;
        detect.channel = parameters.value("channel", "");
        if (detect.channel.empty())
            throw std::runtime_error("Detect effect missing required 'channel'");
        effect.effect = detect;
    }
    else
    {
        throw std::runtime_error("Unknown effect type: '" + typeStr + "'");
    }

    return effect;
}

void ValidateScopeForSource(EffectScope_t scope, EffectSourceKind_t sourceKind,
                            const std::string& rSourceId)
{
    if (scope == EffectScope_t::ThisPop && sourceKind != EffectSourceKind_t::PopType)
    {
        throw std::runtime_error("Effect on '" + rSourceId
            + "': scope ThisPop is only meaningful on a pop type config");
    }
    if (scope == EffectScope_t::ThisUnit && sourceKind != EffectSourceKind_t::UnitComponent)
    {
        throw std::runtime_error("Effect on '" + rSourceId
            + "': scope ThisUnit is only meaningful on a unit component config");
    }
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

std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson,
                                         EffectSourceKind_t sourceKind,
                                         const std::string& rSourceId)
{
    std::vector<EffectConfig_t> effects = ParseEffects(rContainerJson);
    for (const EffectConfig_t& rEffect : effects)
    {
        ValidateScopeForSource(rEffect.scope, sourceKind, rSourceId);
    }
    return effects;
}

} // namespace BonusEffectParser
} // namespace ac
