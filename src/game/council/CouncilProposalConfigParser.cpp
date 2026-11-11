#include "game/council/CouncilProposalConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include <variant>

namespace ac
{

namespace
{

std::vector<RuleFlagId_t> ParseRuleFlagList_(const ConfigReader& rReader, const char* pKey)
{
    std::vector<RuleFlagId_t> flags;
    for (const std::string& rFlag : rReader.StringArray(pKey))
    {
        flags.push_back(ParseRuleFlagId(rFlag));
    }
    return flags;
}

CouncilVoteWeight_t ParseVoteWeight_(const ConfigReader& rReader)
{
    const std::string value = rReader.StringOr("vote_weight", "representative");
    if (value == "representative") return CouncilVoteWeight_t::Representative;
    if (value == "population")     return CouncilVoteWeight_t::Population;
    rReader.FailKey("vote_weight", "is not a known vote weight: '" + value + "'");
}

CouncilProposalKind_t ParseKind_(const ConfigReader& rReader)
{
    const std::string value = rReader.StringOr("kind", "standard");
    if (value == "standard") return CouncilProposalKind_t::Standard;
    if (value == "election") return CouncilProposalKind_t::Election;
    rReader.FailKey("kind", "is not a known proposal kind: '" + value + "'");
}

CouncilElectionOutcome_t ParseElectionOutcome_(const ConfigReader& rReader)
{
    const std::string value = rReader.String("election_outcome");
    if (value == "none")                 return CouncilElectionOutcome_t::None;
    if (value == "planetary_governor")   return CouncilElectionOutcome_t::PlanetaryGovernor;
    if (value == "supreme_leader")       return CouncilElectionOutcome_t::SupremeLeaderVictory;
    rReader.FailKey("election_outcome", "is not a known election outcome: '" + value + "'");
}

// Standing laws are the only continuous shape the council reads; one-shot outcomes belong in
// on_passed_effects.
void ValidateProposalEffectHonored_(const EffectConfig_t& rEffect, const ConfigReader& rReader)
{
    if (rEffect.scope == EffectScope_t::WorldGlobal)
    {
        return;
    }
    rReader.Fail("has an 'effects' entry that is not honored by the council runtime "
                 "(a continuous proposal effect must be WorldGlobal; a one-shot outcome belongs "
                 "in 'on_passed_effects')");
}

// Honored one-shot outcomes: GrantEnergy (credited to every member) and WorldParameter
// (a deferred no-op until WorldEvents — still loadable).
void ValidatePassedEffectHonored_(const TriggeredEffectConfig_t& rEffect,
                                  const ConfigReader& rReader)
{
    if (std::holds_alternative<GrantEnergyEffect_t>(rEffect.effect)
        || std::holds_alternative<WorldParameterEffect_t>(rEffect.effect))
    {
        return;
    }
    rReader.Fail("has an 'on_passed_effects' entry that is not honored by the council runtime "
                 "(allowed: GrantEnergy, WorldParameter)");
}

CouncilProposalConfig_t ParseProposal_(const ConfigReader& rReader)
{
    CouncilProposalConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.description = rReader.StringOr("description", "");
    config.kind = ParseKind_(rReader);
    config.voteWeight = ParseVoteWeight_(rReader);
    config.voteThreshold =
        rReader.Has("vote_threshold") ? rReader.NumberInRange("vote_threshold", 0.0, 1.0) : 0.0;
    config.repeatable = rReader.BoolOr("repeatable", false);
    config.initiallyActive = rReader.BoolOr("initially_active", false);
    config.proposable = rReader.BoolOr("proposable", true);
    config.requiredTech = rReader.StringOr("required_tech", "");
    config.requiredProposals = rReader.StringArray("required_proposals");
    config.repeals = rReader.StringArray("repeals");
    config.requiresRuleFlags = ParseRuleFlagList_(rReader, "requires_rule_flags");
    config.forbidsRuleFlags = ParseRuleFlagList_(rReader, "forbids_rule_flags");
    if (rReader.Has("election_outcome"))
    {
        config.electionOutcome = ParseElectionOutcome_(rReader);
    }
    config.effects = EffectConfigParser::ParseEffects(
        rReader.Json(), EffectSourceKind_t::CouncilProposal, config.id);
    for (const EffectConfig_t& rEffect : config.effects)
    {
        ValidateProposalEffectHonored_(rEffect, rReader);
    }
    config.onPassedEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_passed_effects", config.id);
    for (const TriggeredEffectConfig_t& rEffect : config.onPassedEffects)
    {
        ValidatePassedEffectHonored_(rEffect, rReader);
    }
    return config;
}

} // namespace

std::vector<CouncilProposalConfig_t> CouncilProposalConfigParser::ParseConfig(
    const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<CouncilProposalConfig_t>(configPath, "council proposal",
                                                               ParseProposal_);
}

} // namespace ac
