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
    CouncilProposalConfig_t ParseProposalConfig(const nlohmann::json& proposalJson);
    CouncilVoteWeight_t ParseVoteWeight(const std::string& rValue) const;
    CouncilProposalKind_t ParseKind(const std::string& rValue) const;
    CouncilElectionOutcome_t ParseElectionOutcome(const std::string& rValue) const;
};

} // namespace ac
