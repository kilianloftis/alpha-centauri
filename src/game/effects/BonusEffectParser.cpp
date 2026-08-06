#include "game/effects/BonusEffectParser.h"

#include <magic_enum.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{
namespace BonusEffectParser
{

namespace
{

bool IsTileResourceStat_(StatId_t stat)
{
    return stat == StatId_t::Nutrients || stat == StatId_t::Minerals || stat == StatId_t::Energy;
}

void RequireScope_(EffectScope_t scope,
                   std::initializer_list<EffectScope_t> allowed,
                   const std::string& rErrorMessage)
{
    for (const EffectScope_t allowedScope : allowed)
    {
        if (scope == allowedScope)
        {
            return;
        }
    }
    throw std::runtime_error(rErrorMessage);
}

void ParseGrantBuilding_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    GrantBuildingEffect_t grantBuilding;
    grantBuilding.buildingId = parameters.value("building_id", "");
    if (grantBuilding.buildingId.empty())
    {
        throw std::runtime_error("GrantBuilding effect missing required 'building_id'");
    }
    rEffect.effect = grantBuilding;
}

void ParseGrantTech_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    GrantTechEffect_t grantTech;
    grantTech.techId = parameters.value("tech_id", "");
    if (grantTech.techId.empty())
    {
        throw std::runtime_error("GrantTech effect missing required 'tech_id'");
    }
    rEffect.effect = grantTech;
}

void ParseGrantUnit_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    GrantUnitEffect_t grantUnit;
    grantUnit.unitId = parameters.value("unit_id", "");
    if (grantUnit.unitId.empty())
    {
        throw std::runtime_error("GrantUnit effect missing required 'unit_id'");
    }
    rEffect.effect = grantUnit;
}

void ParseGrantEnergy_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    GrantEnergyEffect_t grantEnergy;
    grantEnergy.amount = static_cast<int>(ParseNumber(parameters, "amount", 0.0));
    if (grantEnergy.amount < 0)
    {
        throw std::runtime_error("GrantEnergy 'amount' must be >= 0");
    }
    rEffect.effect = grantEnergy;
}

void ParseWorldParameter_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    WorldParameterEffect_t worldParam;
    const std::string paramStr = parameters.value("parameter", "");
    if (paramStr == "sea_level")
    {
        worldParam.parameter = WorldParameterId_t::SeaLevel;
    }
    else
    {
        throw std::runtime_error("Unknown WorldParameter '" + paramStr + "'");
    }
    worldParam.amount = static_cast<int>(ParseNumber(parameters, "amount", 0.0));
    rEffect.effect = worldParam;
}

void ParseInfiltration_(const nlohmann::json& /*parameters*/, EffectConfig_t& rEffect)
{
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::FactionGlobal, EffectScope_t::WorldGlobal},
        "Infiltration requires scope FactionGlobal or WorldGlobal");
    if (rEffect.scope == EffectScope_t::FactionGlobal && !rEffect.factionFilter)
    {
        throw std::runtime_error(
            "FactionGlobal Infiltration requires a factionFilter "
            "(WorldGlobal without a filter means all other factions)");
    }
    if (rEffect.factionFilter
        && rEffect.factionFilter->kind == FactionFilterKind_t::ActionTarget
        && rEffect.persistence != EffectPersistence_t::Instantaneous)
    {
        throw std::runtime_error(
            "factionFilter ActionTarget requires persistence Instantaneous");
    }
    rEffect.effect = InfiltrationEffect_t{};
}

void ParseStatModifier_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    StatModifierEffect_t statModifier;
    statModifier.stat = ParseStatId(parameters.value("stat", ""));
    statModifier.op = ParseModifierOp(parameters.value("op", "Add"));
    if (parameters.contains("amount_source"))
    {
        if (statModifier.op != ModifierOp_t::Add)
        {
            throw std::runtime_error("StatModifier 'amount_source' requires op Add");
        }
        statModifier.amountSource =
            ParseAmountSource(parameters.at("amount_source").get<std::string>());
        // amount is the per-band scale when amount_source is set (default 1).
        statModifier.amount = ParseNumber(parameters, "amount", 1.0);
        if (statModifier.stat != StatId_t::Energy)
        {
            throw std::runtime_error(
                "StatModifier 'amount_source' is only valid on the energy stat, got '"
                + parameters.value("stat", "") + "'");
        }
        if (rEffect.scope != EffectScope_t::ThisTile)
        {
            throw std::runtime_error("StatModifier 'amount_source' requires scope ThisTile");
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
        if (!IsTileResourceStat_(statModifier.stat))
        {
            throw std::runtime_error(
                "StatModifier 'selector' is only valid on tile resource "
                "stats (nutrients/minerals/energy), got '" + parameters.value("stat", "") + "'");
        }
        statModifier.selector = ParseTileSelector(parameters.at("selector"));
    }
    statModifier.applyAfterRestriction = parameters.value("apply_after_restriction", false);
    if (statModifier.applyAfterRestriction && !IsTileResourceStat_(statModifier.stat))
    {
        throw std::runtime_error(
            "StatModifier 'apply_after_restriction' is only valid on tile resource "
            "stats (nutrients/minerals/energy), got '" + parameters.value("stat", "") + "'");
    }
    if (statModifier.applyAfterRestriction && statModifier.op != ModifierOp_t::Add)
    {
        throw std::runtime_error("StatModifier 'apply_after_restriction' requires op Add");
    }
    rEffect.effect = statModifier;
}

void ParseTileResourceCap_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    TileResourceCapEffect_t cap;
    cap.stat = ParseStatId(parameters.value("stat", ""));
    if (!IsTileResourceStat_(cap.stat))
    {
        throw std::runtime_error(
            "TileResourceCap 'stat' must be nutrients/minerals/energy, got '"
            + parameters.value("stat", "") + "'");
    }
    cap.max = static_cast<int>(RequireNumber(parameters, "max"));
    if (cap.max < 0)
    {
        throw std::runtime_error("TileResourceCap 'max' must be >= 0");
    }
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::FactionGlobal, EffectScope_t::AllOwnerBases, EffectScope_t::WorldGlobal},
        "TileResourceCap requires a faction-wide scope (FactionGlobal / AllOwnerBases / "
        "WorldGlobal)");
    rEffect.effect = cap;
}

void ParseRuleFlag_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    RuleFlagEffect_t ruleFlag;
    const std::string flagStr = parameters.value("flag", "");
    if (flagStr.empty())
    {
        throw std::runtime_error("RuleFlag effect missing required 'flag'");
    }
    ruleFlag.flag = ParseRuleFlagId(flagStr);
    rEffect.effect = ruleFlag;
}

void ParsePermission_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    PermissionEffect_t permission;
    const std::string permissionStr = parameters.value("permission", "");
    if (permissionStr.empty())
    {
        throw std::runtime_error("Permission effect missing required 'permission'");
    }
    const auto id = magic_enum::enum_cast<PermissionId_t>(permissionStr);
    if (!id.has_value())
    {
        throw std::runtime_error("Unknown permission id: '" + permissionStr + "'");
    }
    permission.permission = *id;
    rEffect.effect = permission;
}

void ParseSocialEngineeringOverride_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    // TODO: define parsing when social engineering rules are finalized
    SocialEngineeringOverrideEffect_t seOverride;
    seOverride.category = parameters.value("category", "");
    seOverride.choice = parameters.value("choice", "");
    rEffect.effect = seOverride;
}

void ParseDiplomaticModifier_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    // TODO: define parsing when diplomatic modifier rules are finalized
    DiplomaticModifierEffect_t diplomatic;
    diplomatic.targetFactionId = parameters.value("target_faction_id", "");
    diplomatic.value = static_cast<int>(ParseNumber(parameters, "value", 0.0));
    rEffect.effect = diplomatic;
}

void ParseSocialRatingModifier_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    SocialRatingModifierEffect_t ratingMod;
    const std::string ratingStr = parameters.value("rating", "");
    if (ratingStr.empty())
    {
        throw std::runtime_error("SocialRatingModifier effect missing required 'rating'");
    }
    ratingMod.rating = ParseSocialRatingId(ratingStr);
    ratingMod.amount = static_cast<int>(ParseNumber(parameters, "amount", 0.0));
    rEffect.effect = ratingMod;
}

void ParseConceal_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    ConcealEffect_t conceal;
    conceal.channel = parameters.value("channel", "");
    if (conceal.channel.empty())
    {
        throw std::runtime_error("Conceal effect missing required 'channel'");
    }
    rEffect.effect = conceal;
}

void ParseDetect_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    DetectEffect_t detect;
    detect.channel = parameters.value("channel", "");
    if (detect.channel.empty())
    {
        throw std::runtime_error("Detect effect missing required 'channel'");
    }
    rEffect.effect = detect;
}

void ParseOrbitalAttack_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::FactionGlobal, EffectScope_t::AllOwnerBases},
        "OrbitalAttack requires scope FactionGlobal or AllOwnerBases");
    OrbitalAttackEffect_t orbitalAttack;
    orbitalAttack.chance = static_cast<int>(RequireNumber(parameters, "chance"));
    if (orbitalAttack.chance < 0 || orbitalAttack.chance > 100)
    {
        throw std::runtime_error("OrbitalAttack 'chance' must be in [0, 100]");
    }
    orbitalAttack.cooldownTurns = static_cast<int>(RequireNumber(parameters, "cooldown_turns"));
    if (orbitalAttack.cooldownTurns < 0)
    {
        throw std::runtime_error("OrbitalAttack 'cooldown_turns' must be >= 0");
    }
    orbitalAttack.chanceOfDestructionOnFail =
        static_cast<int>(ParseNumber(parameters, "chance_of_destruction_on_fail", 0.0));
    if (orbitalAttack.chanceOfDestructionOnFail < 0
        || orbitalAttack.chanceOfDestructionOnFail > 100)
    {
        throw std::runtime_error(
            "OrbitalAttack 'chance_of_destruction_on_fail' must be in [0, 100]");
    }
    rEffect.effect = orbitalAttack;
}

void ParseInterceptAttempt_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    if (!rEffect.unitFilter)
    {
        throw std::runtime_error("InterceptAttempt requires a unitFilter");
    }
    InterceptAttemptEffect_t intercept;
    intercept.chance = static_cast<int>(RequireNumber(parameters, "chance"));
    if (intercept.chance < 0 || intercept.chance > 100)
    {
        throw std::runtime_error("InterceptAttempt 'chance' must be in [0, 100]");
    }
    if (parameters.contains("cooldown_turns"))
    {
        intercept.cooldownTurns = static_cast<int>(RequireNumber(parameters, "cooldown_turns"));
        if (intercept.cooldownTurns < 0)
        {
            throw std::runtime_error("InterceptAttempt 'cooldown_turns' must be >= 0");
        }
    }
    intercept.chanceOfDestructionOnFail =
        static_cast<int>(ParseNumber(parameters, "chance_of_destruction_on_fail", 0.0));
    if (intercept.chanceOfDestructionOnFail < 0 || intercept.chanceOfDestructionOnFail > 100)
    {
        throw std::runtime_error(
            "InterceptAttempt 'chance_of_destruction_on_fail' must be in [0, 100]");
    }
    rEffect.effect = intercept;
}

void ParseTransportParams_(const nlohmann::json& parameters, EffectConfig_t& rEffect)
{
    RequireScope_(
        rEffect.scope,
        {EffectScope_t::ThisUnit},
        "TransportParams requires scope ThisUnit");
    TransportParamsEffect_t transport;
    if (parameters.contains("passenger_domains"))
    {
        if (!parameters.at("passenger_domains").is_array())
        {
            throw std::runtime_error("TransportParams 'passenger_domains' must be an array");
        }
        for (const auto& rDomainJson : parameters.at("passenger_domains"))
        {
            if (!rDomainJson.is_string())
            {
                throw std::runtime_error(
                    "TransportParams passenger_domains entries must be strings");
            }
            transport.passengerDomains.push_back(
                ParseUnitDomain(rDomainJson.get<std::string>()));
        }
    }
    if (parameters.contains("load_site_flags"))
    {
        if (!parameters.at("load_site_flags").is_array())
        {
            throw std::runtime_error("TransportParams 'load_site_flags' must be an array");
        }
        for (const auto& rFlagJson : parameters.at("load_site_flags"))
        {
            if (!rFlagJson.is_string() || rFlagJson.get<std::string>().empty())
            {
                throw std::runtime_error(
                    "TransportParams load_site_flags entries must be non-empty strings");
            }
            transport.loadSiteFlags.push_back(ParseRuleFlagId(rFlagJson.get<std::string>()));
        }
    }
    if (transport.passengerDomains.empty() && transport.loadSiteFlags.empty())
    {
        throw std::runtime_error(
            "TransportParams requires at least one of 'passenger_domains' "
            "or 'load_site_flags'");
    }
    rEffect.effect = transport;
}

using ParseEffectFn_ = void (*)(const nlohmann::json& parameters, EffectConfig_t& rEffect);

const std::unordered_map<std::string, ParseEffectFn_>& EffectTypeParsers_()
{
    static const std::unordered_map<std::string, ParseEffectFn_> k_Parsers = {
        {"GrantBuilding", ParseGrantBuilding_},
        {"GrantTech", ParseGrantTech_},
        {"GrantUnit", ParseGrantUnit_},
        {"GrantEnergy", ParseGrantEnergy_},
        {"WorldParameter", ParseWorldParameter_},
        {"Infiltration", ParseInfiltration_},
        {"StatModifier", ParseStatModifier_},
        {"TileResourceCap", ParseTileResourceCap_},
        {"RuleFlag", ParseRuleFlag_},
        {"Permission", ParsePermission_},
        {"SocialEngineeringOverride", ParseSocialEngineeringOverride_},
        {"DiplomaticModifier", ParseDiplomaticModifier_},
        {"SocialRatingModifier", ParseSocialRatingModifier_},
        {"Conceal", ParseConceal_},
        {"Detect", ParseDetect_},
        {"OrbitalAttack", ParseOrbitalAttack_},
        {"InterceptAttempt", ParseInterceptAttempt_},
        {"TransportParams", ParseTransportParams_},
    };
    return k_Parsers;
}

} // namespace

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
    if (rStat == "difficult_terrain_cost")  return StatId_t::DifficultTerrainCost;
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
    if (rStat == "commerce_rate")           return StatId_t::CommerceRate;
    if (rStat == "council_votes")          return StatId_t::CouncilVotes;
    if (rStat == "commerce_energy_bonus")   return StatId_t::CommerceEnergyBonus;
    throw std::runtime_error("Unknown stat id: '" + rStat + "'");
}

RuleFlagId_t ParseRuleFlagId(const std::string& rFlag)
{
    if (rFlag == "flight")                      return RuleFlagId_t::Flight;
    if (rFlag == "population_boom")             return RuleFlagId_t::PopulationBoom;
    if (rFlag == "near_zero_growth")            return RuleFlagId_t::NearZeroGrowth;
    if (rFlag == "remove_shroud")               return RuleFlagId_t::RemoveShroud;
    if (rFlag == "remove_fog")                  return RuleFlagId_t::RemoveFog;
    if (rFlag == "single_use")                  return RuleFlagId_t::SingleUse;
    if (rFlag == "ignore_zone_of_control")      return RuleFlagId_t::IgnoreZoneOfControl;
    if (rFlag == "ignores_difficult_terrain")   return RuleFlagId_t::IgnoreDifficultTerrain;
    if (rFlag == "treat_fungus_as_road")         return RuleFlagId_t::TreatFungusAsRoad;
    if (rFlag == "forces_psi_combat")            return RuleFlagId_t::ForcesPsiCombat;
    if (rFlag == "found_base")                   return RuleFlagId_t::FoundBase;
    if (rFlag == "terraform")                   return RuleFlagId_t::Terraform;
    if (rFlag == "supply_crawl")                return RuleFlagId_t::SupplyCrawl;
    if (rFlag == "probe_team")                  return RuleFlagId_t::ProbeTeam;
    if (rFlag == "cannot_capture_bases")        return RuleFlagId_t::CannotCaptureBases;
    if (rFlag == "no_conquest_repair")          return RuleFlagId_t::NoConquestRepair;
    if (rFlag == "prevents_conquest_pop_loss")  return RuleFlagId_t::PreventsConquestPopLoss;
    if (rFlag == "prevents_disengage")          return RuleFlagId_t::PreventsDisengage;
    if (rFlag == "refuels_air")                 return RuleFlagId_t::RefuelsAir;
    if (rFlag == "loads_air_transport")         return RuleFlagId_t::LoadsAirTransport;
    if (rFlag == "creche")                      return RuleFlagId_t::Creche;
    if (rFlag == "headquarters")                return RuleFlagId_t::Headquarters;
    if (rFlag == "probe_subversion_immune")     return RuleFlagId_t::ProbeSubversionImmune;
    if (rFlag == "blocks_probe_teams")          return RuleFlagId_t::BlocksProbeTeams;
    if (rFlag == "ignores_probe_block")         return RuleFlagId_t::IgnoresProbeBlock;
    if (rFlag == "atrocities_forbidden")        return RuleFlagId_t::AtrocitiesForbidden;
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
    const auto op = magic_enum::enum_cast<ModifierOp_t>(rOp);
    if (!op.has_value())
    {
        throw std::runtime_error("Unknown modifier op: '" + rOp + "'");
    }
    return *op;
}

StatModifierEffect_t::AmountSource_t ParseAmountSource(const std::string& rSource)
{
    const auto source = magic_enum::enum_cast<StatModifierEffect_t::AmountSource_t>(rSource);
    if (!source.has_value())
    {
        throw std::runtime_error("Unknown amount_source: '" + rSource + "'");
    }
    return *source;
}

EffectScope_t ParseEffectScope(const std::string& rScope)
{
    const auto scope = magic_enum::enum_cast<EffectScope_t>(rScope);
    if (!scope.has_value())
    {
        throw std::runtime_error("Unknown effect scope: '" + rScope + "'");
    }
    return *scope;
}

EffectPersistence_t ParseEffectPersistence(const std::string& rPersistence)
{
    const auto persistence = magic_enum::enum_cast<EffectPersistence_t>(rPersistence);
    if (!persistence.has_value())
    {
        throw std::runtime_error("Unknown effect persistence: '" + rPersistence + "'");
    }
    return *persistence;
}

double ParseNumber(const nlohmann::json& parameters, const std::string& key, double defaultValue)
{
    const auto it = parameters.find(key);
    if (it == parameters.end())
    {
        return defaultValue;
    }
    if (it->is_number())
    {
        return it->get<double>();
    }
    if (it->is_string())
    {
        const std::string& rStr = it->get_ref<const std::string&>();
        try
        {
            std::size_t idx = 0;
            const double value = std::stod(rStr, &idx);
            if (idx != rStr.size())
            {
                throw std::runtime_error(
                    "Invalid numeric string for parameter '" + key
                    + "': trailing characters");
            }
            return value;
        }
        catch (const std::invalid_argument&)
        {
            throw std::runtime_error("Invalid numeric string for parameter '" + key + "'");
        }
        catch (const std::out_of_range&)
        {
            throw std::runtime_error(
                "Numeric string out of range for parameter '" + key + "'");
        }
    }
    throw std::runtime_error("Expected a number or numeric string for parameter '" + key + "'");
}

double RequireNumber(const nlohmann::json& parameters, const std::string& key)
{
    if (!parameters.contains(key))
    {
        throw std::runtime_error("Missing required parameter '" + key + "'");
    }
    return ParseNumber(parameters, key, 0.0);
}

ConditionKind_t ParseConditionKind(const std::string& rKind)
{
    const auto kind = magic_enum::enum_cast<ConditionKind_t>(rKind);
    if (!kind.has_value())
    {
        throw std::runtime_error("Unknown condition kind: '" + rKind + "'");
    }
    return *kind;
}

Condition_t ParseCondition(const nlohmann::json& conditionJson)
{
    Condition_t condition;
    condition.kind = ParseConditionKind(conditionJson.value("kind", ""));
    if (condition.kind == ConditionKind_t::IsDefending
        || condition.kind == ConditionKind_t::OriginBaseIsTargetBase
        || condition.kind == ConditionKind_t::AttackerIsEmbarked)
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
    if (rDomain == "orbital") return UnitDomain_t::Orbital;
    throw std::runtime_error(
        "Unknown unit domain '" + rDomain + "' (expected land, sea, air, or orbital)");
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

FactionFilter_t ParseFactionFilter(const nlohmann::json& filterJson)
{
    FactionFilter_t filter;
    const std::string kindStr = filterJson.value("kind", "");
    if (kindStr == "ActionTarget")
    {
        filter.kind = FactionFilterKind_t::ActionTarget;
    }
    else if (kindStr == "CouncilMembers")
    {
        filter.kind = FactionFilterKind_t::CouncilMembers;
    }
    else
    {
        throw std::runtime_error("Unknown factionFilter kind: '" + kindStr + "'");
    }
    return filter;
}

EffectConfig_t ParseEffectConfig(const nlohmann::json& effectJson)
{
    EffectConfig_t effect;

    const std::string typeStr = effectJson.at("type").get<std::string>();
    const std::string scopeStr = effectJson.at("scope").get<std::string>();
    const std::string persistenceStr = effectJson.value("persistence", "Continuous");
    const auto& parameters = effectJson.value("parameters", nlohmann::json::object());

    effect.scope = ParseEffectScope(scopeStr);
    effect.persistence = ParseEffectPersistence(persistenceStr);
    effect.radius = effectJson.value("radius", 0);
    if (effect.radius < 0)
    {
        throw std::runtime_error("Effect 'radius' must be >= 0");
    }
    if (effect.radius != 0 && effect.scope != EffectScope_t::ThisTile)
    {
        throw std::runtime_error("Effect 'radius' is only valid with scope ThisTile");
    }
    if (effectJson.contains("condition"))
    {
        effect.condition = ParseCondition(effectJson.at("condition"));
    }
    if (effectJson.contains("unitFilter"))
    {
        effect.unitFilter = ParseUnitFilter(effectJson.at("unitFilter"));
    }
    if (effectJson.contains("factionFilter"))
    {
        effect.factionFilter = ParseFactionFilter(effectJson.at("factionFilter"));
    }
    effect.removedByTech = effectJson.value("removed_by_tech", "");

    const auto& parsers = EffectTypeParsers_();
    const auto it = parsers.find(typeStr);
    if (it == parsers.end())
    {
        throw std::runtime_error("Unknown effect type: '" + typeStr + "'");
    }
    it->second(parameters, effect);

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
    if (scope == EffectScope_t::ThisBase || scope == EffectScope_t::ProducedAtThisBase)
    {
        // Listed exhaustively (no default:) so adding an EffectSourceKind_t forces a decision
        // here rather than silently inheriting the rejection.
        bool bCanSupplyOriginBase = false;
        switch (sourceKind)
        {
        case EffectSourceKind_t::Building:
        case EffectSourceKind_t::PopType:
        case EffectSourceKind_t::SocialPolicy:
        case EffectSourceKind_t::SocialRating:
            bCanSupplyOriginBase = true;
            break;
        case EffectSourceKind_t::UnitComponent:
        case EffectSourceKind_t::Improvement:
        case EffectSourceKind_t::Faction:
        case EffectSourceKind_t::CouncilProposal:
        case EffectSourceKind_t::CouncilRules:
        case EffectSourceKind_t::ProbeAction:
        case EffectSourceKind_t::TileYieldRules:
            bCanSupplyOriginBase = false;
            break;
        }
        if (!bCanSupplyOriginBase)
        {
            const char* pScopeName =
                scope == EffectScope_t::ThisBase ? "ThisBase" : "ProducedAtThisBase";
            throw std::runtime_error(
                "Effect on '" + rSourceId + "': scope " + pScopeName
                + " requires a source that can supply an origin base "
                  "(Building, PopType, SocialPolicy, or SocialRating)");
        }
    }
}

std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson)
{
    std::vector<EffectConfig_t> effects;
    if (rContainerJson.contains("effects"))
    {
        const nlohmann::json& rEffectsJson = rContainerJson.at("effects");
        if (!rEffectsJson.is_array())
        {
            throw std::runtime_error("'effects' must be a JSON array");
        }
        for (const auto& rEffectJson : rEffectsJson)
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
