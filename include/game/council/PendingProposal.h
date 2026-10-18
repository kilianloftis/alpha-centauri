#pragma once

#include "game/base/BaseTypes.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

enum class CouncilBallot_t
{
    Yea,
    Nay,
    Abstain,
};

// The vote the council is considering.
struct PendingProposal_t
{
    std::string proposalId;
    FactionId_t proposerId = 0;
    bool vetoed = false;
    // Standard ballots; election votes use electionVotes instead.
    std::map<FactionId_t, CouncilBallot_t> ballots;
    std::map<FactionId_t, std::optional<FactionId_t>> electionVotes;
    // Who may be voted for, fixed when the vote opened (empty for a standard ballot). Eligibility
    // derives from live population, so a mid-vote recount would strand ballots already cast.
    std::vector<FactionId_t> eligibleCandidateIds;
};

} // namespace ac
