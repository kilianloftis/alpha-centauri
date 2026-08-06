#include "game/council/CouncilProposalConfigParser.h"

#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <variant>

namespace ac
{

namespace
{

std::vector<RuleFlagId_t> ParseRuleFlagList_(const nlohmann::json& rJson, const char* key)
{
    std::vector<RuleFlagId_t> flags;
    if (!rJson.contains(key))
    {
        return flags;
    }
    const nlohmann::json& rArr = rJson.at(key);
    if (!rArr.is_array())
    {
        throw std::runtime_error(std::string("Council proposal field '") + key
                                 + "' must be an array of flag ids");
    }
    flags.reserve(rArr.size());
    for (const auto& rEntry : rArr)
    {
        flags.push_back(ParseRuleFlagId(rEntry.get<std::string>()));
    }
    return flags;
}

// Honored proposal shapes (runtime consumers): Continuous+WorldGlobal (CouncilEffects world
// store); Instantaneous+GrantEnergy (applier); Instantaneous+WorldParameter+WorldGlobal
// (deferred no-op until WorldEvents — still loadable). Everything else would pass a vote
// and do nothing.
void ValidateProposalEffectHonored_(const EffectConfig_t& rEffect, const std::string& rProposalId)
{
    if (rEffect.persistence == EffectPersistence_t::Continuous
        && rEffect.scope == EffectScope_t::WorldGlobal)
    {
        return;
    }
    if (rEffect.persistence == EffectPersistence_t::Instantaneous
        && std::holds_alternative<GrantEnergyEffect_t>(rEffect.effect))
    {
        return;
    }
    if (rEffect.persistence == EffectPersistence_t::Instantaneous
        && std::holds_alternative<WorldParameterEffect_t>(rEffect.effect)
        && rEffect.scope == EffectScope_t::WorldGlobal)
    {
        return;
    }
    throw std::runtime_error(
        "Council proposal '" + rProposalId
        + "' has an effect shape that is not honored by the council runtime "
          "(allowed: Continuous+WorldGlobal; Instantaneous+GrantEnergy; "
          "Instantaneous+WorldParameter+WorldGlobal)");
}

} // namespace

std::vector<CouncilProposalConfig_t> CouncilProposalConfigParser::ParseConfig(
    const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<CouncilProposalConfig_t>(
        configPath, "council proposal",
        [this](const nlohmann::json& rJson) { return ParseProposalConfig(rJson); });
}

CouncilProposalConfig_t CouncilProposalConfigParser::ParseProposalConfig(
    const nlohmann::json& proposalJson)
{
    CouncilProposalConfig_t config;
    config.id = ConfigFields::ParseId(proposalJson);
    config.name = ConfigFields::ParseName(proposalJson, config.id);
    config.description = proposalJson.value("description", std::string());
    config.kind = ParseKind(proposalJson.value("kind", "standard"));
    config.voteWeight = ParseVoteWeight(proposalJson.value("vote_weight", "representative"));
    config.voteThreshold = proposalJson.value("vote_threshold", 0.0);
    if (config.voteThreshold < 0.0 || config.voteThreshold > 1.0)
    {
        throw std::runtime_error("Council proposal '" + config.id
                                 + "' has vote_threshold outside [0, 1]");
    }
    config.repeatable = proposalJson.value("repeatable", false);
    config.initiallyActive = proposalJson.value("initially_active", false);
    config.proposable = proposalJson.value("proposable", true);
    config.requiredTech = ConfigFields::ParseRequiredTech(proposalJson);
    config.requiredProposals =
        proposalJson.value("required_proposals", std::vector<std::string>{});
    config.repeals = proposalJson.value("repeals", std::vector<std::string>{});
    config.requiresRuleFlags = ParseRuleFlagList_(proposalJson, "requires_rule_flags");
    config.forbidsRuleFlags = ParseRuleFlagList_(proposalJson, "forbids_rule_flags");
    if (proposalJson.contains("election_outcome"))
    {
        config.electionOutcome =
            ParseElectionOutcome(proposalJson.at("election_outcome").get<std::string>());
    }
    config.effects = EffectConfigParser::ParseEffects(
        proposalJson, EffectSourceKind_t::CouncilProposal, config.id);
    for (const EffectConfig_t& rEffect : config.effects)
    {
        ValidateProposalEffectHonored_(rEffect, config.id);
    }
    return config;
}

CouncilVoteWeight_t CouncilProposalConfigParser::ParseVoteWeight(const std::string& rValue) const
{
    if (rValue == "representative") return CouncilVoteWeight_t::Representative;
    if (rValue == "population")     return CouncilVoteWeight_t::Population;
    throw std::runtime_error("Unknown council vote_weight: '" + rValue + "'");
}

CouncilProposalKind_t CouncilProposalConfigParser::ParseKind(const std::string& rValue) const
{
    if (rValue == "standard") return CouncilProposalKind_t::Standard;
    if (rValue == "election") return CouncilProposalKind_t::Election;
    throw std::runtime_error("Unknown council proposal kind: '" + rValue + "'");
}

CouncilElectionOutcome_t CouncilProposalConfigParser::ParseElectionOutcome(
    const std::string& rValue) const
{
    if (rValue == "none")                 return CouncilElectionOutcome_t::None;
    if (rValue == "planetary_governor")   return CouncilElectionOutcome_t::PlanetaryGovernor;
    if (rValue == "supreme_leader")       return CouncilElectionOutcome_t::SupremeLeaderVictory;
    throw std::runtime_error("Unknown council election_outcome: '" + rValue + "'");
}

} // namespace ac
