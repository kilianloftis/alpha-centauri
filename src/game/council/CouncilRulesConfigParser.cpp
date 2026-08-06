#include "game/council/CouncilRulesConfigParser.h"

#include "game/effects/BonusEffectParser.h"

#include <cstddef>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>

namespace ac
{

namespace
{

// Honored governor shapes: Continuous+FactionGlobal (CouncilEffects::SetGovernorEffects);
// Instantaneous+Infiltration (ApplyGovernor). Infiltration scopes are already enforced at parse.
void ValidateGovernorEffectHonored_(const EffectConfig_t& rEffect, std::size_t index,
                                    const std::string& rConfigPath)
{
    if (rEffect.persistence == EffectPersistence_t::Continuous
        && rEffect.scope == EffectScope_t::FactionGlobal)
    {
        return;
    }
    if (rEffect.persistence == EffectPersistence_t::Instantaneous
        && std::holds_alternative<InfiltrationEffect_t>(rEffect.effect))
    {
        return;
    }
    throw std::runtime_error(
        "governor_effects[" + std::to_string(index) + "] in '" + rConfigPath
        + "' has a shape that is not honored by the council runtime "
          "(allowed: Continuous+FactionGlobal; Instantaneous+Infiltration)");
}

} // namespace

CouncilRulesConfig_t CouncilRulesConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open council rules config '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    if (!json.is_object())
    {
        throw std::runtime_error("Council rules config '" + configPath
                                 + "' must be a JSON object");
    }

    CouncilRulesConfig_t config;
    config.governorProposeIntervalYears =
        json.value("governor_propose_interval_years", config.governorProposeIntervalYears);
    config.memberProposeIntervalYears =
        json.value("member_propose_interval_years", config.memberProposeIntervalYears);

    if (config.governorProposeIntervalYears <= 0)
    {
        throw std::runtime_error(
            "Council rules governor_propose_interval_years must be > 0");
    }
    if (config.memberProposeIntervalYears <= 0)
    {
        throw std::runtime_error(
            "Council rules member_propose_interval_years must be > 0");
    }

    if (json.contains("governor_effects"))
    {
        // ParseEffects expects a container with an "effects" array; wrap the field.
        nlohmann::json wrapper = nlohmann::json::object();
        wrapper["effects"] = json.at("governor_effects");
        config.governorEffects = BonusEffectParser::ParseEffects(
            wrapper, EffectSourceKind_t::CouncilRules, "council_governor");
        for (std::size_t index = 0; index < config.governorEffects.size(); ++index)
        {
            ValidateGovernorEffectHonored_(config.governorEffects[index], index, configPath);
        }
    }

    return config;
}

} // namespace ac
