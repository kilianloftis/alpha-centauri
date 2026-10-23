#include "game/council/CouncilRulesConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"

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

// CouncilEffects::SetGovernorEffects only reads FactionGlobal entries, so anything else here
// would be silently inert. One-shot governor perks live in on_elected_effects.
void ValidateGovernorEffectHonored_(const EffectConfig_t& rEffect, std::size_t index,
                                    const std::string& rConfigPath)
{
    if (rEffect.scope == EffectScope_t::FactionGlobal)
    {
        return;
    }
    throw std::runtime_error(
        "governor_effects[" + std::to_string(index) + "] in '" + rConfigPath
        + "' is not honored by the council runtime (a continuous governor effect must be "
          "FactionGlobal; a one-shot perk belongs in 'on_elected_effects')");
}

std::vector<EffectConfig_t> ParseGovernorEffects_(const nlohmann::json& rEffects,
                                                  const std::string& rConfigPath)
{
    // ParseEffects expects a container with an "effects" array; wrap the field.
    nlohmann::json wrapper = nlohmann::json::object();
    wrapper["effects"] = rEffects;
    std::vector<EffectConfig_t> effects = EffectConfigParser::ParseEffects(
        wrapper, EffectSourceKind_t::CouncilRules, "council_governor");
    for (std::size_t index = 0; index < effects.size(); ++index)
    {
        ValidateGovernorEffectHonored_(effects[index], index, rConfigPath);
    }
    return effects;
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
        config.governorEffects = ParseGovernorEffects_(json.at("governor_effects"), configPath);
    }
    config.onElectedEffects = TriggeredEffectParser::ParseTriggeredEffects(
        json, "on_elected_effects", "council_governor");

    return config;
}

} // namespace ac
