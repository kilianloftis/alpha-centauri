#include "game/diplomacy/DiplomacyConfigParser.h"

#include "lib/config/ConfigFields.h"
#include "lib/config/ConfigSets.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/map/MapUtils.h"
#include "lib/config/EnumNames.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

const nlohmann::json& Require_(const nlohmann::json& rJson, const std::string& rKey,
                               const std::string& rContext)
{
    if (!rJson.contains(rKey))
    {
        throw std::runtime_error(rContext + " requires '" + rKey + "'");
    }
    return rJson.at(rKey);
}

bool RequireBool_(const nlohmann::json& rJson, const std::string& rKey,
                  const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, rKey, rContext);
    if (!rValue.is_boolean())
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be a boolean");
    }
    return rValue.get<bool>();
}

const nlohmann::json& RequireObject_(const nlohmann::json& rJson, const std::string& rKey,
                                     const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, rKey, rContext);
    if (!rValue.is_object())
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be a JSON object");
    }
    return rValue;
}

// The stats some rule resolves toward a partner; a status effect on any other would be ignored.
bool IsPartnerStat_(StatId_t stat)
{
    return stat == StatId_t::CommerceRate || stat == StatId_t::ProbeActorCost;
}

// A probe surcharge may be limited to base quotes (Pact's); nothing else reads a condition here.
bool ConditionAllowed_(const EffectConfig_t& rEffect, StatId_t stat)
{
    if (!rEffect.condition)
    {
        return true;
    }
    return stat == StatId_t::ProbeActorCost
        && std::holds_alternative<TargetIsBase_t>(*rEffect.condition);
}

std::vector<EffectConfig_t> ParseStatusEffects_(const nlohmann::json& rJson,
                                                  const std::string& rContext)
{
    std::vector<EffectConfig_t> effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::DiplomaticStatus, rContext);
    for (const EffectConfig_t& rEffect : effects)
    {
        const auto* pModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (!pModifier || !IsPartnerStat_(pModifier->stat) || pModifier->amountSource
            || !ConditionAllowed_(rEffect, pModifier->stat))
        {
            throw std::runtime_error(rContext
                                     + ": effects may only be StatModifier commerce_rate "
                                       "entries, or probe_actor_cost ones optionally limited "
                                       "to TargetIsBase");
        }
    }
    return effects;
}

std::optional<int> RequireOptionalPositiveInt_(const nlohmann::json& rJson,
                                               const std::string& rKey,
                                               const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, rKey, rContext);
    if (rValue.is_null())
    {
        return std::nullopt;
    }
    if (!rValue.is_number_integer() || rValue.get<int>() <= 0)
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be null or an integer > 0");
    }
    return rValue.get<int>();
}

DiplomaticStatusRules_t ParseStatus_(const nlohmann::json& rJson, DiplomaticStatus_t status,
                                     const std::string& rContext)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rContext + " must be a JSON object");
    }
    static const std::vector<std::string> known = {
        "enter_territory",      "share_tiles", "repair_at_bases", "may_attack",
        "break_by_attack",      "defensive_obligation", "effects", "duration_turns",
    };
    ConfigFields::RejectUnknownKeys(rJson, known, rContext);

    DiplomaticStatusRules_t rules;
    rules.bEnterTerritory = RequireBool_(rJson, "enter_territory", rContext);
    rules.bShareTiles = RequireBool_(rJson, "share_tiles", rContext);
    rules.bRepairAtBases = RequireBool_(rJson, "repair_at_bases", rContext);
    rules.bMayAttack = RequireBool_(rJson, "may_attack", rContext);
    rules.bBreakByAttack = RequireBool_(rJson, "break_by_attack", rContext);
    rules.bDefensiveObligation = RequireBool_(rJson, "defensive_obligation", rContext);
    rules.effects = ParseStatusEffects_(rJson, rContext);
    rules.durationTurns = RequireOptionalPositiveInt_(rJson, "duration_turns", rContext);
    if (rules.durationTurns && !StepDown(status))
    {
        throw std::runtime_error(rContext + ": 'duration_turns' needs a status to expire into");
    }
    if (rules.bDefensiveObligation && !StepDown(status))
    {
        throw std::runtime_error(rContext
                                 + ": 'defensive_obligation' needs a status to step down into "
                                   "when the obligation is declined");
    }
    return rules;
}

BaseValueConfig_t ParseBaseValue_(const nlohmann::json& rJson, const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(
        rJson,
        {"scan_radius", "credits_offset", "distance_offset", "min_population", "minimum",
         "ungarrisoned_divisor", "counterparty_absent_multiplier", "lone_base_multiplier",
         "outnumbered_ratio", "outnumbered_divisor", "minerals_per_row",
         "project_rows_multiplier", "unit_rows_multiplier"},
        rContext);
    BaseValueConfig_t config;
    config.scanRadius = ConfigFields::RequireIntAtLeast(rJson, "scan_radius", 1, rContext);
    if (config.scanRadius > k_TableOffsetsMaxRadius)
    {
        throw std::runtime_error(rContext + ": 'scan_radius' must be at most "
                                 + std::to_string(k_TableOffsetsMaxRadius));
    }
    config.creditsOffset = ConfigFields::RequireIntAtLeast(rJson, "credits_offset", 0, rContext);
    config.distanceOffset =
        ConfigFields::RequireIntAtLeast(rJson, "distance_offset", 1, rContext);
    config.minPopulation = ConfigFields::RequireIntAtLeast(rJson, "min_population", 0, rContext);
    config.minimum = ConfigFields::RequireIntAtLeast(rJson, "minimum", 0, rContext);
    config.ungarrisonedDivisor =
        ConfigFields::RequireIntAtLeast(rJson, "ungarrisoned_divisor", 1, rContext);
    config.counterpartyAbsentMultiplier =
        ConfigFields::RequireIntAtLeast(rJson, "counterparty_absent_multiplier", 1, rContext);
    config.loneBaseMultiplier =
        ConfigFields::RequireIntAtLeast(rJson, "lone_base_multiplier", 1, rContext);
    config.outnumberedRatio =
        ConfigFields::RequireIntAtLeast(rJson, "outnumbered_ratio", 1, rContext);
    config.outnumberedDivisor =
        ConfigFields::RequireIntAtLeast(rJson, "outnumbered_divisor", 1, rContext);
    config.mineralsPerRow =
        ConfigFields::RequireIntAtLeast(rJson, "minerals_per_row", 1, rContext);
    config.projectRowsMultiplier =
        ConfigFields::RequireIntAtLeast(rJson, "project_rows_multiplier", 0, rContext);
    config.unitRowsMultiplier =
        ConfigFields::RequireIntAtLeast(rJson, "unit_rows_multiplier", 0, rContext);
    return config;
}

DefensiveObligationMode_t RequireObligationMode_(const nlohmann::json& rJson,
                                                 const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, "defensive_obligation_mode", rContext);
    if (!rValue.is_string())
    {
        throw std::runtime_error(rContext + ": 'defensive_obligation_mode' must be a string");
    }
    return EnumFromName<DefensiveObligationMode_t>(rValue.get<std::string>(),
                                                   "defensive obligation mode");
}

} // namespace

DiplomacyConfig_t DiplomacyConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open diplomacy config '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    const std::string context = "Diplomacy config '" + configPath + "'";
    if (!json.is_object())
    {
        throw std::runtime_error(context + " must be a JSON object");
    }
    ConfigFields::RejectUnknownKeys(json,
                                    {"statuses", "defensive_obligation_mode", "friction_max",
                                     "blemishes_max", "break_agreement_blemishes",
                                     "probe_keep_friction_draw", "artifact_capture_friction",
                                     "allied_territory_battle_friction",
                                     "terraform_rainfall_friction",
                                     "terraform_rainfall_stop_distance", "base_value"},
                                    context);
    const nlohmann::json& rStatuses = RequireObject_(json, "statuses", context);
    const nlohmann::json& rBaseValue = RequireObject_(json, "base_value", context);

    DiplomacyConfig_t config;
    config.defensiveObligationMode = RequireObligationMode_(json, context);
    config.frictionMax = ConfigFields::RequireIntAtLeast(json, "friction_max", 1, context);
    config.blemishesMax = ConfigFields::RequireIntAtLeast(json, "blemishes_max", 1, context);
    config.breakAgreementBlemishes =
        ConfigFields::RequireIntAtLeast(json, "break_agreement_blemishes", 0, context);
    config.probeKeepFrictionDraw =
        ConfigFields::RequireIntAtLeast(json, "probe_keep_friction_draw", 1, context);
    config.artifactCaptureFriction =
        ConfigFields::RequireIntAtLeast(json, "artifact_capture_friction", 0, context);
    config.alliedTerritoryBattleFriction =
        ConfigFields::RequireInt(json, "allied_territory_battle_friction", context);
    config.terraformRainfallFriction =
        ConfigFields::RequireIntAtLeast(json, "terraform_rainfall_friction", 0, context);
    config.terraformRainfallStopDistance =
        ConfigFields::RequireIntAtLeast(json, "terraform_rainfall_stop_distance", 0, context);
    config.baseValue = ParseBaseValue_(rBaseValue, context + " base_value");
    config.statuses =
        ConfigFields::ParseEnumKeyedObject<DiplomaticStatus_t, DiplomaticStatusRules_t>(
            rStatuses, "diplomatic status", context,
            [](const nlohmann::json& rEntry, DiplomaticStatus_t status, const std::string& rName) {
                return ParseStatus_(rEntry, status, "statuses." + rName);
            });
    return config;
}

} // namespace ac
