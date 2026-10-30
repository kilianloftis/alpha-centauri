#include "game/diplomacy/DiplomacyConfigParser.h"

#include "lib/config/ConfigFields.h"
#include "lib/config/ConfigSets.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
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
        "defensive_obligation", "effects",     "duration_turns",
    };
    ConfigFields::RejectUnknownKeys(rJson, known, rContext);

    DiplomaticStatusRules_t rules;
    rules.bEnterTerritory = RequireBool_(rJson, "enter_territory", rContext);
    rules.bShareTiles = RequireBool_(rJson, "share_tiles", rContext);
    rules.bRepairAtBases = RequireBool_(rJson, "repair_at_bases", rContext);
    rules.bMayAttack = RequireBool_(rJson, "may_attack", rContext);
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
                                     "probe_keep_friction_draw", "artifact_capture_friction"},
                                    context);
    const nlohmann::json& rStatuses = RequireObject_(json, "statuses", context);

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
    config.statuses =
        ConfigFields::ParseEnumKeyedObject<DiplomaticStatus_t, DiplomaticStatusRules_t>(
            rStatuses, "diplomatic status", context,
            [](const nlohmann::json& rEntry, DiplomaticStatus_t status, const std::string& rName) {
                return ParseStatus_(rEntry, status, "statuses." + rName);
            });
    return config;
}

} // namespace ac
