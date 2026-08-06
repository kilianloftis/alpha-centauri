#pragma once

#include "game/effects/EffectConfig.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ac
{

enum class CouncilVoteWeight_t
{
    Representative, // one vote per council member (modified by CouncilVotes effects)
    Population,     // votes seeded from total population (modified by CouncilVotes effects)
};

enum class CouncilProposalKind_t
{
    Standard, // Yea / Nay / Abstain
    Election, // vote for a candidate faction, or abstain
};

// What happens when an election proposal passes (beyond applying effects).
enum class CouncilElectionOutcome_t
{
    None,
    PlanetaryGovernor,
    SupremeLeaderVictory, // stub: marks diplomatic victory pending
};

struct CouncilProposalConfig_t
{
    std::string id;
    std::string name;
    std::string description;
    CouncilProposalKind_t kind = CouncilProposalKind_t::Standard;
    CouncilVoteWeight_t voteWeight = CouncilVoteWeight_t::Representative;
    // Fraction of total vote weight required for passage (elections / specials).
    // 0 = simple majority of non-abstaining weight (Yea > Nay).
    double voteThreshold = 0.0;
    bool repeatable = false;
    // When true, PlanetaryCouncil activates this proposal (and its continuous effects) at
    // construction — used for standing world law such as the U.N. Charter.
    bool initiallyActive = false;
    // When false, the proposal can never be put on the agenda (seed / background entries).
    bool proposable = true;
    std::string requiredTech; // empty if none; checked on the proposing faction
    // Proposal ids that must currently be in force (active) to propose this.
    std::vector<std::string> requiredProposals;
    // When this proposal passes, these proposal ids are removed from the active set.
    std::vector<std::string> repeals;
    // Active council WorldGlobal RuleFlags that must / must not be in force.
    std::vector<RuleFlagId_t> requiresRuleFlags;
    std::vector<RuleFlagId_t> forbidsRuleFlags;
    CouncilElectionOutcome_t electionOutcome = CouncilElectionOutcome_t::None;
    std::vector<EffectConfig_t> effects;

    bool IsAvailable(const std::vector<std::string>& rDiscoveredTechIds) const
    {
        if (requiredTech.empty())
        {
            return true;
        }
        return std::find(rDiscoveredTechIds.begin(), rDiscoveredTechIds.end(), requiredTech)
               != rDiscoveredTechIds.end();
    }
};

} // namespace ac
