#pragma once

#include "game/council/CouncilProposalConfig.h"
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace ac
{

class CouncilProposalConfigParser
{
public:
    std::vector<CouncilProposalConfig_t> ParseConfig(const std::string& configPath);

private:
    CouncilProposalConfig_t ParseProposalConfig_(const nlohmann::json& proposalJson);
    CouncilVoteWeight_t ParseVoteWeight_(const std::string& rValue) const;
    CouncilProposalKind_t ParseKind_(const std::string& rValue) const;
    CouncilElectionOutcome_t ParseElectionOutcome_(const std::string& rValue) const;
};

} // namespace ac
