#include "game/council/CouncilRulesConfigParser.h"

#include "game/effects/BonusEffectParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

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
            wrapper, EffectSourceKind_t::CouncilProposal, "council_governor");
    }

    return config;
}

} // namespace ac
