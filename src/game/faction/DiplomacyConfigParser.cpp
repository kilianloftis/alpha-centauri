#include "game/faction/DiplomacyConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/DiplomaticTransitionRules.h"
#include "lib/config/EnumNames.h"

#include <algorithm>
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

void RejectUnknownKeys_(const nlohmann::json& rJson, const std::vector<std::string>& rKnown,
                        const std::string& rContext)
{
    for (const auto& [rKey, rUnused] : rJson.items())
    {
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error(rContext + ": unknown key '" + rKey + "'");
        }
    }
}

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

std::vector<EffectConfig_t> ParseStatusEffects_(const nlohmann::json& rJson,
                                                  const std::string& rContext)
{
    std::vector<EffectConfig_t> effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::DiplomaticStatus, rContext);
    for (const EffectConfig_t& rEffect : effects)
    {
        const auto* pModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (!pModifier || pModifier->stat != StatId_t::CommerceRate || rEffect.condition
            || pModifier->amountSource)
        {
            throw std::runtime_error(rContext
                                     + ": effects may only be unconditional StatModifier "
                                       "commerce_rate entries");
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
    RejectUnknownKeys_(rJson, known, rContext);

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
    return rules;
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
    RejectUnknownKeys_(json, {"statuses"}, context);
    const nlohmann::json& rStatuses = Require_(json, "statuses", context);
    if (!rStatuses.is_object())
    {
        throw std::runtime_error(context + ": 'statuses' must be a JSON object");
    }

    DiplomacyConfig_t config;
    std::array<bool, k_DiplomaticStatusCount> seen{};
    for (const auto& [rName, rEntry] : rStatuses.items())
    {
        const DiplomaticStatus_t status = EnumFromName<DiplomaticStatus_t>(rName, "diplomatic status");
        const std::size_t index = static_cast<std::size_t>(status);
        if (seen[index])
        {
            throw std::runtime_error(context + " repeats status '" + rName + "'");
        }
        seen[index] = true;
        config.statuses[index] = ParseStatus_(rEntry, status, "statuses." + rName);
    }
    for (const DiplomaticStatus_t status : magic_enum::enum_values<DiplomaticStatus_t>())
    {
        if (!seen[static_cast<std::size_t>(status)])
        {
            throw std::runtime_error(context + " is missing status '"
                                     + std::string(magic_enum::enum_name(status)) + "'");
        }
    }
    return config;
}

} // namespace ac
